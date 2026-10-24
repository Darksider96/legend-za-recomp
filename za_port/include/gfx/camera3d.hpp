#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace za::gfx {

struct Vec3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 Cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 Normalized() const {
        float l = Length();
        return l > 1e-6f ? (*this * (1.0f / l)) : Vec3{0, 0, 0};
    }
};

struct Mat4 {
    float m[4][4]{};

    static Mat4 Identity() {
        Mat4 res{};
        for (int i = 0; i < 4; ++i) res.m[i][i] = 1.0f;
        return res;
    }

    static Mat4 Perspective(float fov_rad, float aspect, float near_z, float far_z) {
        Mat4 res{};
        float tan_half = std::tan(fov_rad * 0.5f);
        res.m[0][0] = 1.0f / (aspect * tan_half);
        res.m[1][1] = 1.0f / tan_half;
        res.m[2][2] = -(far_z + near_z) / (far_z - near_z);
        res.m[2][3] = -1.0f;
        res.m[3][2] = -(2.0f * far_z * near_z) / (far_z - near_z);
        return res;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).Normalized();
        Vec3 r = f.Cross(up).Normalized();
        Vec3 u = r.Cross(f);

        Mat4 res = Identity();
        res.m[0][0] = r.x;  res.m[1][0] = r.y;  res.m[2][0] = r.z;
        res.m[0][1] = u.x;  res.m[1][1] = u.y;  res.m[2][1] = u.z;
        res.m[0][2] = -f.x; res.m[1][2] = -f.y; res.m[2][2] = -f.z;
        res.m[3][0] = -r.Dot(eye);
        res.m[3][1] = -u.Dot(eye);
        res.m[3][2] = f.Dot(eye);
        return res;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 res{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 4; ++k) {
                    res.m[i][j] += m[i][k] * o.m[k][j];
                }
            }
        }
        return res;
    }

    bool ProjectPoint(const Vec3& pt, int view_w, int view_h, float& out_sx, float& out_sy, float& out_z) const {
        float x = pt.x * m[0][0] + pt.y * m[1][0] + pt.z * m[2][0] + m[3][0];
        float y = pt.x * m[0][1] + pt.y * m[1][1] + pt.z * m[2][1] + m[3][1];
        float z = pt.x * m[0][2] + pt.y * m[1][2] + pt.z * m[2][2] + m[3][2];
        float w = pt.x * m[0][3] + pt.y * m[1][3] + pt.z * m[2][3] + m[3][3];

        if (w <= 0.05f) return false; // Atrás da câmera ou clipping plano próximo

        float inv_w = 1.0f / w;
        float ndc_x = x * inv_w;
        float ndc_y = y * inv_w;
        out_z = z * inv_w;

        out_sx = (ndc_x * 0.5f + 0.5f) * view_w;
        out_sy = (-ndc_y * 0.5f + 0.5f) * view_h; // Inversão Y de tela
        return true;
    }
};

class Camera3D {
public:
    enum class Mode {
        Orbit,
        Free
    };

    Mode mode{Mode::Orbit};
    Vec3 orbit_target{0.0f, 1.0f, 0.0f};
    float orbit_distance{4.5f};

    Vec3 position{0.0f, 1.8f, -4.5f};
    float yaw{0.0f};    // Radianos
    float pitch{0.15f}; // Radianos (olhando ligeiramente para o alvo)
    float fov{60.0f * (3.14159265f / 180.0f)};
    float move_speed{6.0f};
    float min_ground_y{0.35f};
    bool follow_target{true};

    void Follow(const Vec3& target_pos, float dt = 0.0166f) {
        if (mode == Mode::Orbit) {
            float lerp_k = std::clamp(10.0f * dt, 0.05f, 0.6f);
            orbit_target = orbit_target * (1.0f - lerp_k) + target_pos * lerp_k;
        }
    }

    void ToggleMode() {
        if (mode == Mode::Orbit) {
            mode = Mode::Free;
        } else {
            mode = Mode::Orbit;
            orbit_distance = (position - orbit_target).Length();
            if (orbit_distance < 1.5f) orbit_distance = 4.0f;
        }
    }

    void Zoom(float delta) {
        if (mode == Mode::Orbit) {
            orbit_distance = std::clamp(orbit_distance - delta, 1.2f, 30.0f);
        } else {
            fov = std::clamp(fov - delta * 0.05f, 20.0f * (3.14159265f / 180.0f), 100.0f * (3.14159265f / 180.0f));
        }
    }

    void Update(float dt, int stick_lx, int stick_ly, int stick_rx, int stick_ry) {
        // Rotação da Câmera via Analógico Direito (Mouse / RStick)
        constexpr float kRotSpeed = 0.000045f;
        yaw += stick_rx * kRotSpeed;
        pitch += stick_ry * kRotSpeed;

        constexpr float kMoveNorm = 1.0f / 32767.0f;

        if (mode == Mode::Orbit) {
            // Clamping para não virar a câmera de cabeça para baixo no modo órbita
            constexpr float kMaxPitchOrbit = 80.0f * (3.14159265f / 180.0f);
            pitch = std::clamp(pitch, -kMaxPitchOrbit, kMaxPitchOrbit);

            if (!follow_target) {
                // Ajuste de distância orbital com W/S ou LStick Y
                float zoom_input = stick_ly * kMoveNorm;
                orbit_distance = std::clamp(orbit_distance - zoom_input * move_speed * dt, 1.5f, 25.0f);

                // Altura do alvo com A/D
                orbit_target.y = std::clamp(orbit_target.y + stick_lx * (kMoveNorm * dt * 2.0f), 0.2f, 4.0f);
            }

            // Posição na esfera orbital
            position = Vec3{
                orbit_target.x - std::sin(yaw) * std::cos(pitch) * orbit_distance,
                orbit_target.y - std::sin(pitch) * orbit_distance,
                orbit_target.z - std::cos(yaw) * std::cos(pitch) * orbit_distance
            };
        } else {
            // Modo livre (Free flight)
            constexpr float kMaxPitch = 85.0f * (3.14159265f / 180.0f);
            pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);

            Vec3 forward{
                std::sin(yaw) * std::cos(pitch),
                std::sin(pitch),
                std::cos(yaw) * std::cos(pitch)
            };
            Vec3 right{
                std::cos(yaw),
                0.0f,
                -std::sin(yaw)
            };
            Vec3 planar_forward{
                std::sin(yaw),
                0.0f,
                std::cos(yaw)
            };

            float input_x = stick_lx * kMoveNorm;
            float input_y = stick_ly * kMoveNorm;

            Vec3 velocity = (planar_forward * input_y + right * input_x) * (move_speed * dt);
            position = position + velocity;
        }

        // Prevenção de penetração no chão (Ground Collision Clamping)
        if (position.y < min_ground_y) {
            position.y = min_ground_y;
        }
    }

    Mat4 GetViewMatrix() const {
        if (mode == Mode::Orbit) {
            return Mat4::LookAt(position, orbit_target, Vec3{0.0f, 1.0f, 0.0f});
        }
        Vec3 forward{
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        };
        return Mat4::LookAt(position, position + forward, Vec3{0.0f, 1.0f, 0.0f});
    }

    Mat4 GetProjectionMatrix(int width, int height) const {
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        return Mat4::Perspective(fov, aspect, 0.1f, 500.0f);
    }

    Mat4 GetViewProjection(int width, int height) const {
        return GetViewMatrix() * GetProjectionMatrix(width, height);
    }
};

} // namespace za::gfx
