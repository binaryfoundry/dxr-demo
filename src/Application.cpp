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

    SetupScene();

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

void Application::SetupScene()
{

}

void Application::Update()
{
    const float time_ms = timer_end(fps_time);
    fps_time = timer_start();
    fps_time_avg = fps_alpha * fps_time_avg + (1.0f - fps_alpha) * time_ms;

    //const float fps_scale = std::max<float>(
    //    1.0f / std::min<float>(5.0f, fps_time_avg / 16.66666f), 0.1f);

    context->property_manager.Update(
        time_ms / 1000.0f);

    /*const bool reinit_pipeline =*/
    GuiUpdate();

    //const float window_aspect_ratio =
    //    static_cast<float>(window_width) /
    //    window_height;

    renderer->Render();
}

void Application::ViewScale()
{
    //const float window_aspect =
    //    static_cast<float>(window_width) /
    //    window_height;

    projection = glm::ortho<float>(
        0,
        static_cast<float>(window_width),
        static_cast<float>(window_height),
        0,
        -1.0f,
        1.0f);

    view = glm::mat4();

    view = glm::translate(
        view,
        glm::vec3(0.0f, 0.0f, 0.0f));

    view = glm::scale(
        view,
        glm::vec3(1.0f, 1.0f, 1.0f));
}

bool Application::GuiUpdate()
{
    ImGui::NewFrame();

    ImGui::Begin(
        "Menu",
        NULL,
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)",
        fps_time_avg,
        1000.0f / fps_time_avg);

    ImGui::End();

    return false;
}
