#pragma once

#include <memory>

#include "Timing.hpp"
#include "Camera.hpp"

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

    Camera camera;

    Properties::Property<float> prop;

    std::shared_ptr<IRenderer> renderer;

    bool GuiUpdate();

public:
    Application();

    void Init(std::shared_ptr<IRenderer> renderer);
    void Deinit();
    void Update();
};
