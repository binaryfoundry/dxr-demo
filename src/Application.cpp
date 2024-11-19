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
        case Scancode::S_W:
            forward_speed = move_speed;
            break;
        case Scancode::S_S:
            forward_speed = -move_speed;
            break;
        case Scancode::S_A:
            strafe_speed = move_speed;
            break;
        case Scancode::S_D:
            strafe_speed = -move_speed;
            break;
        default:
            break;
        }
    };

    key_up_callback = [=](Scancode key)
    {
        switch (key)
        {
        case Scancode::S_W:
            forward_speed = 0.0f;
            break;
        case Scancode::S_S:
            forward_speed = 0.0f;
            break;
        case Scancode::S_A:
            strafe_speed = 0.0f;
            break;
        case Scancode::S_D:
            strafe_speed = 0.0f;
            break;
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

    position.z += forward_speed;
    position.x += strafe_speed;

    renderer->Render(position);
}

bool Application::GuiUpdate()
{
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    return false;
}
