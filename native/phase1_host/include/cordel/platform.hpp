// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/logic.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <string>

namespace cordel {
void sdl_require(bool ok,const char* operation);
class SdlSession {
public:
    SdlSession();
    ~SdlSession();
    SdlSession(const SdlSession&)=delete;
    SdlSession& operator=(const SdlSession&)=delete;
};
class Platform {
    SDL_Window* window_{};
    SDL_GLContext context_{};
    SDL_Gamepad* gamepad_{};
    void cleanup() noexcept;
    void open_gamepad(SDL_JoystickID id);
public:
    Input input;
    std::string capture_error;
    int narrative_choice{};
    bool narrative_ack{},narrative_cancel{};
    Platform(int width,int height);
    ~Platform();
    Platform(const Platform&)=delete;
    Platform& operator=(const Platform&)=delete;
    SDL_Window* window() const { return window_; }
    std::array<int,2> window_size() const;
    std::array<int,2> drawable_size() const;
    std::string driver() const;
    bool capture();
    void release();
    void process(const SDL_Event& event);
    void poll();
    // Offscreen SDL 3.4.8 only changes size metadata, not its EGL pbuffer.
    // Recreate the offscreen window/surface, retaining the current GL context.
    // Desktop drivers use SDL_SetWindowSize. Return whether surface recreated.
    bool resize(int width,int height);
    void present();
    bool enable_vsync();
    void title(const std::string& value);
};
}
