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

    entities.resize(3);
    entities[0].scale = glm::vec3(5, 5, 5);
    entities[0].position = glm::vec3(0, 0, 2);
    entities[0].instance_id = 0;

    entities[1].position = glm::vec3(-1.5, 2, 2);
    entities[1].instance_id = 1;

    entities[2].position = glm::vec3(2, 2, 2);
    entities[2].instance_id = 1;
}

void Application::Deinit()
{
}

static float zoomValue = 1.0f;
static double ticks = 0.0;

void updateQuaternion(
    glm::quat& quaternion,
    const float dt,
    const glm::vec3& rotation_axis,
    const float angular_speed)
{
    const float angle = angular_speed * dt;
    const glm::quat incremental_rotation = glm::angleAxis(
        glm::radians(angle),
        glm::normalize(rotation_axis));
    quaternion = incremental_rotation * quaternion;
    quaternion = glm::normalize(quaternion);
}

void Application::Update()
{
    entities[1].position.y = static_cast<float>(2.0 + sin(ticks * 0.01));
    entities[2].position.z = static_cast<float>(2.0 + sin(ticks * 0.003));

    updateQuaternion(entities[1].orientation, 1.0f, glm::vec3(0, 1, 0), 1.0f);
    updateQuaternion(entities[2].orientation, 1.0f, glm::vec3(1, 0, 0), 1.0f);
    ticks++;

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

    camera.Viewport(
        glm::vec4(0, 0, window_width, window_height));

    camera.Zoom(zoomValue);

    camera.Yaw(
        static_cast<float>(captured_mouse_delta_x) /
        (mouse_speed * fps_scale * window_aspect_ratio));

    camera.Pitch(
        static_cast<float>(captured_mouse_delta_y) /
        (mouse_speed * fps_scale));

    camera.Strafe(strafe_speed / fps_scale);
    camera.Forward(forward_speed / fps_scale);

    renderer->Render(
        camera,
        entities);
}

bool Application::GuiUpdate()
{
    ImGui::NewFrame();

    ImGui::Begin("Camera Controls");
    ImGui::SliderFloat("Float Value", &zoomValue, 0.25, 4.0f);
    ImGui::End();

    return false;
}
