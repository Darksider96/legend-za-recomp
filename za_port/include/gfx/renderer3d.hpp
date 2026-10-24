#pragma once
#include "gfx/camera3d.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/trcol.hpp"
#include <cstdint>
#include <vector>
#include <optional>
#include <memory>

namespace za::game {
class PokeActor;
}

namespace za::gfx {

struct AnimationClip;

struct SceneryInstance {
    Model model;
    Vec3 position{0.0f, 0.0f, 0.0f};
    float scale{1.0f};
    float rot_y{0.0f};
    uint32_t base_color{0x8090A0};
};

class Renderer3D {
public:
    static Renderer3D& Instance();

    void Initialize(int width, int height);
    void Render(uint32_t* pixel_buffer, int width, int height, const Camera3D& camera, bool is_aiming, bool is_throwing);

    void SetActiveModel(const Model& model, const std::shared_ptr<AnimationClip>& anim = nullptr);
    bool HasActiveModel() const { return active_model_.has_value() || active_actor_ != nullptr; }
    const Model* GetActiveModel() const;

    void SetActiveActor(std::shared_ptr<game::PokeActor> actor);
    std::shared_ptr<game::PokeActor> GetActiveActor() const { return active_actor_; }

    void SetOpponentActor(std::shared_ptr<game::PokeActor> opponent) { opponent_actor_ = opponent; }
    std::shared_ptr<game::PokeActor> GetOpponentActor() const { return opponent_actor_; }

    void SetSceneryModel(const Model& model, const Vec3& pos = Vec3{0.0f, 0.0f, 15.0f}, float scale = 0.08f, float rot_y = 0.0f, uint32_t color = 0x8898B0);
    void ClearScenery() { scenery_.clear(); }

    void SetWireframeOnly(bool wireframe) { wireframe_only_ = wireframe; }
    bool IsWireframeOnly() const { return wireframe_only_; }
    void ToggleWireframe() { wireframe_only_ = !wireframe_only_; }

    Camera3D& GetCamera() { return camera_; }
    const Camera3D& GetCamera() const { return camera_; }

    void SetActiveSpecies(uint16_t species_id) { active_species_id_ = species_id; }
    uint16_t GetActiveSpecies() const { return active_species_id_; }

    CollisionWorld& GetCollisionWorld() { return collision_world_; }
    const CollisionWorld& GetCollisionWorld() const { return collision_world_; }

private:
    Renderer3D();

    void DrawLine2D(uint32_t* buf, int w, int h, int x0, int y0, int x1, int y1, uint32_t color);
    void DrawLine3D(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& a, const Vec3& b, uint32_t color);
    void DrawTriangleShaded(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& v0, const Vec3& v1, const Vec3& v2, uint32_t base_color);
    void DrawGroundGrid(uint32_t* buf, int w, int h, const Mat4& vp, const Camera3D& cam);
    void DrawAxes(uint32_t* buf, int w, int h, const Mat4& vp);
    void Draw3DCube(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& center, float size, float rot_angle);
    void DrawModel(uint32_t* buf, int w, int h, const Mat4& vp, const Model& model, const Vec3& pos, float scale, float rot_y, uint32_t override_col = 0);
    void DrawCrosshair(uint32_t* buf, int w, int h, bool is_aiming, bool is_throwing);

    Camera3D camera_;
    std::shared_ptr<game::PokeActor> active_actor_;
    std::shared_ptr<game::PokeActor> opponent_actor_;
    std::optional<Model> active_model_;
    std::shared_ptr<AnimationClip> active_anim_;
    std::vector<SceneryInstance> scenery_;
    CollisionWorld collision_world_;
    uint16_t active_species_id_{150};
    std::vector<float> z_buffer_;
    bool wireframe_only_{false};
    float anim_timer_{0.0f};
    int width_{1280};
    int height_{720};
};

} // namespace za::gfx
