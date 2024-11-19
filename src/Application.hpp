#pragma once

#include <memory>

#include "Timing.hpp"

#include "properties/Property.hpp"
#include "interfaces/IApplication.hpp"
#include "interfaces/IRenderer.hpp"

class Application : public IApplication
{
private:
    const float fps_alpha = 0.9f;
    hrc::time_point fps_time;
    float fps_time_avg = 60;

    float move_speed = 0.1f;
    float forward_speed = 0.0f;
    float strafe_speed = 0.0f;

    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position = glm::vec3(0.0, 1.5, -7.0);

    Properties::Property<float> prop;

    std::shared_ptr<IRenderer> renderer;

    bool GuiUpdate();

public:
    void Init(std::shared_ptr<IRenderer> renderer);
    void Deinit();
    void Update();
};
