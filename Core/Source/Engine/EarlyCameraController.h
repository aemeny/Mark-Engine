#pragma once
#include <glm/glm.hpp>
#include <glm/ext.hpp>

static bool constexpr CAMERA_LEFT_HANDED = true;
struct GLFWwindow;

namespace Mark::Engine
{
    struct PersProjInfo
    {
        float m_FOV;
        float m_windowWidth;
        float m_windowHeight;
        float m_zNear;
        float m_zFar;
    };
    struct CameraMovement
    {
        bool m_moveForward{ false };
        bool m_moveBackward{ false };
        bool m_moveLeft{ false };
        bool m_moveRight{ false };
        bool m_moveUp{ false };
        bool m_moveDown{ false };
        bool m_fastMove{ false };
    };

    struct EarlyCameraController
    {
        EarlyCameraController(const glm::vec3& _pos, const glm::vec3& _target,
            const glm::vec3& _up, PersProjInfo& _persProjInfo);
        ~EarlyCameraController() = default;

        const glm::mat4& getProjMatrix() const { return m_persProjection; }
        glm::vec3 getCameraPosition() const { return m_cameraPosition; }
        glm::mat4 getViewMatrix() const;
        glm::mat4 getVPMatrix() const;

        void tick(GLFWwindow* window);

        CameraMovement m_movement;
    private:
        glm::mat4 m_persProjection{ glm::mat4(0.0) };
        glm::vec3 m_cameraPosition{ glm::vec3(0.0f) };
        glm::quat m_cameraOrientation{ glm::quat(glm::vec3(0.0f)) };
        
        glm::vec3 m_velocity{ 0.0f };
        float m_yaw{ 0.0f }, m_pitch{ 0.0f }; // radians
        float m_mouseSensitivity{ 0.0015f }; // rad/pixel

        float m_acceleration{ 100.0f };
        float m_damping{ 12.0f };
        float m_maxSpeed{ 10.0f };
        float m_fastMoveCoef{ 6.0f };

        glm::vec3 forward() const;
        glm::vec3 right() const;
        glm::vec3 up() const;
        void update(float _dt);
        void addMouseDelta(float _dxPixels, float _dyPixels);
    };
} // namespace Mark::Engine