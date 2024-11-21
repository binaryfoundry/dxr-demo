#pragma once

#include "math/Math.hpp"
#include "math/Angles.hpp"

class Camera final
{
private:
    const glm::vec3 up = glm::vec3(0.0, 1.0, 0.0);

    const glm::vec3 pitch_axis = glm::vec3(1, 0, 0);
    const glm::vec3 yaw_axis = glm::vec3(0, 1, 0);
    const glm::vec3 roll_axis = glm::vec3(0, 0, 1);

    glm::vec3 direction;

    Angles orientation;
    glm::vec3 position;
    glm::vec4 viewport;

    glm::mat4 view;

    float zoom = 1.0f;
    float near_plane = 0.001f;
    float far_plane = 1000.0f;

    bool validated = false;

    void Validate();

public:
    Camera(const glm::vec3 position);
    Camera(const Camera&) = delete;
    ~Camera() = default;

    void Strafe(const float speed);
    void Forward(const float speed);

    void Yaw(const float speed);
    void Pitch(const float speed);

    void Viewport(const glm::vec4 value);
    void Zoom(const float value);

    float Aspect();
    float Near();
    float Far();
    float Zoom() const;

    glm::vec3& Position()
    {
        Validate();

        return position;
    };

    glm::mat4& View()
    {
        Validate();

        return view;
    };
};
