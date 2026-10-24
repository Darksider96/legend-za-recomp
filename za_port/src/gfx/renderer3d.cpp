#include "gfx/renderer3d.hpp"
#include "gfx/tranm.hpp"
#include "game/poke_actor.hpp"
#include <cmath>
#include <algorithm>

namespace za::gfx {

Renderer3D& Renderer3D::Instance() {
    static Renderer3D instance;
    return instance;
}

Renderer3D::Renderer3D() {
    camera_.position = Vec3{0.0f, 2.2f, -7.0f};
    camera_.yaw = 0.0f;
    camera_.pitch = -0.10f; // Olhando levemente para baixo
}

void Renderer3D::Initialize(int width, int height) {
    width_ = width;
    height_ = height;
    z_buffer_.assign(static_cast<size_t>(width * height), 1e9f);
}

inline uint32_t BlendColor(uint32_t c0, uint32_t c1, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    uint32_t r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;

    uint32_t r = static_cast<uint32_t>(r0 + (r1 - r0) * t);
    uint32_t g = static_cast<uint32_t>(g0 + (g1 - g0) * t);
    uint32_t b = static_cast<uint32_t>(b0 + (b1 - b0) * t);
    return (r << 16) | (g << 8) | b;
}

void Renderer3D::DrawLine2D(uint32_t* buf, int w, int h, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            buf[y0 * w + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void Renderer3D::DrawLine3D(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& a, const Vec3& b, uint32_t color) {
    float sx0, sy0, sz0;
    float sx1, sy1, sz1;

    bool p0 = vp.ProjectPoint(a, w, h, sx0, sy0, sz0);
    bool p1 = vp.ProjectPoint(b, w, h, sx1, sy1, sz1);

    if (p0 && p1) {
        // Atenuação por distância (fog)
        float avg_z = (sz0 + sz1) * 0.5f;
        float fog = std::clamp(1.0f - (avg_z - 0.1f) / 60.0f, 0.15f, 1.0f);
        uint32_t fogged_color = BlendColor(0x0E0F17, color, fog);

        DrawLine2D(buf, w, h, static_cast<int>(sx0), static_cast<int>(sy0),
                              static_cast<int>(sx1), static_cast<int>(sy1), fogged_color);
    }
}

void Renderer3D::DrawGroundGrid(uint32_t* buf, int w, int h, const Mat4& vp, const Camera3D& cam) {
    constexpr float kExtent = 24.0f;
    constexpr float kStep = 2.0f;

    // Alinhamento suave com a posição da câmera
    float base_x = std::floor(cam.position.x / kStep) * kStep;
    float base_z = std::floor(cam.position.z / kStep) * kStep;

    // Linhas paralelas a Z
    for (float x = -kExtent; x <= kExtent; x += kStep) {
        float world_x = base_x + x;
        uint32_t col = (std::abs(world_x) < 0.1f) ? 0x4080FF : 0x223048; // Eixo Z destacado em azul claro
        DrawLine3D(buf, w, h, vp, Vec3{world_x, 0.0f, base_z - kExtent}, Vec3{world_x, 0.0f, base_z + kExtent}, col);
    }

    // Linhas paralelas a X
    for (float z = -kExtent; z <= kExtent; z += kStep) {
        float world_z = base_z + z;
        uint32_t col = (std::abs(world_z) < 0.1f) ? 0xFF5050 : 0x223048; // Eixo X destacado em vermelho claro
        DrawLine3D(buf, w, h, vp, Vec3{base_x - kExtent, 0.0f, world_z}, Vec3{base_x + kExtent, 0.0f, world_z}, col);
    }
}

void Renderer3D::DrawAxes(uint32_t* buf, int w, int h, const Mat4& vp) {
    Vec3 origin{0.0f, 0.05f, 0.0f};
    DrawLine3D(buf, w, h, vp, origin, Vec3{3.5f, 0.05f, 0.0f}, 0xFF3333); // Eixo X Vermelho
    DrawLine3D(buf, w, h, vp, origin, Vec3{0.0f, 3.5f, 0.0f}, 0x33FF33);  // Eixo Y Verde
    DrawLine3D(buf, w, h, vp, origin, Vec3{0.0f, 0.05f, 3.5f}, 0x3388FF); // Eixo Z Azul
}

void Renderer3D::Draw3DCube(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& center, float size, float rot) {
    float h_sz = size * 0.5f;
    float cos_a = std::cos(rot), sin_a = std::sin(rot);

    auto rotate_y = [&](float lx, float ly, float lz) -> Vec3 {
        return Vec3{
            center.x + lx * cos_a - lz * sin_a,
            center.y + ly,
            center.z + lx * sin_a + lz * cos_a
        };
    };

    // 8 vértices do cubo
    Vec3 v[8] = {
        rotate_y(-h_sz, -h_sz, -h_sz),
        rotate_y( h_sz, -h_sz, -h_sz),
        rotate_y( h_sz,  h_sz, -h_sz),
        rotate_y(-h_sz,  h_sz, -h_sz),
        rotate_y(-h_sz, -h_sz,  h_sz),
        rotate_y( h_sz, -h_sz,  h_sz),
        rotate_y( h_sz,  h_sz,  h_sz),
        rotate_y(-h_sz,  h_sz,  h_sz)
    };

    uint32_t gold_col = 0xFFD700; // Dourado Pokémon Z-A
    uint32_t cyan_col = 0x00E5FF; // Ciano Mega Evolução

    // Arestas da base inferior
    DrawLine3D(buf, w, h, vp, v[0], v[1], gold_col);
    DrawLine3D(buf, w, h, vp, v[1], v[5], gold_col);
    DrawLine3D(buf, w, h, vp, v[5], v[4], gold_col);
    DrawLine3D(buf, w, h, vp, v[4], v[0], gold_col);

    // Arestas do topo
    DrawLine3D(buf, w, h, vp, v[3], v[2], cyan_col);
    DrawLine3D(buf, w, h, vp, v[2], v[6], cyan_col);
    DrawLine3D(buf, w, h, vp, v[6], v[7], cyan_col);
    DrawLine3D(buf, w, h, vp, v[7], v[3], cyan_col);

    // Pilares verticais
    DrawLine3D(buf, w, h, vp, v[0], v[3], 0xA0C0FF);
    DrawLine3D(buf, w, h, vp, v[1], v[2], 0xA0C0FF);
    DrawLine3D(buf, w, h, vp, v[5], v[6], 0xA0C0FF);
    DrawLine3D(buf, w, h, vp, v[4], v[7], 0xA0C0FF);

    // Octaedro / Prisma interno (Símbolo Z-A)
    Vec3 top = rotate_y(0.0f, h_sz * 1.5f, 0.0f);
    Vec3 bot = rotate_y(0.0f, -h_sz * 1.5f, 0.0f);
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        DrawLine3D(buf, w, h, vp, top, v[i + 4], 0xFF80DF);
        DrawLine3D(buf, w, h, vp, bot, v[i], 0x80DFFF);
    }
}


void Renderer3D::DrawTriangleShaded(uint32_t* buf, int w, int h, const Mat4& vp, const Vec3& v0, const Vec3& v1, const Vec3& v2, uint32_t base_color) {
    float sx0, sy0, sz0;
    float sx1, sy1, sz1;
    float sx2, sy2, sz2;

    if (!vp.ProjectPoint(v0, w, h, sx0, sy0, sz0) ||
        !vp.ProjectPoint(v1, w, h, sx1, sy1, sz1) ||
        !vp.ProjectPoint(v2, w, h, sx2, sy2, sz2)) return;

    // Normal da face e iluminação difusa direcional (Sol de Lumiose City)
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 norm = edge1.Cross(edge2).Normalized();
    Vec3 light_dir = Vec3{0.50f, 0.85f, 0.35f}.Normalized();

    float diffuse = std::max(0.0f, norm.Dot(light_dir));
    float intensity = std::clamp(0.32f + 0.68f * diffuse, 0.0f, 1.0f);

    uint32_t r = static_cast<uint32_t>(((base_color >> 16) & 0xFF) * intensity);
    uint32_t g = static_cast<uint32_t>(((base_color >> 8) & 0xFF) * intensity);
    uint32_t b = static_cast<uint32_t>((base_color & 0xFF) * intensity);
    uint32_t shaded_color = (r << 16) | (g << 8) | b;

    int min_x = std::max(0, static_cast<int>(std::floor(std::min({sx0, sx1, sx2}))));
    int max_x = std::min(w - 1, static_cast<int>(std::ceil(std::max({sx0, sx1, sx2}))));
    int min_y = std::max(0, static_cast<int>(std::floor(std::min({sy0, sy1, sy2}))));
    int max_y = std::min(h - 1, static_cast<int>(std::ceil(std::max({sy0, sy1, sy2}))));

    float denom = (sy1 - sy2) * (sx0 - sx2) + (sx2 - sx1) * (sy0 - sy2);
    if (std::abs(denom) < 1e-5f) return;
    float inv_denom = 1.0f / denom;

    for (int y = min_y; y <= max_y; ++y) {
        float py = static_cast<float>(y);
        for (int x = min_x; x <= max_x; ++x) {
            float px = static_cast<float>(x);

            float w0 = ((sy1 - sy2) * (px - sx2) + (sx2 - sx1) * (py - sy2)) * inv_denom;
            float w1 = ((sy2 - sy0) * (px - sx2) + (sx0 - sx2) * (py - sy2)) * inv_denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float z = w0 * sz0 + w1 * sz1 + w2 * sz2;
                size_t idx = static_cast<size_t>(y * w + x);
                if (z < z_buffer_[idx]) {
                    z_buffer_[idx] = z;
                    buf[idx] = shaded_color;
                }
            }
        }
    }
}

const Model* Renderer3D::GetActiveModel() const {
    if (active_actor_) return &active_actor_->model;
    return active_model_.has_value() ? &*active_model_ : nullptr;
}

void Renderer3D::SetActiveActor(std::shared_ptr<game::PokeActor> actor) {
    active_actor_ = actor;
    if (active_actor_) {
        active_species_id_ = active_actor_->species_id;
    }
}

void Renderer3D::SetActiveModel(const Model& model, const std::shared_ptr<AnimationClip>& anim) {
    active_model_ = model;
    active_anim_ = anim;
    anim_timer_ = 0.0f;
}

void Renderer3D::SetSceneryModel(const Model& model, const Vec3& pos, float scale, float rot_y, uint32_t color) {
    SceneryInstance sc;
    sc.model = model;
    sc.position = pos;
    sc.scale = scale;
    sc.rot_y = rot_y;
    sc.base_color = color;
    scenery_.push_back(std::move(sc));
}

void Renderer3D::DrawModel(uint32_t* buf, int w, int h, const Mat4& vp, const Model& model, const Vec3& pos, float scale, float rot_y, uint32_t override_col) {
    float cos_a = std::cos(rot_y), sin_a = std::sin(rot_y);
    const bool has_skinning = !model.skeleton.skinning.empty();

    for (size_t m_idx = 0; m_idx < model.meshes.size(); ++m_idx) {
        const auto& mesh = model.meshes[m_idx];
        if (mesh.positions.empty() || mesh.indices.empty()) continue;

        // Paleta de cores temática oficial por Pokémon / submalha ou cor sobrescrita
        uint32_t base_col = (override_col != 0) ? override_col : 0xD8D0E4; // Mewtwo base lilás claro
        if (override_col == 0) {
            if (model.name.find("pm0025") != std::string::npos) {
                base_col = 0xF8D030; // Amarelo clássico do Pikachu
            } else if (model.name.find("pm0001") != std::string::npos) {
                base_col = (m_idx == 0) ? 0x68BAAC : 0xDE5868; // Bulbasaur corpo verde e flor/bulbo
            } else if (model.name.find("pm0008") != std::string::npos) {
                base_col = (m_idx == 0) ? 0x7898B0 : 0xD89858; // Wartortle pele e casco
            } else if (m_idx == 1) {
                base_col = 0x9844A8; // Cauda e barriga roxas do Mewtwo
            }
        }

        const size_t tri_count = mesh.indices.size() / 3;
        const bool mesh_has_weights = !mesh.weights.empty() && !mesh.joints.empty();

        for (size_t t = 0; t < tri_count; ++t) {
            uint32_t i0 = mesh.indices[t * 3 + 0];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];

            if (i0 * 3 + 2 >= mesh.positions.size() ||
                i1 * 3 + 2 >= mesh.positions.size() ||
                i2 * 3 + 2 >= mesh.positions.size()) continue;

            auto transform_vertex = [&](uint32_t idx) -> Vec3 {
                float lx = mesh.positions[idx * 3 + 0];
                float ly = mesh.positions[idx * 3 + 1];
                float lz = mesh.positions[idx * 3 + 2];

                // Deformação esquelética em tempo real (Skinning Trinity Engine)
                if (has_skinning && mesh_has_weights && idx * 4 + 3 < mesh.weights.size() && idx * 4 + 3 < mesh.joints.size()) {
                    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
                    float w_sum = 0.0f;
                    for (int k = 0; k < 4; ++k) {
                        float w_val = mesh.weights[idx * 4 + k];
                        if (w_val <= 0.0001f) continue;
                        uint16_t j = mesh.joints[idx * 4 + k];
                        if (j < model.skeleton.skinning.size()) {
                            const auto& m = model.skeleton.skinning[j];
                            // Multiplicação por matriz coluna-major 4x4
                            sx += w_val * (m[0] * lx + m[4] * ly + m[8]  * lz + m[12]);
                            sy += w_val * (m[1] * lx + m[5] * ly + m[9]  * lz + m[13]);
                            sz += w_val * (m[2] * lx + m[6] * ly + m[10] * lz + m[14]);
                            w_sum += w_val;
                        }
                    }
                    if (w_sum > 0.0001f) {
                        lx = sx; ly = sy; lz = sz;
                    }
                }

                lx *= scale;
                ly *= scale;
                lz *= scale;

                return Vec3{
                    pos.x + lx * cos_a - lz * sin_a,
                    pos.y + ly,
                    pos.z + lx * sin_a + lz * cos_a
                };
            };

            Vec3 v0 = transform_vertex(i0);
            Vec3 v1 = transform_vertex(i1);
            Vec3 v2 = transform_vertex(i2);

            // Backface culling para desempenho e clareza de profundidade
            Vec3 norm = (v1 - v0).Cross(v2 - v0);
            Vec3 view_dir = (camera_.position - v0);
            if (norm.Dot(view_dir) <= 0.0f) continue;

            if (!wireframe_only_) {
                // Polígono sólido iluminado com buffer de profundidade (Z-Buffer)
                DrawTriangleShaded(buf, w, h, vp, v0, v1, v2, base_col);
            } else {
                // Modo Aramado técnico
                uint32_t edge_col = (override_col != 0) ? override_col : 0xC8A0FF;
                DrawLine3D(buf, w, h, vp, v0, v1, edge_col);
                DrawLine3D(buf, w, h, vp, v1, v2, edge_col);
                DrawLine3D(buf, w, h, vp, v2, v0, 0x00E5FF);
            }
        }
    }
}

