// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/runtime.hpp"
#include <chrono>
#include <thread>

namespace cordel {
FrameSample FrameRunner::next(Platform& platform,Renderer& renderer) {
    double event_start=monotonic_seconds();platform.poll();
    if(boundary) boundary->frame_begin(platform,renderer,simulation);
    double event_ms=(monotonic_seconds()-event_start)*1000;
    last_forced_stall=std::exchange(platform.input.stall,false);
    if(last_forced_stall) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    double now=monotonic_seconds(),raw=now-previous_time_;previous_time_=now;
    if(boundary&&boundary->reset_clock()) {clock_.reset();raw=0;}
    Vec3 before=simulation.current.position;
    double sim_start=monotonic_seconds();
    bool paused=boundary&&boundary->paused();
    if(paused) clock_.reset();
    auto step=clock_.advance(paused?0:raw);
    if(paused) step.raw=raw;
    double camera_ms=0,physics_ms=0;
    unsigned scheduled=step.ticks;step.ticks=0;
    for(unsigned tick=0;tick<scheduled;++tick) {
        if(boundary&&boundary->paused()) break;
        double physics_start=monotonic_seconds();
        auto position=simulation.current.position;
        physics.fixed_tick({position.x,position.y,position.z});
        physics_ms+=(monotonic_seconds()-physics_start)*1000;
        double tick_start=monotonic_seconds();simulation.tick(platform.input);
        ++step.ticks;
        camera_ms+=(monotonic_seconds()-tick_start)*1000;
        if(boundary) boundary->fixed_tick(renderer,simulation);
    }
    step.simulated=step.ticks*FixedClock::dt;
    if(boundary&&boundary->paused()) {clock_.reset();step.alpha=1;}
    double sim_ms=(monotonic_seconds()-sim_start)*1000;
    last_movement=length(simulation.current.position-before);
    double prep_start=monotonic_seconds();
    Camera render_camera=simulation.render_camera(step.alpha);
    auto size=platform.drawable_size();
    double extraction_ms=(monotonic_seconds()-prep_start)*1000;
    auto render=renderer.render(render_camera,size[0],size[1]);
    double completion_start=monotonic_seconds();renderer.complete_frame();
    double completion_ms=(monotonic_seconds()-completion_start)*1000;
    double present_start=monotonic_seconds();platform.present();
    return {++frames_,step,event_ms,sim_ms,camera_ms,render.prep_ms+extraction_ms,
            render.submit_ms,completion_ms,(monotonic_seconds()-present_start)*1000,
            physics_ms,physics.ticks(),physics.live(),physics.ground?physics.ground->distance:-1.,physics.overlaps};
}
}
