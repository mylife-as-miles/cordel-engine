// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/diagnostics.hpp"
#include "cordel/runtime.hpp"
#include "cordel/narrative_session.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>

namespace cordel {
struct Options {
    bool self_test{},uncapped{},narrative{},narrative_test{};
    double seconds{};
    std::filesystem::path scene{CORDEL_DEFAULT_SCENE},output{"tmp/native-host"};
};
static Options parse(int argc,char** argv) {
    Options options;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--self-test") options.self_test=true;
        else if(arg=="--narrative") options.narrative=true;
        else if(arg=="--narrative-test") options.narrative_test=true;
        else if(arg=="--uncapped") options.uncapped=true;
        else if((arg=="--seconds"||arg=="--scene"||arg=="--output")&&i+1<argc) {
            std::string value=argv[++i];
            if(arg=="--seconds") {
                std::size_t consumed=0;options.seconds=std::stod(value,&consumed);
                if(consumed!=value.size()||!std::isfinite(options.seconds)||options.seconds<=0)
                    throw std::invalid_argument("--seconds requires a positive finite duration");
            } else if(arg=="--scene") options.scene=value;
            else options.output=value;
        } else throw std::invalid_argument("Usage: cordel-native-host [--self-test] [--uncapped] [--seconds N] [--scene glTF] [--output dir]");
    }
    return options;
}
static void run(Platform& platform,Renderer& renderer,Counters& counters,Trace& trace,const Options& options) {
    renderer.load(options.scene);
    trace.event("scene_loaded",{{"meshes",renderer.scene().meshes.size()},{"triangles",renderer.scene().triangles()},
        {"resources",json_resources(counters)}});
    bool vsync=false;
    if(!options.uncapped) vsync=platform.enable_vsync();
    else SDL_GL_SetSwapInterval(0);
    // Offscreen EGL can accept swap interval without actually blocking. Require
    // several swaps to exhibit pacing before trusting it. Do not benchmark this warm-up.
    double warm_swap_seconds=0;
    for(int i=0;i<12;++i) {
        auto size=platform.drawable_size();renderer.render(Camera{},size[0],size[1]);
        renderer.complete_frame();
        double swap_start=monotonic_seconds();platform.present();warm_swap_seconds+=monotonic_seconds()-swap_start;
    }
    vsync=vsync&&warm_swap_seconds/12>=.012;
    if(!vsync) SDL_GL_SetSwapInterval(0);
    std::string pacing=options.uncapped?"uncapped":vsync?"vsync_observed":"monotonic_sleep_fallback";
    trace.event("loop_started",{{"pacing",pacing},{"fixed_hz",60},{"catch_up_ticks",3},
        {"backpressure","glFinish once per render frame; diagnostic synchronous path"},
        {"gpu_timing","unavailable"},{"measurement_camera",json_vector(Camera{}.position)},
        {"yaw",0},{"pitch",-8}});
    FrameRunner runner;Statistics stats;
    using Clock=std::chrono::steady_clock;
    auto deadline=Clock::now();
    double started=monotonic_seconds(),next_log=started;
    double cpu=double(std::clock())/CLOCKS_PER_SEC;
    while(!platform.input.quit) {
        FrameSample frame=runner.next(platform,renderer);
        stats.record(frame);
        if(runner.last_forced_stall) {
            auto record=frame_record(frame,platform,runner.simulation.current,counters);
            record["movement_metres"]=runner.last_movement;
            trace.event("forced_stall",std::move(record));
        }
        double time=monotonic_seconds();
        if(time>=next_log) {
            next_log=time+.5;
            auto record=frame_record(frame,platform,runner.simulation.current,counters);
            record["pacing"]=pacing;record["render_fps"]=double(stats.frames)/std::max(.000001,time-started);
            trace.event("frame",std::move(record));
            platform.title("CORDEL ENGINE — Phase 1.2 Native Host | "+std::to_string(double(stats.frames)/std::max(.000001,time-started))+" FPS | Escape: release | F6: stall");
        }
        if(!options.uncapped&&!vsync) {
            deadline+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1./60));
            // If rendering missed the deadline, skip missed slots without spinning.
            auto current=Clock::now();
            if(deadline<current) deadline=current;
            std::this_thread::sleep_until(deadline);
        }
        if(options.seconds>0&&monotonic_seconds()-started>=options.seconds) break;
    }
    double wall=monotonic_seconds()-started;
    Json summary=stats.summary(wall,double(std::clock())/CLOCKS_PER_SEC-cpu);
    write_json(options.output/"measurement.json",Json::Object{{"pacing",pacing},{"summary",summary},
        {"window_size",json_size(platform.window_size())},{"drawable_size",json_size(platform.drawable_size())},
        {"gpu",renderer.gpu()},{"gl_version",renderer.version()},
        {"frame_metric","native render loop iterations, including swap; not physical display refresh"},
        {"backpressure","synchronous glFinish per frame; bounded diagnostic path, not production queue design"},
        {"cpu_fragments","monotonic durations of CPU code, may include descheduling/driver waits"}});
    trace.event("loop_finished",{{"summary",summary}});
    platform.release();renderer.unload();
    if(!counters.empty()) throw std::runtime_error("Live resources after loop shutdown");
    trace.event("scene_unloaded",{{"resources",json_resources(counters)},{"held_input_empty",platform.input.actions.empty()}});
}
}
int main(int argc,char** argv) {
    try {
        auto options=cordel::parse(argc,argv);
        double startup=cordel::monotonic_seconds();
        cordel::Trace trace(options.output);
        cordel::Counters counters;
        {
            cordel::SdlSession sdl;
            cordel::Platform platform(850,480);
            cordel::Renderer renderer(counters);
            trace.event("host_initialized",{{"identity","CORDEL ENGINE 0.1.0-dev"},{"milestone","Phase 1.2 Native Host"},
                {"backend","SDL3 + OpenGL core"},{"sdl_version",SDL_GetVersion()},{"driver",platform.driver()},
                {"gl_version",renderer.version()},{"gpu",renderer.gpu()},{"core_profile",renderer.profile()},
                {"depth_bits",renderer.depth_bits()},{"window_size",cordel::json_size(platform.window_size())},
                {"drawable_size",cordel::json_size(platform.drawable_size())},{"asset",options.scene.string()},
                {"coordinates","RH +Y up -Z forward metre/unit; no Assimp reflection"},
                {"context_startup_seconds",cordel::monotonic_seconds()-startup},{"gpu_timing","unavailable"}});
            if(options.narrative||options.narrative_test)
                cordel::narrative::run_narrative(platform,renderer,counters,trace,options.scene,options.output,
                    CORDEL_NARRATIVE_PYTHON,options.narrative_test,options.seconds);
            else if(options.self_test) cordel::run_self_test(platform,renderer,counters,trace,options.scene,options.output);
            else cordel::run(platform,renderer,counters,trace,options);
        }
        // Destroyed the complete original renderer/context/window and SDL state
        // before constructing this optional second host, rather than coexisting.
        if(options.self_test) {
            cordel::SdlSession sdl;
            cordel::Platform platform(850,480);
            cordel::Renderer renderer(counters);renderer.load(options.scene);
            renderer.render(cordel::Camera{},850,480);platform.present();
            renderer.unload();platform.release();
            if(!counters.empty()) throw std::runtime_error("Replacement context leaked resources");
            trace.event("context_recreated",{{"after_full_host_destruction",true},{"resources",cordel::json_resources(counters)}});
            cordel::write_json(options.output/"context-recreation.json",cordel::Json::Object{
                {"success",true},{"after_full_host_destruction",true},{"resources",cordel::json_resources(counters)}});
        }
        if(!counters.empty()) throw std::runtime_error("Live resources at final shutdown");
        trace.event("host_shutdown",{{"live_resources",cordel::json_resources(counters)},
            {"window_context_destroyed",true},{"driver_memory_release","not measured"}});
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"CORDEL native host failed: "<<error.what()<<'\n';return 1;
    }
}