void Renderer3D::DrawCrosshair(uint32_t* buf, int w, int h, bool is_aiming, bool is_throwing) {
    int cx = w / 2;
    int cy = h / 2;

    if (is_aiming) {
        // Mira de precisão (Aim Mode ZL)
        uint32_t aim_col = is_throwing ? 0xFFE040 : 0x00FFCC;
        int r = 26;

        // Círculo central com marcas
        for (int a = 0; a < 36; ++a) {
            float rad0 = a * 0.1745329f;
            float rad1 = (a + 1) * 0.1745329f;
            int x0 = cx + static_cast<int>(std::cos(rad0) * r);
            int y0 = cy + static_cast<int>(std::sin(rad0) * r);
            int x1 = cx + static_cast<int>(std::cos(rad1) * r);
            int y1 = cy + static_cast<int>(std::sin(rad1) * r);
            DrawLine2D(buf, w, h, x0, y0, x1, y1, aim_col);
        }

        // Ticks de precisão
        DrawLine2D(buf, w, h, cx - r - 8, cy, cx - r + 3, cy, aim_col);
        DrawLine2D(buf, w, h, cx + r - 3, cy, cx + r + 8, cy, aim_col);
        DrawLine2D(buf, w, h, cx, cy - r - 8, cx, cy - r + 3, aim_col);
        DrawLine2D(buf, w, h, cx, cy + r - 3, cx, cy + r + 8, aim_col);

        // Ponto central
        buf[cy * w + cx] = 0xFFFFFF;
    } else {
        // Retícula neutra de terceira pessoa
        uint32_t dot_col = 0xCCD8EE;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                buf[(cy + dy) * w + (cx + dx)] = dot_col;
            }
        }
        DrawLine2D(buf, w, h, cx - 12, cy, cx - 5, cy, 0x8899B0);
        DrawLine2D(buf, w, h, cx + 5, cy, cx + 12, cy, 0x8899B0);
        DrawLine2D(buf, w, h, cx, cy - 12, cx, cy - 5, 0x8899B0);
        DrawLine2D(buf, w, h, cx, cy + 5, cx, cy + 12, 0x8899B0);
    }
}

