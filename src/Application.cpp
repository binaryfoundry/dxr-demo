#include "Application.hpp"

#include "Input.hpp"
#include "imgui.h"
#include "math/Random.hpp"

#include <array>
#include <iostream>

#include "sdl/Main.hpp"

int main(int argc, char* argv[])
{
    std::unique_ptr<IApplication> app = std::make_unique<Application>();
    return sdl_init(app);
}

void Application::Init(std::shared_ptr<IRenderer> new_renderer)
{
    renderer = new_renderer;

    key_down_callback = [=](Scancode key)
    {
        switch (key)
        {
        default:
            break;
        }
    };

    key_up_callback = [=](Scancode key)
    {
        switch (key)
        {
        default:
            break;
        }
    };

    fps_time = timer_start();

    prop.Animate(
        context->property_manager,
        0, 1.0f, 1.0f,
        Properties::EasingFunction::Linear,
        []() {
            std::cout << "Complete!\n";
        }
    );
}

void Application::Deinit()
{
}

void Application::Update()
{
    const float time_ms = timer_end(fps_time);
    fps_time = timer_start();
    fps_time_avg = fps_alpha * fps_time_avg + (1.0f - fps_alpha) * time_ms;

    context->property_manager.Update(
        time_ms / 1000.0f);

    GuiUpdate();

    renderer->Render();
}

bool Application::GuiUpdate()
{
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    return false;
}
