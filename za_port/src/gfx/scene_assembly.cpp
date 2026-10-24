#include "gfx/scene_assembly.hpp"

#include "vfs/asset_export.hpp"

namespace za::gfx {

namespace {

bool IsAuxiliaryCollision(std::string_view path) {
    return path.ends_with("_hicol_col.trcol") || path.ends_with("_allchrcol_col.trcol");
}

std::string Directory(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

} // namespace

SceneAssembler::SceneAssembler(Reader reader, SceneAssemblyOptions options)
    : reader_(std::move(reader)), options_(std::move(options)) {}

bool SceneAssembler::AddScene(std::string_view scene_path, std::string& error) {
    return AddScene(scene_path, kIdentityTransform, error);
}

bool SceneAssembler::AddScene(std::string_view scene_path, const std::array<float, 16>& parent, std::string& error) {
    if (!reader_) {
        error = "SceneAssembler sem leitor de arquivos";
        return false;
    }
    return LoadScene(vfs::NormalizePath(scene_path), parent, 0, error);
}

std::string SceneAssembler::Resolve(const std::string& scene, const std::string& reference) const {
    return reference.starts_with(".") ? vfs::ResolveRelativePath(Directory(scene), reference) : vfs::NormalizePath(reference);
}

bool SceneAssembler::LoadScene(const std::string& path, const std::array<float, 16>& parent, unsigned depth, std::string& error) {
    if (depth > options_.max_depth) {
        error = "hierarquia de cenas profunda demais em " + path;
        return false;
    }
    // "x.trscn" e o nome de referencia; o arquivo guardado e o bloco "x_0.trscn"
    const std::string chunk = SceneChunkPath(path);
    if (visited_.contains(path) || visited_.contains(chunk)) return true;
    visited_.insert(path);
    if (options_.scene_filter && !options_.scene_filter(path)) return true;

    std::vector<uint8_t> data;
    std::string loaded = path;
    if (!reader_(path, data)) {
        if (chunk == path || !reader_(chunk, data)) {
            missing_.push_back(path);
            error = "cena ausente: " + path;
            return false;
        }
        loaded = chunk;
    }
    visited_.insert(loaded);

    Scene scene;
    if (!ParseTrscn(data, scene, error)) {
        missing_.push_back(loaded);
        error = loaded + ": " + error;
        return false;
    }
    scenes_.push_back(loaded);
    for (const SceneEntity& entity : scene.entities) VisitEntity(entity, parent, loaded, scene.name, depth);

    // Sub-cenas listadas na raiz sem entidade SubScene correspondente
    std::string ignored;
    for (const std::string& sub : scene.sub_scenes) LoadScene(Resolve(loaded, sub), parent, depth + 1, ignored);
    return true;
}

void SceneAssembler::VisitEntity(const SceneEntity& entity, const std::array<float, 16>& parent, const std::string& scene,
                                 const std::string& node, unsigned depth) {
    ++entities_;
    const bool is_collision = entity.type == "trinity_CollisionComponent";
    // Componentes de colisao tem SRT proprio (relativo ao objeto) que nao se propaga aos irmaos
    const std::array<float, 16> world = entity.has_transform && !is_collision
                                            ? MultiplyTransforms(parent, ComposeTransform(entity.transform))
                                            : parent;
    const std::string& name = entity.name.empty() ? node : entity.name;

    if (entity.type == "SubScene") {
        std::string ignored;
        for (const std::string& asset : entity.assets) {
            if (asset.ends_with(".trscn")) LoadScene(Resolve(scene, asset), world, depth + 1, ignored);
        }
    } else if (entity.type == "trinity_ModelComponent") {
        for (const std::string& asset : entity.assets) {
            if (asset.ends_with(".trmdl")) models_.push_back({vfs::NormalizePath(asset), world, scene, name});
        }
    } else if (is_collision) {
        bool has_mesh = false;
        const std::array<float, 16> shape_world = entity.has_transform ? MultiplyTransforms(parent, ComposeTransform(entity.transform)) : parent;
        for (const std::string& asset : entity.assets) {
            if (!asset.ends_with(".trcol")) continue;
            has_mesh = true;
            if (options_.auxiliary_collision || !IsAuxiliaryCollision(asset)) {
                collisions_.push_back({vfs::NormalizePath(asset), shape_world, scene, name});
            }
        }
        boxes_ += has_mesh ? 0 : 1;
    }
    for (const SceneEntity& child : entity.children) VisitEntity(child, world, scene, name, depth);
}

size_t SceneAssembler::BuildCollisionWorld(CollisionWorld& world, std::string& error, const std::array<float, 3>& origin) const {
    std::unordered_map<std::string, CollisionMesh> cache;
    std::array<float, 16> recenter = kIdentityTransform;
    recenter[12] = -origin[0];
    recenter[13] = -origin[1];
    recenter[14] = -origin[2];
    size_t added = 0;
    for (const ScenePlacement& placement : collisions_) {
        auto it = cache.find(placement.path);
        if (it == cache.end()) {
            std::vector<uint8_t> data;
            CollisionMesh mesh;
            std::string parse_error;
            if (!reader_(placement.path, data) || !ParseTrcol(data, mesh, parse_error)) {
                error += (error.empty() ? "" : "; ") + placement.path + (parse_error.empty() ? ": ausente" : ": " + parse_error);
                continue;
            }
            it = cache.emplace(placement.path, std::move(mesh)).first;
        }
        if (it->second.Empty()) continue;
        CollisionMesh mesh = it->second;
        TransformMesh(mesh, MultiplyTransforms(recenter, placement.world));
        world.Add(std::move(mesh));
        ++added;
    }
    return added;
}

} // namespace za::gfx
