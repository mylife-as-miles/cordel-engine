// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/runtime.hpp"
#include <chrono>
#include <thread>

namespace cordel {
FrameSample FrameRunner::next(Platform& platform,Renderer& renderer) {
    double event_start=monotonic_seconds();platform.poll();
    double event_ms=(monotonic_seconds()-event_start)*1000;
    last_forced_stall=std::exchange(platform.input.stall,false);
    if(last_forced_stall) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    double now=monotonic_seconds(),raw=now-previous_time_;previous_time_=now;
    Vec3 before=simulation.current.position;
    double sim_start=monotonic_seconds();auto step=clock_.advance(raw);
    double camera_ms=0;
    for(unsigned tick=0;tick<step.ticks;++tick) {
        double tick_start=monotonic_seconds();simulation.tick(platform.input);
        camera_ms+=(monotonic_seconds()-tick_start)*1000;
    }
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
            render.submit_ms,completion_ms,(monotonic_seconds()-present_start)*1000};
}
}
