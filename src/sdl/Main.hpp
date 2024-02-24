#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <memory>

#include "../interfaces/IApplication.hpp"

int sdl_init(std::unique_ptr<IApplication>& app);
