#include "EarlyCameraController.h"
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

namespace Mark::Engine
{
    EarlyCameraController::EarlyCameraController(const glm::vec3& _pos, const glm::vec3& _target, 
        const glm::vec3& _up, PersProjInfo& _persProjInfo)
    {
        m_cameraPosition = _pos;
        
        float aspect = (float)_persProjInfo.m_windowWidth / (float)_persProjInfo.m_windowHeight;

        if (CAMERA_LEFT_HANDED) {
            m_cameraOrientation = glm::lookAtLH(_pos, _target, _up);
            m_persProjection = glm::perspectiveLH(_persProjInfo.m_FOV, aspect, _persProjInfo.m_zNear, _persProjInfo.m_zFar);
        }
        else {
            m_cameraOrientation = glm::lookAtRH(_pos, _target, _up);
            m_persProjection = glm::perspectiveRH(_persProjInfo.m_FOV, aspect, _persProjInfo.m_zNear, _persProjInfo.m_zFar);
        }

        const glm::vec3 dir = glm::normalize(_target - _pos);
        if (CAMERA_LEFT_HANDED) {
            m_cameraOrientation = glm::quatLookAtLH(dir, _up);
        }
        else {
            m_cameraOrientation = glm::quatLookAtRH(dir, _up);
        }

        // Seed yaw/pitch roughly from dir
        m_yaw = std::atan2(dir.x, CAMERA_LEFT_HANDED ? dir.z : -dir.z);
        m_pitch = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
    }

    glm::mat4 EarlyCameraController::getViewMatrix() const
    {
        glm::mat4 translate = glm::translate(glm::mat4(1.0f), -m_cameraPosition);
        glm::mat4 rotateInv = glm::mat4_cast(glm::conjugate(m_cameraOrientation));
        return rotateInv * translate;
    }
    glm::mat4 EarlyCameraController::getVPMatrix() const
    {
        return m_persProjection * getViewMatrix();
    }

    glm::vec3 EarlyCameraController::forward() const 
    {
        const glm::vec3 localF = CAMERA_LEFT_HANDED ? glm::vec3(0, 0, 1) : glm::vec3(0, 0, -1);
        return glm::normalize(m_cameraOrientation * localF);
    }
    glm::vec3 EarlyCameraController::right() const 
    {
        return glm::normalize(m_cameraOrientation * glm::vec3(1, 0, 0));
    }
    glm::vec3 EarlyCameraController::up() const 
    {
        return glm::normalize(m_cameraOrientation * glm::vec3(0, 1, 0));
    }

    void EarlyCameraController::addMouseDelta(float _dxPixels, float _dyPixels)
    {
        m_yaw += _dxPixels * m_mouseSensitivity;
        m_pitch -= _dyPixels * m_mouseSensitivity;

        // clamp pitch to avoid flipping
        constexpr float kLimit = glm::radians(89.0f);
        m_pitch = glm::clamp(m_pitch, -kLimit, kLimit);

        // Orientation from yaw (Y up) then pitch (camera local X)
        glm::quat qYaw = glm::angleAxis(m_yaw, glm::vec3(0, 1, 0));
        glm::quat qPitch = glm::angleAxis(m_pitch, glm::vec3(1, 0, 0));
        m_cameraOrientation = glm::normalize(qYaw * qPitch);
    }

    void EarlyCameraController::tick(GLFWwindow* window)
    {
        // Simple dt
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        auto& mv = m_movement;
        mv.m_moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        mv.m_moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        mv.m_moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        mv.m_moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        mv.m_moveUp = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        mv.m_moveDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        mv.m_fastMove = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

        // RMB mouse-look
        static bool rotating = false;
        static double lastX = 0.0, lastY = 0.0;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) 
        {
            double x, y; 
            glfwGetCursorPos(window, &x, &y);
            if (!rotating) {
                rotating = true;
                // Hide cursor and allow unlimited relative motion
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                lastX = x; lastY = y; // Avoid an initial jump
            }
            else {
                addMouseDelta(static_cast<float>(x - lastX), static_cast<float>(y - lastY));
                lastX = x; lastY = y;
            }
        }
        else 
        {
            if (rotating) {
                // Release capture when RMB is released
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            rotating = false;
        }

        update(dt);
    }

    void EarlyCameraController::update(float _dt)
    {
        float boost = (m_movement.m_fastMove ? m_fastMoveCoef : 1.0f);
        float accel = m_acceleration * boost;
        float vmax = m_maxSpeed * boost;

        glm::vec3 desired{ 0.0f };
        if (m_movement.m_moveForward) desired += forward();
        if (m_movement.m_moveBackward) desired -= forward();
        if (m_movement.m_moveRight) desired += right();
        if (m_movement.m_moveLeft) desired -= right();
        if (m_movement.m_moveUp) desired -= up();
        if (m_movement.m_moveDown) desired += up();

        if (glm::length2(desired) > 0.0f) desired = glm::normalize(desired);

        m_velocity += desired * accel * _dt;

        // Cap top speed
        float speed = glm::length(m_velocity);
        if (speed > vmax && speed > 0.0f) m_velocity = (m_velocity / speed) * vmax;

        // Damping
        const bool hasInput = glm::length2(desired) > 0.0f;
        const float damp = hasInput ? m_damping : (m_damping * 3.0f);
        m_velocity -= m_velocity * glm::min(1.0f, damp * _dt);

        m_cameraPosition += m_velocity * _dt;
    }
} // namespace Mark::Engine