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

Application::Application() :
    camera(glm::vec3(0.0, 1.5, 7.0))
{
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

    const float fps_scale = std::max<float>(
        1.0f / std::min<float>(5.0f, fps_time_avg / 16.66666f), 0.1f);

    context->property_manager.Update(
        time_ms / 1000.0f);

    GuiUpdate();

    const float window_aspect_ratio =
        static_cast<float>(window_width) /
        window_height;

    const float mouse_speed = 75.0f;

    camera.Yaw(
        static_cast<float>(captured_mouse_delta_x) /
        (mouse_speed * fps_scale * window_aspect_ratio));

    camera.Pitch(
        static_cast<float>(captured_mouse_delta_y) /
        (mouse_speed * fps_scale));

    camera.Strafe(strafe_speed / fps_scale);
    camera.Forward(forward_speed / fps_scale);

    renderer->Render(camera);
}

bool Application::GuiUpdate()
{
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    return false;
}
