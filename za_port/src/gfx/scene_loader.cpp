#include "gfx/scene_loader.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>

namespace za::gfx {

SceneLoader& SceneLoader::Instance() {
    static SceneLoader instance;
    return instance;
}

static std::string GetBaseDirectory(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash == std::string::npos) return "";
    return path.substr(0, last_slash + 1);
}

bool SceneLoader::LoadModelFile(const std::string& trmdl_path,
                                VfsReaderFn vfs_reader,
                                Model& out_model,
                                std::string& error) {
    if (!vfs_reader) {
        error = "VfsReader invalido";
        return false;
    }

    std::vector<uint8_t> data;
    if (!vfs_reader(trmdl_path, data)) {
        error = "Nao foi possivel abrir o arquivo .trmdl: " + trmdl_path;
        return false;
    }

    TrmdlFile tf;
    if (!ParseTrmdl(data, tf, error)) {
        return false;
    }

    if (tf.mesh_files.empty()) {
        error = "Nenhum arquivo .trmsh referenciado no .trmdl";
        return false;
    }

    std::string base_dir = GetBaseDirectory(trmdl_path);
    std::string mesh_path = base_dir + tf.mesh_files.front();

    if (!vfs_reader(mesh_path, data)) {
        error = "Nao foi possivel abrir o arquivo .trmsh: " + mesh_path;
        return false;
    }

    std::string buffer_file;
    if (!ParseTrmsh(data, out_model.meshes, buffer_file, error)) {
        return false;
    }

    std::string buf_path = base_dir + buffer_file;
    if (!vfs_reader(buf_path, data)) {
        error = "Nao foi possivel abrir o arquivo de buffer .trmbf: " + buf_path;
        return false;
    }

    if (!LoadTrmbf(data, out_model.meshes, error)) {
        return false;
    }

    out_model.name = trmdl_path;

    // Carregamento opcional de esqueleto para modelos articulados
    if (!tf.skeleton_file.empty()) {
        std::string skel_path = base_dir + tf.skeleton_file;
        if (vfs_reader(skel_path, data)) {
            std::string skel_err;
            if (ParseTrskl(data, out_model.skeleton, skel_err)) {
                UpdateSkeletonMatrices(out_model.skeleton);
            }
        }
    }

    return true;
}

bool SceneLoader::LoadCollisionFile(const std::string& trcol_path,
                                    VfsReaderFn vfs_reader,
                                    CollisionMesh& out_mesh,
                                    std::string& error) {
    if (!vfs_reader) {
        error = "VfsReader invalido";
        return false;
    }

    std::vector<uint8_t> data;
    if (!vfs_reader(trcol_path, data)) {
        error = "Nao foi possivel abrir o arquivo .trcol: " + trcol_path;
        return false;
    }

    if (!ParseTrcol(data, out_mesh, error)) {
        return false;
    }

    BuildGroundGrid(out_mesh);
    return true;
}

bool SceneLoader::LoadCollisionSectors(const std::vector<std::string>& trcol_paths,
                                       VfsReaderFn vfs_reader,
                                       CollisionWorld& target_world,
                                       size_t& out_loaded,
                                       std::string& error) {
    out_loaded = 0;
    for (const auto& path : trcol_paths) {
        CollisionMesh mesh;
        std::string err;
        if (LoadCollisionFile(path, vfs_reader, mesh, err)) {
            target_world.Add(std::move(mesh));
            out_loaded++;
        }
    }
    if (out_loaded == 0 && !trcol_paths.empty()) {
        error = "Falha ao carregar setores de colisao solicitados";
    }
    return out_loaded > 0;
}

bool SceneLoader::LoadScene(const std::string& scene_path,
                            VfsReaderFn vfs_reader,
                            LoadedSceneResult& out_result,
                            std::string& error,
                            CollisionWorld* target_world) {
    out_result = {};
    if (!vfs_reader) {
        error = "VfsReader invalido";
        return false;
    }

    SceneAssembler assembler(vfs_reader);
    if (!assembler.AddScene(scene_path, error)) {
        return false;
    }

    // Carrega modelos visuais posicionados pela hierarquia de mundo composta
    for (const auto& placement : assembler.Models()) {
        Model model;
        std::string err;
        if (LoadModelFile(placement.path, vfs_reader, model, err)) {
            Vec3 pos{placement.world[12], placement.world[13], placement.world[14]};
            float scale = std::sqrt(placement.world[0]*placement.world[0] + placement.world[1]*placement.world[1] + placement.world[2]*placement.world[2]);
            if (scale < 1e-4f) scale = 1.0f;
            float rot_y = std::atan2(-placement.world[8], placement.world[0]);

            out_result.models_loaded++;
            out_result.model_paths.push_back(placement.path);
            out_result.models.push_back(LoadedModelInstance{
                std::move(model), pos, scale, rot_y, placement.path
            });
        }
    }

    // Carrega e posiciona malhas de colisão Havok com transformações compostas
    if (target_world) {
        size_t added = assembler.BuildCollisionWorld(*target_world, error);
        out_result.collisions_loaded = added;
        for (const auto& placement : assembler.Collisions()) {
            out_result.collision_paths.push_back(placement.path);
        }
    } else {
        CollisionWorld temp_world;
        size_t added = assembler.BuildCollisionWorld(temp_world, error);
        out_result.collisions_loaded = added;
        for (const auto& placement : assembler.Collisions()) {
            out_result.collision_paths.push_back(placement.path);
        }
        for (size_t i = 0; i < temp_world.MeshCount(); ++i) {
            out_result.total_collision_triangles += temp_world.Mesh(i).triangles.size();
            out_result.collisions.push_back(temp_world.Mesh(i));
        }
    }

    return true;
}

} // namespace za::gfx
