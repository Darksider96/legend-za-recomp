#pragma once
#include "gfx/camera3d.hpp"
#include "gfx/scene_assembly.hpp"
#include "gfx/trscn.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/trcol.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace za::gfx {

struct LoadedModelInstance {
    Model model;
    Vec3 position{0.0f, 0.0f, 0.0f};
    float scale{1.0f};
    float rot_y{0.0f};
    std::string path;
};

struct LoadedSceneResult {
    size_t models_loaded{0};
    size_t collisions_loaded{0};
    size_t total_collision_triangles{0};
    std::vector<std::string> model_paths;
    std::vector<std::string> collision_paths;
    std::vector<LoadedModelInstance> models;
    std::vector<CollisionMesh> collisions;
};

using VfsReaderFn = std::function<bool(const std::string& path, std::vector<uint8_t>& out_bytes)>;

class SceneLoader {
public:
    static SceneLoader& Instance();

    // Carrega um arquivo de cena .trscn recursivamente a partir do VFS,
    // preenchendo out_result e opcionalmente populando um CollisionWorld destino
    bool LoadScene(const std::string& scene_path,
                   VfsReaderFn vfs_reader,
                   LoadedSceneResult& out_result,
                   std::string& error,
                   CollisionWorld* target_world = nullptr);

    // Carrega uma lista de setores de colisão .trcol para um CollisionWorld
    bool LoadCollisionSectors(const std::vector<std::string>& trcol_paths,
                              VfsReaderFn vfs_reader,
                              CollisionWorld& target_world,
                              size_t& out_loaded,
                              std::string& error);

    // Carrega um modelo .trmdl avulso completo (com meshes e buffers)
    bool LoadModelFile(const std::string& trmdl_path,
                       VfsReaderFn vfs_reader,
                       Model& out_model,
                       std::string& error);

    // Carrega uma malha de colisão .trcol avulsa
    bool LoadCollisionFile(const std::string& trcol_path,
                           VfsReaderFn vfs_reader,
                           CollisionMesh& out_mesh,
                           std::string& error);
};

} // namespace za::gfx
