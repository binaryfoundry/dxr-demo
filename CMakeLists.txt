cmake_minimum_required(VERSION 3.8)

set(PROJECT_NAME rayproj)
set(PROJECT_files_NAME rayproj-files)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SOURCES
    src/Application.cpp
    src/Context.cpp
    src/Timing.cpp
    src/Input.cpp
    src/File.cpp)

set(HEADERS
    src/Application.hpp
    src/Context.hpp
    src/Timing.hpp
    src/Input.hpp
    src/File.hpp)

set(SOURCES_INTERFACES
    src/interfaces/IApplication.cpp
    src/interfaces/IRenderer.cpp)

set(HEADERS_INTERFACES
    src/interfaces/IApplication.hpp
    src/interfaces/IRenderer.hpp)

set(SOURCES_MATH
    src/math/Math.cpp
    src/math/Angles.cpp
    src/math/Random.cpp)

set(HEADERS_MATH
    src/math/Math.hpp
    src/math/Angles.hpp
    src/math/Random.hpp)

set(SOURCES_SDL
    src/sdl/SDL.cpp
    src/sdl/Main.cpp
    src/sdl/Imgui.cpp)

set(HEADERS_SDL
    src/sdl/SDL.hpp
    src/sdl/Main.hpp
    src/sdl/Imgui.hpp)

set(SOURCES_PROPERTIES
    src/properties/Interpolator.cpp
    src/properties/Easing.cpp
    src/properties/IProperty.cpp
    src/properties/Property.cpp
    src/properties/Manager.cpp)

set(HEADERS_PROPERTIES
    src/properties/Interpolator.hpp
    src/properties/Easing.hpp
    src/properties/IProperty.hpp
    src/properties/Property.hpp
    src/properties/Manager.hpp)

set(SOURCES_IMGUI
    lib/imgui/imgui/imgui.cpp
    lib/imgui/imgui/imgui_draw.cpp
    lib/imgui/imgui/imgui_demo.cpp
    lib/imgui/imgui/imgui_widgets.cpp)

set(HEADERS_IMGUI
    lib/imgui/imgui/imconfig.h
    lib/imgui/imgui/imgui.h
    lib/imgui/imgui/imgui_internal.h
    lib/imgui/imgui/imstb_rectpack.h
    lib/imgui/imgui/imstb_textedit.h
    lib/imgui/imgui/imstb_truetype.h)

set(SOURCES_D3D12
    src/d3d12/Allocator.cpp
    src/d3d12/Renderer.cpp
    src/d3d12/Context.cpp
    src/d3d12/Descriptor.cpp
    src/d3d12/Frame.cpp
    src/d3d12/Scene.cpp
    src/d3d12/Imgui.cpp)

set(HEADERS_D3D12
    src/d3d12/Allocator.hpp
    src/d3d12/Renderer.hpp
    src/d3d12/Context.hpp
    src/d3d12/Descriptor.hpp
    src/d3d12/Frame.hpp
    src/d3d12/Scene.hpp
    src/d3d12/Imgui.hpp)

set(SOURCES_D3D12_RAYTRACING
    src/d3d12/raytracing/Allocation.cpp
    src/d3d12/raytracing/TopStructure.cpp
    src/d3d12/raytracing/BottomStructure.cpp)

set(HEADERS_D3D12_RAYTRACING
    src/d3d12/raytracing/Allocation.hpp
    src/d3d12/raytracing/TopStructure.hpp
    src/d3d12/raytracing/BottomStructure.hpp)

set(SOURCES_D3D12_SHADERS
    ${PROJECT_SOURCE_DIR}/src/d3d12/shaders/shader.hlsl)

SOURCE_GROUP("Source" FILES ${SOURCES})
SOURCE_GROUP("Source" FILES ${HEADERS})

SOURCE_GROUP("Source\\interfaces" FILES ${SOURCES_INTERFACES})
SOURCE_GROUP("Source\\interfaces" FILES ${HEADERS_INTERFACES})

SOURCE_GROUP("Source\\math" FILES ${SOURCES_MATH})
SOURCE_GROUP("Source\\math" FILES ${HEADERS_MATH})