void Renderer3D::Render(uint32_t* pixel_buffer, int width, int height, const Camera3D& camera, bool is_aiming, bool is_throwing) {
    if (!pixel_buffer || width <= 0 || height <= 0) return;

    if (z_buffer_.size() != static_cast<size_t>(width * height)) {
        z_buffer_.assign(static_cast<size_t>(width * height), 1e9f);
    } else {
        std::fill(z_buffer_.begin(), z_buffer_.end(), 1e9f);
    }

    anim_timer_ += 0.02f;

    // 1. Céu e Horizonte (Gradiente Suave 3D)
    for (int y = 0; y < height; ++y) {
        float factor = static_cast<float>(y) / height;
        uint32_t sky_col = BlendColor(0x0A0D18, 0x1E2538, factor * factor);
        uint32_t* row = &pixel_buffer[y * width];
        for (int x = 0; x < width; ++x) {
            row[x] = sky_col;
        }
    }

    // 2. Matriz de Projeção e Visão da Câmera
    Mat4 vp = camera.GetViewProjection(width, height);

    // 3. Grid de Perspectiva do Solo em 3D
    DrawGroundGrid(pixel_buffer, width, height, vp, camera);

    // 4. Eixos 3D do Mundo
    DrawAxes(pixel_buffer, width, height, vp);

    // 5. Cenários e Construções de Lumiose City (Torre Prisma e Arredores)
    for (const auto& sc : scenery_) {
        DrawModel(pixel_buffer, width, height, vp, sc.model, sc.position, sc.scale, sc.rot_y, sc.base_color);
    }

    // 6. Modelo 3D Real do Pokémon Ativo e do Oponente com Animação Esquelética e Locomoção
    if (active_actor_) {
        active_actor_->EvaluatePose();
        uint32_t flash_col = (active_actor_->hit_flash_timer > 0.0f) ? 0xFF3333 : 0;
        DrawModel(pixel_buffer, width, height, vp, active_actor_->model,
                  active_actor_->position, active_actor_->scale, active_actor_->yaw, flash_col);
    }
    if (opponent_actor_) {
        opponent_actor_->EvaluatePose();
        uint32_t flash_col = (opponent_actor_->hit_flash_timer > 0.0f) ? 0xFF3333 : 0;
        DrawModel(pixel_buffer, width, height, vp, opponent_actor_->model,
                  opponent_actor_->position, opponent_actor_->scale, opponent_actor_->yaw, flash_col);
    }
    if (!active_actor_ && !opponent_actor_) {
        if (active_model_) {
            if (active_anim_ && !active_model_->skeleton.Empty()) {
                EvaluateAnimation(*active_anim_, anim_timer_, active_model_->skeleton);
            }
            DrawModel(pixel_buffer, width, height, vp, *active_model_, Vec3{0.0f, 0.0f, 0.0f}, 1.2f, 0.0f);
        } else {
            Draw3DCube(pixel_buffer, width, height, vp, Vec3{0.0f, 1.2f, 0.0f}, 2.0f, anim_timer_);
        }
    }

    // 7. Retícula de Mira Dinâmica no Centro da Tela
    DrawCrosshair(pixel_buffer, width, height, is_aiming, is_throwing);
}

} // namespace za::gfx
