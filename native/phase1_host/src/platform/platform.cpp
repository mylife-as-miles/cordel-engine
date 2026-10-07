// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/platform.hpp"
#include <optional>

namespace cordel {
void sdl_require(bool ok,const char* operation) {
    if(!ok) throw std::runtime_error(std::string(operation)+": "+SDL_GetError());
}
SdlSession::SdlSession() {
    sdl_require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD),"SDL_Init");
}
SdlSession::~SdlSession() { SDL_Quit(); }
Platform::Platform(int width,int height) {
    try {
        sdl_require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3),"GL major");
        sdl_require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3),"GL minor");
        sdl_require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE),"GL core");
        sdl_require(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24),"GL depth");
        sdl_require(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1),"GL doublebuffer");
        window_=SDL_CreateWindow("CORDEL ENGINE 0.1.0-dev — Phase 1.2 Native Host",width,height,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
        sdl_require(window_!=nullptr,"SDL_CreateWindow");
        context_=SDL_GL_CreateContext(window_);
        sdl_require(context_!=nullptr,"SDL_GL_CreateContext 3.3 core");
        sdl_require(SDL_GL_MakeCurrent(window_,context_),"SDL_GL_MakeCurrent");
        input.focused=(SDL_GetWindowFlags(window_)&SDL_WINDOW_INPUT_FOCUS)!=0;
        int count=0;
        auto ids=SDL_GetGamepads(&count);
        if(count>0) open_gamepad(ids[0]);
        SDL_free(ids);
    } catch(...) { cleanup(); throw; }
}
Platform::~Platform() { cleanup(); }
void Platform::cleanup() noexcept {
    if(window_) { SDL_SetWindowRelativeMouseMode(window_,false); input.lose_focus(); }
    if(gamepad_) { SDL_CloseGamepad(gamepad_); gamepad_=nullptr; }
    if(context_) { SDL_GL_DestroyContext(context_); context_=nullptr; }
    if(window_) { SDL_DestroyWindow(window_); window_=nullptr; }
}
std::array<int,2> Platform::window_size() const {
    std::array<int,2> size{}; sdl_require(SDL_GetWindowSize(window_,&size[0],&size[1]),"window size"); return size;
}
std::array<int,2> Platform::drawable_size() const {
    std::array<int,2> size{}; sdl_require(SDL_GetWindowSizeInPixels(window_,&size[0],&size[1]),"drawable size"); return size;
}
std::string Platform::driver() const { return SDL_GetCurrentVideoDriver(); }
bool Platform::capture() {
    input.focused=true;
    capture_error.clear();
    bool ok=SDL_SetWindowRelativeMouseMode(window_,true);
    input.captured=SDL_GetWindowRelativeMouseMode(window_);
    if(!ok||!input.captured) capture_error=SDL_GetError();
    return ok&&input.captured;
}
void Platform::release() {
    sdl_require(SDL_SetWindowRelativeMouseMode(window_,false),"relative mouse release");
    input.captured=SDL_GetWindowRelativeMouseMode(window_);
    input.clear();
}
static std::optional<Action> binding(SDL_Scancode key) {
    switch(key) {
        case SDL_SCANCODE_W:return Action::Forward;
        case SDL_SCANCODE_S:return Action::Backward;
        case SDL_SCANCODE_A:return Action::Left;
        case SDL_SCANCODE_D:return Action::Right;
        case SDL_SCANCODE_SPACE:return Action::Up;
        case SDL_SCANCODE_LCTRL:return Action::Down;
        case SDL_SCANCODE_LSHIFT:return Action::Sprint;
        default:return {};
    }
}
void Platform::open_gamepad(SDL_JoystickID id) {
    if(!gamepad_) gamepad_=SDL_OpenGamepad(id);
}
void Platform::process(const SDL_Event& ev) {
    // Synthetic events in the probe use this exact desktop dispatch path.
    switch(ev.type) {
        case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:input.quit=true;release();break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:case SDL_EVENT_WINDOW_MINIMIZED:case SDL_EVENT_WINDOW_HIDDEN:
            release();input.lose_focus();break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:input.clear();input.focused=true;break;
        case SDL_EVENT_KEY_UP:
            input.actions.up(static_cast<int>(ev.key.scancode));break;
        case SDL_EVENT_KEY_DOWN:
            if(ev.key.scancode==SDL_SCANCODE_ESCAPE) { release(); break; }
            if(ev.key.scancode==SDL_SCANCODE_F6&&!ev.key.repeat) input.stall=true;
            if(input.focused) if(auto action=binding(ev.key.scancode))
                input.actions.down(static_cast<int>(ev.key.scancode),*action);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(ev.button.button==SDL_BUTTON_LEFT) capture();
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if(input.focused&&input.captured) { input.mouse_x+=ev.motion.xrel;input.mouse_y+=ev.motion.yrel; }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:open_gamepad(ev.gdevice.which);break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if(gamepad_&&SDL_GetGamepadID(gamepad_)==ev.gdevice.which) {
                SDL_CloseGamepad(gamepad_);gamepad_=nullptr;input.axes={};
            } break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if(input.focused&&(ev.gaxis.axis==SDL_GAMEPAD_AXIS_LEFTX||ev.gaxis.axis==SDL_GAMEPAD_AXIS_LEFTY))
                if(gamepad_&&SDL_GetGamepadID(gamepad_)==ev.gaxis.which)
                    input.axes[ev.gaxis.axis==SDL_GAMEPAD_AXIS_LEFTX?0:1]=std::clamp(double(ev.gaxis.value)/32767.,-1.,1.);
            break;
        default:break;
    }
}
void Platform::poll() { SDL_Event ev{}; while(SDL_PollEvent(&ev)) process(ev); }
bool Platform::resize(int w,int h) {
    if(w<=0||h<=0) throw std::invalid_argument("Nonpositive window size");
    if(driver()=="offscreen") {
        release();
        auto replacement=SDL_CreateWindow("CORDEL ENGINE — offscreen surface",w,h,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
        sdl_require(replacement!=nullptr,"recreate offscreen surface");
        if(!SDL_GL_MakeCurrent(replacement,context_)) { SDL_DestroyWindow(replacement);sdl_require(false,"surface make current"); }
        SDL_DestroyWindow(window_);window_=replacement;
        input.lose_focus();
        return true;
    }
    sdl_require(SDL_SetWindowSize(window_,w,h),"SDL_SetWindowSize");
    SDL_SyncWindow(window_);
    return false;
}
void Platform::present() { sdl_require(SDL_GL_SwapWindow(window_),"SDL_GL_SwapWindow"); }
bool Platform::enable_vsync() { return SDL_GL_SetSwapInterval(1); }
void Platform::title(const std::string& s) { SDL_SetWindowTitle(window_,s.c_str()); }
}