SOURCE_GROUP("Source\\sdl" FILES ${SOURCES_SDL})
SOURCE_GROUP("Source\\sdl" FILES ${HEADERS_SDL})

SOURCE_GROUP("Source\\properties" FILES ${SOURCES_PROPERTIES})
SOURCE_GROUP("Source\\properties" FILES ${HEADERS_PROPERTIES})

SOURCE_GROUP("Source\\imgui" FILES ${SOURCES_IMGUI})
SOURCE_GROUP("Source\\imgui" FILES ${HEADERS_IMGUI})

SOURCE_GROUP("Source\\d3d12" FILES ${SOURCES_D3D12})
SOURCE_GROUP("Source\\d3d12" FILES ${HEADERS_D3D12})

SOURCE_GROUP("Source\\d3d12\\raytracing" FILES ${SOURCES_D3D12_RAYTRACING})
SOURCE_GROUP("Source\\d3d12\\raytracing" FILES ${HEADERS_D3D12_RAYTRACING})

SOURCE_GROUP("Source\\d3d12\\shaders" FILES ${SOURCES_D3D12_SHADERS})

include_directories(
    ${PROJECT_SOURCE_DIR}/lib/d3d12
    ${PROJECT_SOURCE_DIR}/lib/glm
    ${PROJECT_SOURCE_DIR}/lib/stb
    ${PROJECT_SOURCE_DIR}/lib/imgui/imgui)

if (WIN32)
    message(STATUS "Platform: Windows")
    add_compile_options("/EHsc")

    set(SDL2_PATH ${PROJECT_SOURCE_DIR}/lib/sdl/win/sdl2)
    set(SDL2_LIB_PATH ${SDL2_PATH}/lib/x64)

    set(SDL2_IMAGE_PATH ${PROJECT_SOURCE_DIR}/lib/sdl/win/sdl2-image)
    set(SDL2_IMAGE_LIB_PATH ${SDL2_IMAGE_PATH}/lib/x64)

    include_directories(${SDL2_PATH}/include)
    link_directories(${SDL2_LIB_PATH})

    include_directories(${SDL2_IMAGE_PATH}/include)
    link_directories(${SDL2_IMAGE_LIB_PATH})
endif ()

add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${SOURCES_INTERFACES}
    ${HEADERS_INTERFACES}
    ${SOURCES_MATH}
    ${HEADERS_MATH}
    ${SOURCES_SDL}
    ${HEADERS_SDL}
    ${SOURCES_PROPERTIES}
    ${HEADERS_PROPERTIES}
    ${SOURCES_IMGUI}
    ${HEADERS_IMGUI}
    ${SOURCES_D3D12}
    ${HEADERS_D3D12}
    ${SOURCES_D3D12_RAYTRACING}
    ${HEADERS_D3D12_RAYTRACING}
    ${SOURCES_D3D12_SHADERS})

add_custom_target(
    ${PROJECT_files_NAME} ALL
    COMMENT "Copying Files..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/files/
    ${CMAKE_CURRENT_BINARY_DIR}/files)

add_dependencies(
    ${PROJECT_NAME}
    ${PROJECT_files_NAME})

if (WIN32)
    set_property(
        TARGET ${PROJECT}
        PROPERTY
        VS_DPI_AWARE
        "PerMonitor"
    )

    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include/
        ${SDL2_PATH}/include
        ${SDL2_IMAGE_PATH}/include)

    target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        SDL2
        SDL2main
        SDL2_image)

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SDL2_LIB_PATH}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SDL2_IMAGE_LIB_PATH}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

    set_source_files_properties(
        ${PROJECT_SOURCE_DIR}/src/d3d12/shaders/shader.hlsl
        PROPERTIES
        VS_SHADER_TYPE Library
        VS_SHADER_MODEL 6.3
        VS_SHADER_VARIABLE_NAME compiledShader
        VS_SHADER_OUTPUT_HEADER_FILE ${PROJECT_SOURCE_DIR}/src/d3d12/shaders/shader.fxh)

endif ()