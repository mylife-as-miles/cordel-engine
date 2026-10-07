// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/runtime.hpp"
#include "logic_suite.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace cordel {
static void push_key(SDL_Scancode key,bool down=true) {
    SDL_Event ev{};ev.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;ev.key.scancode=key;
    sdl_require(SDL_PushEvent(&ev),"push key");
}
static void push_window(Uint32 type) {
    SDL_Event ev{};ev.type=type;sdl_require(SDL_PushEvent(&ev),"push focus");
}
static Json pixel_json(const std::array<unsigned char,4>& p) {
    return Json::Array{int(p[0]),int(p[1]),int(p[2]),int(p[3])};
}
static std::array<unsigned char,4> center(Renderer& r,int w,int h) {
    auto pixels=r.read_pixels(w,h);auto i=(static_cast<std::size_t>(h/2)*static_cast<std::size_t>(w)+static_cast<std::size_t>(w/2))*4;
    return {pixels[i],pixels[i+1],pixels[i+2],pixels[i+3]};
}
static Json depth_probe(Checks& check,Renderer& r,int w,int h) {
    Camera camera;camera.position={-1.8,1.2,4};camera.pitch=0;
    std::array<std::array<unsigned char,4>,4> samples{};
    Json::Array results;
    for(int i=0;i<4;++i) {
        bool depth=i<2,reverse=i%2!=0;
        r.render(camera,w,h,depth,reverse);samples[i]=center(r,w,h);
        results.emplace_back(Json::Object{{"depth",depth},{"reversed",reverse},{"center_rgba",pixel_json(samples[i])}});
    }
    check.require(samples[0]==samples[1],"depth independent of mesh order");
    check.require(samples[0][0]>100&&samples[0][0]>samples[0][2]*3,"near red wins depth");
    check.require(samples[2]!=samples[3],"without depth submission order matters");
    check.require(samples[2][2]>samples[2][0]*3&&samples[3][0]>samples[3][2]*3,"far blue / red control colors");
    return Json::Object{{"drawable",Json::Array{w,h}},{"samples",results}};
}
static Json square_probe(Checks& check,Renderer& r,int w,int h) {
    Camera camera;camera.position={1.2,.5,4};camera.pitch=0;
    r.render(camera,w,h);
    auto pixels=r.read_pixels(w,h);
    auto top=(static_cast<std::size_t>(h)-1)*static_cast<std::size_t>(w)*4;
    check.require(pixels[top]==24&&pixels[top+1]==33&&pixels[top+2]==43&&pixels[top+3]==255,
        "valid framebuffer corner clear "+std::to_string(w)+"x"+std::to_string(h)+" actual="+
        pixel_json({pixels[top],pixels[top+1],pixels[top+2],pixels[top+3]}).dump());
    int minx=w,miny=h,maxx=-1,maxy=-1;
    // The front face's lighting is ~0.35+0.65*.5/length(-.4,.8,.5), matching Phase 1.1.
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        auto i=(static_cast<std::size_t>(y)*static_cast<std::size_t>(w)+static_cast<std::size_t>(x))*4;
        if(std::abs(int(pixels[i])-153)<=2&&std::abs(int(pixels[i+1])-146)<=2&&std::abs(int(pixels[i+2])-102)<=2) {
            minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);
        }
    }
    int width=maxx-minx+1,height=maxy-miny+1;
    check.require(width>10&&height>10,"cream face detected");
    check.require(std::abs(width-height)<=1,"square perspective proportion");
    return Json::Object{{"front_face_pixels",Json::Array{width,height}},{"ratio",double(width)/height}};
}
void run_self_test(Platform& p,Renderer& r,Counters& counters,Trace& trace,
                   const std::filesystem::path& scene,const std::filesystem::path& out) {
    Checks check;
    auto logic=logic_checks(scene);
    trace.event("logic_checks",{{"passed",logic},{"context_independent",true}});
    Json::Array cycles,resize_results;
    Json depth;
    for(int cycle=0;cycle<5;++cycle) {
        check.require(counters.empty(),"zero counters before scene load");
        r.load(scene);
        check.require(r.scene().meshes.size()==7&&r.scene().triangles()==84,"loaded seven meshes / 84 triangles");
        check.require(counters.live[0]==1&&counters.live[1]==0&&counters.live[2]==7&&counters.live[3]==7&&counters.live[4]==7&&counters.live[5]==1,"explicit live scene objects");
        trace.event("scene_loaded",{{"cycle",cycle+1},{"meshes",7},{"triangles",84},{"resources",json_resources(counters)}});
        FrameRunner runner;
        if(cycle==0) {
            r.render(Camera{},850,480);r.save_ppm(out/"viewport.ppm",850,480);
            depth=depth_probe(check,r,850,480);
            write_json(out/"depth-probe.json",depth);
            Camera camera;camera.position={-1.8,1.2,4};camera.pitch=0;
            r.render(camera,850,480);r.save_ppm(out/"depth-on.ppm",850,480);
            r.render(camera,850,480,false);r.save_ppm(out/"depth-off.ppm",850,480);
            for(auto size:{std::array{850,480},std::array{500,500},std::array{320,500},std::array{1280,720}}) {
                bool recreated=p.resize(size[0],size[1]);p.poll();
                auto drawable=p.drawable_size(),window=p.window_size();
                check.require(drawable==size&&window==size,"actual resize dimensions");
                Json squares=square_probe(check,r,drawable[0],drawable[1]);
                r.save_ppm(out/("resize-"+std::to_string(size[0])+"x"+std::to_string(size[1])+".ppm"),size[0],size[1]);
                Json depth_result=depth_probe(check,r,drawable[0],drawable[1]);
                // Camera controls still work through the same event/tick/frame path after surface changes.
                push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);push_key(SDL_SCANCODE_W);
                runner.reset();Vec3 before=runner.simulation.current.position;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));runner.next(p,r);
                check.require(length(runner.simulation.current.position-before)>0,"camera continues after resize");
                push_key(SDL_SCANCODE_W,false);p.poll();
                resize_results.emplace_back(Json::Object{{"window",json_size(window)},{"drawable",json_size(drawable)},
                    {"aspect",double(drawable[0])/drawable[1]},{"square",squares},{"depth",depth_result},
                    {"offscreen_surface_recreated",recreated},{"context_retained",true},{"camera_after_resize",true}});
            }
            write_json(out/"resize-probe.json",Json::Object{{"results",resize_results},{"high_dpi_device_verified",false}});
            p.resize(850,480);p.poll();
            // Required W-down -> focus-loss check traverses SDL's queue.
            push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);push_key(SDL_SCANCODE_W);p.poll();
            check.require(p.input.actions.held(Action::Forward),"SDL queued W down");
            push_window(SDL_EVENT_WINDOW_FOCUS_LOST);p.poll();
            check.require(p.input.actions.empty()&&!p.input.focused&&!p.input.captured,"SDL focus loss clears W");
            // Equivalent keyboard controls traversing real event dispatch and fixed ticks.
            Json::Array keyboard_results;
            double active_camera_sum=0,active_camera_worst_per_frame=0;
            unsigned active_camera_ticks=0;
            for(auto key:{SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_SPACE,SDL_SCANCODE_LCTRL}) {
                p.release();push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);push_key(key);
                runner.reset();Vec3 before=runner.simulation.current.position;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));auto sample=runner.next(p,r);
                check.require(sample.step.ticks>0,"queued keyboard actually executes a fixed tick");
                active_camera_sum+=sample.camera_ms;active_camera_ticks+=sample.step.ticks;
                active_camera_worst_per_frame=std::max(active_camera_worst_per_frame,sample.camera_ms);
                Vec3 displacement=runner.simulation.current.position-before;
                check.near(length(displacement),3*sample.step.simulated,"queued keyboard fixed-step distance");
                Vec3 expected;
                switch(key) {
                    case SDL_SCANCODE_W:expected=Camera{}.forward();break;
                    case SDL_SCANCODE_S:expected=Camera{}.forward()*-1;break;
                    case SDL_SCANCODE_A:expected={-1,0,0};break;
                    case SDL_SCANCODE_D:expected={1,0,0};break;
                    case SDL_SCANCODE_SPACE:expected={0,1,0};break;
                    default:expected={0,-1,0};break;
                }
                check.vec(displacement,expected*(3*sample.step.simulated),"queued key direction");
                keyboard_results.emplace_back(Json::Object{{"scancode",int(key)},{"displacement",json_vector(displacement)},
                    {"camera_tick_cpu_ms",sample.camera_ms},{"fixed_ticks",sample.step.ticks},
                    {"simulated_dt",sample.step.simulated}});
                push_key(key,false);p.poll();check.require(p.input.actions.empty(),"queued key up");
            }
            push_key(SDL_SCANCODE_W);push_key(SDL_SCANCODE_LSHIFT);runner.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));auto sprint=runner.next(p,r);
            check.near(length(runner.simulation.current.position-Camera{}.position),9*sprint.step.simulated,"queued sprint 9m/s");
            p.release();
            // Relative mode is queried from SDL. Synthetic motion only proves the
            // dispatch/mode path; no claim of real mouse displacement in offscreen.
            SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;
            sdl_require(SDL_PushEvent(&click),"push capture click");p.poll();
            bool captured=p.input.captured;
            check.require(captured==SDL_GetWindowRelativeMouseMode(p.window()),"relative mode SDL status matches");
            if(!captured&&p.driver()!="offscreen") throw std::runtime_error("Relative mouse capture unavailable: "+p.capture_error);
            double yaw=0,pitch=0;
            if(captured) {
                SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.xrel=100;motion.motion.yrel=-40;
                sdl_require(SDL_PushEvent(&motion),"push relative mouse motion");
                Camera camera;camera.pitch=0;runner.reset(camera);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));runner.next(p,r);
                yaw=runner.simulation.current.yaw;pitch=runner.simulation.current.pitch;
                check.near(yaw,15,"relative yaw");check.near(pitch,6,"relative pitch");
            }
            push_key(SDL_SCANCODE_ESCAPE);p.poll();
            check.require(!SDL_GetWindowRelativeMouseMode(p.window())&&!p.input.captured&&p.input.actions.empty(),"Escape releases capture");
            int gamepads=0;auto ids=SDL_GetGamepads(&gamepads);SDL_free(ids);
            write_json(out/"input-probe.json",Json::Object{{"keyboard",keyboard_results},{"sprint_multiplier",3},
                {"active_camera_ticks",active_camera_ticks},{"active_camera_mean_cpu_ms_per_tick",active_camera_sum/active_camera_ticks},
                {"active_camera_worst_cpu_ms_per_frame",active_camera_worst_per_frame},
                {"focus_clear",true},{"sdl_relative_window_flag",captured},{"capture_error",p.capture_error},
                {"sdl_has_real_keyboard_focus",SDL_GetKeyboardFocus()==p.window()},
                {"physical_relative_motion_verified",false},
                {"synthetic_relative_yaw",yaw},{"synthetic_relative_pitch",pitch},{"escape_released",true},
                {"physical_mouse_verified",false},{"physical_gamepads",gamepads},
                {"analog_logic_tested",true},{"physical_controller_verified",false}});
            // Actual F6 -> common loop sleep -> clock -> ticks -> render path.
            push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);push_key(SDL_SCANCODE_W);push_key(SDL_SCANCODE_F6);
            runner.reset();auto stall=runner.next(p,r);
            check.require(runner.last_forced_stall&&stall.step.raw>=.25,"actual F6 250ms forced stall");
            check.require(stall.step.ticks==3,"F6 bounded three ticks");
            check.near(stall.step.simulated,.05,"F6 50ms simulated");
            check.near(runner.last_movement,.15,"F6 W displacement .150m");
            check.require(stall.step.dropped>.18,"F6 discarded excess backlog");
            auto record=frame_record(stall,p,runner.simulation.current,counters);
            record["movement_metres"]=runner.last_movement;
            trace.event("forced_stall",record);write_json(out/"stall-probe.json",record);
            push_key(SDL_SCANCODE_W,false);p.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));auto recovery=runner.next(p,r);
            check.require(recovery.step.ticks<=2&&recovery.step.dropped==0,"stall bounded recovery no catch-up debt");
        }
        // Five sessions actually render and move, then unload all per-scene GL objects.
        p.release();push_window(SDL_EVENT_WINDOW_FOCUS_GAINED);push_key(SDL_SCANCODE_W);
        runner.reset();
        unsigned tick_count=0;
        for(int f=0;f<6;++f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(17));
            auto frame=runner.next(p,r);tick_count+=frame.step.ticks;
        }
        check.require(tick_count>=6,"lifecycle session executed fixed ticks");
        p.release();r.unload();r.check();
        check.require(counters.empty(),"all per-scene GL objects destroyed");
        check.require(counters.created==counters.destroyed,"GL creation/destruction balanced");
        check.require(p.input.actions.empty()&&p.input.axes==std::array<double,2>{}&&!p.input.captured,"unload releases input and mouse");
        Json::Object record{{"cycle",cycle+1},{"render_frames",6},{"fixed_ticks",tick_count},
            {"live_resources",json_resources(counters)},{"held_empty",true},{"mouse_captured",false},
            {"driver_memory_measured",false}};
        cycles.emplace_back(record);trace.event("scene_unloaded",record);
    }
    write_json(out/"lifecycle.json",Json::Object{{"cycles",cycles},{"per_scene_program",true},
        {"host_global_gl_objects",0},{"final_live_resources",json_resources(counters)}});
    write_json(out/"self-test.json",Json::Object{{"success",true},{"native_logic_checks",logic},
        {"graphics_runtime_checks",check.passed},{"scene_cycles",5},
        {"hardware_verified",false},{"gpu_timing","unavailable"}});
    trace.event("self_test_passed",{{"logic_checks",logic},{"graphics_runtime_checks",check.passed},
        {"scene_cycles",5}});
}
}
