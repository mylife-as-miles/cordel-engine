// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/motor_debug.hpp"
#include <thread>
#include <chrono>
namespace cordel {
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
class PauseProbe:public FrameBoundary {
public:bool stop{},reset{};
    void frame_begin(Platform&,Renderer&,Simulation&) override {}
    void fixed_tick(Renderer&,Simulation&) override {}
    bool paused() const override {return stop;}
    bool reset_clock() override {return std::exchange(reset,false);}
};
void key(Platform&p,SDL_Scancode code,bool down=true) {SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.windowID=SDL_GetWindowID(p.window());e.key.scancode=code;require(SDL_PushEvent(&e),"SDL key push failed");}
}
void run_motor_test(Platform&p,Renderer&r,Counters&c,Trace&trace,const std::filesystem::path&out) {
    SDL_GL_SetSwapInterval(0);Json::Array cycles;Json stall,focus,pause;std::size_t frames=0;
    for(unsigned cycle=0;cycle<5;++cycle) {
        r.load(motor_debug_scene());FrameRunner runner;runner.enable_motor();p.input.focused=true;
        Camera camera;camera.position={0,4,10};camera.pitch=-20;runner.reset(camera);
        auto step=[&] {auto sample=runner.next(p,r);++frames;std::this_thread::sleep_for(std::chrono::milliseconds(17));return sample;};
        for(int i=0;i<10;++i)step();
        require(runner.motor->state().grounded,"native motor settles on ground");
        auto start=runner.motor->state().position;key(p,SDL_SCANCODE_W);for(int i=0;i<20;++i)step();
        require(runner.motor->state().position.z<start.z-1,"native W advances motor");
        if(cycle==0) {
            auto before=runner.motor->state();key(p,SDL_SCANCODE_F6);auto frame=step();auto after=runner.motor->state();
            require(frame.step.ticks==3&&frame.step.dropped>=.19,"motor forced stall remains bounded");
            require(std::abs(physics::length(after.position-before.position)-.225)<.0001,"stall motor advances three 4.5m/s ticks");
            stall=Json::Object{{"raw_seconds",frame.step.raw},{"motor_ticks",std::size_t(after.ticks-before.ticks)},{"simulated_seconds",frame.step.simulated},
                {"dropped_seconds",frame.step.dropped},{"horizontal_displacement_m",std::hypot(after.position.x-before.position.x,after.position.z-before.position.z)},
                {"vertical_displacement_m",after.position.y-before.position.y}};
            SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;e.window.windowID=SDL_GetWindowID(p.window());require(SDL_PushEvent(&e),"focus event failed");
            step();auto stopped=runner.motor->state().position;for(int i=0;i<10;++i)step();
            require(p.input.actions.empty()&&physics::length(runner.motor->state().position-stopped)<1e-8,"focus loss stops motor input");
            focus=Json::Object{{"held_empty",p.input.actions.empty()},{"displacement_after_clear_m",physics::length(runner.motor->state().position-stopped)}};
            // Gravity remains authoritative even without input focus.
            runner.motor->reset({0,4,2});
            auto air_before=runner.motor->state();key(p,SDL_SCANCODE_F6);auto air_frame=step();auto air_after=runner.motor->state();
            require(air_frame.step.ticks==3&&air_after.ticks-air_before.ticks==3,"air stall three motor ticks");
            require(std::abs(air_after.position.y-air_before.position.y+.01635)<.00001,"air stall gravity advances only three ticks");
            auto sf=stall.object();sf["airborne_raw_seconds"]=air_frame.step.raw;sf["airborne_dropped_seconds"]=air_frame.step.dropped;
            sf["airborne_vertical_displacement_m"]=air_after.position.y-air_before.position.y;sf["airborne_motor_ticks"]=3;stall=sf;
            auto falling=runner.motor->state().position;for(int i=0;i<10;++i)step();require(runner.motor->state().position.y<falling.y,"focus loss does not pause gravity");
            PauseProbe boundary;runner.boundary=&boundary;boundary.stop=true;auto ticks=runner.motor->state().ticks;auto pause_frames=frames;
            for(int i=0;i<20;++i)step();
            require(runner.motor->state().ticks==ticks,"explicit pause stops motor ticks");
            boundary.stop=false;boundary.reset=true;auto resume=step();require(resume.step.dropped==0,"pause resume carries no catch-up debt");
            pause=Json::Object{{"paused_motor_ticks",std::size_t(runner.motor->state().ticks-ticks)},{"render_frames",frames-pause_frames},{"resume_dropped_seconds",resume.step.dropped}};runner.boundary=nullptr;
            for(auto size:{std::array<int,2>{500,500},std::array<int,2>{320,500},std::array<int,2>{1200,700},std::array<int,2>{850,480}}) {p.resize(size[0],size[1]);step();require(p.drawable_size()==size,"motor drawable resize");}
            r.save_ppm(out/"motor-greybox.ppm",850,480);
        }
        p.release();runner.motor.reset();runner.physics.clear();r.unload();auto live=runner.physics.live();
        require(character::CharacterMotor::live_count()==0&&live.bodies==0&&live.shapes==0&&c.empty(),"motor scene resources release");
        cycles.emplace_back(Json::Object{{"cycle",cycle},{"motors",character::CharacterMotor::live_count()},{"bodies",live.bodies},{"shapes",live.shapes},{"gl",json_resources(c)}});
    }
    require(physics::PhysicsWorld::global_live().worlds==0,"five motor worlds destroyed");
    write_json(out/"motor-runtime.json",Json::Object{{"success",true},{"frames",frames},{"stall",stall},{"focus",focus},{"pause",pause},{"cycles",cycles},{"final_gl",json_resources(c)},
        {"final_motors",character::CharacterMotor::live_count()},{"final_worlds",physics::PhysicsWorld::global_live().worlds}});
    trace.event("motor_gate_passed",{{"frames",frames},{"cycles",cycles.size()},{"stall",stall},{"resources",json_resources(c)}});
}
}
