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

    glm::mat4 projection;
    glm::mat4 view;

    Properties::Property<float> prop;

    std::shared_ptr<IRenderer> renderer;

    bool GuiUpdate();

public:
    void Init(std::shared_ptr<IRenderer> renderer);
    void Deinit();
    void Update();
};
