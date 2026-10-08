// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/diagnostics.hpp"
#include <iostream>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace cordel {
Json json_vector(Vec3 v) { return Json::Array{v.x,v.y,v.z}; }
Json json_size(std::array<int,2> size) { return Json::Array{size[0],size[1]}; }
Json json_resources(const Counters& c) {
    return Json::Object{{"programs",c.live[0]},{"shader_stages",c.live[1]},
        {"vaos",c.live[2]},{"vbos",c.live[3]},{"ebos",c.live[4]},{"scenes",c.live[5]},{"textures",0}};
}
void write_json(const std::filesystem::path& path,const Json& value) {
    std::ofstream out(path);out<<value.dump()<<'\n';
    if(!out) throw std::runtime_error("JSON write failed: "+path.string());
}
Trace::Trace(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    stream_.open(directory/"native-trace.jsonl",std::ios::out|std::ios::trunc);
    if(!stream_) throw std::runtime_error("Trace open failed");
}
void Trace::event(std::string name,Json::Object fields) {
    fields["event"]=std::move(name);fields["timestamp_seconds"]=monotonic_seconds()-start_;
    auto line=Json(std::move(fields)).dump();
    stream_<<line<<'\n';stream_.flush();
    if(!stream_) throw std::runtime_error("Trace write failed");
    std::cout<<line<<'\n';
}
Json::Object frame_record(const FrameSample& s,const Platform& platform,const Camera& camera,const Counters& c) {
    return {{"frame_id",s.id},{"raw_dt",s.step.raw},{"render_interval",s.step.raw},
        {"fixed_ticks",s.step.ticks},{"simulated_dt",s.step.simulated},{"dropped_dt",s.step.dropped},
        {"interpolation_alpha",s.step.alpha},{"event_cpu_ms",s.events_ms},
        {"physics_cpu_ms",s.physics_ms},{"physics_total_fixed_ticks",std::size_t(s.physics_ticks)},
        {"physics_live",Json::Array{s.physics_live.worlds,s.physics_live.shapes,s.physics_live.bodies}},
        {"physics_ground_distance",s.ground_distance},{"physics_overlaps",s.physics_overlaps},
        {"simulation_cpu_ms",s.simulation_ms},{"camera_tick_cpu_ms",s.camera_ms},
        {"render_prep_cpu_ms",s.prep_ms},{"gl_submission_cpu_ms",s.submit_ms},
        {"gl_completion_wait_cpu_ms",s.completion_ms},
        {"present_cpu_ms",s.present_ms},{"camera_position",json_vector(camera.position)},
        {"yaw",camera.yaw},{"pitch",camera.pitch},{"window_size",json_size(platform.window_size())},
        {"drawable_size",json_size(platform.drawable_size())},{"live_resources",json_resources(c)},
        {"mouse_captured",platform.input.captured},{"focused",platform.input.focused},
        {"mouse_capture_status_semantics","SDL window relative-mode request flag"},
        {"sdl_keyboard_focus",SDL_GetKeyboardFocus()==platform.window()},
        {"simulation_hz",60},{"gpu_timing","unavailable"}};
}
void Statistics::record(const FrameSample& f) {
    ++frames;ticks+=f.step.ticks;interval_sum+=f.step.raw;
    interval_worst=std::max(interval_worst,f.step.raw);dropped+=f.step.dropped;
    events+=f.events_ms;simulation+=f.simulation_ms;camera+=f.camera_ms;
    prep+=f.prep_ms;submit+=f.submit_ms;completion+=f.completion_ms;present+=f.present_ms;
}
Json Statistics::summary(double wall,double cpu) const {
    double count=static_cast<double>(std::max(std::size_t{1},frames));
    return Json::Object{{"render_frames",frames},{"simulation_ticks",ticks},{"wall_seconds",wall},
        {"render_fps",wall>0?static_cast<double>(frames)/wall:0},
        {"simulation_observed_hz",wall>0?static_cast<double>(ticks)/wall:0},
        {"frame_interval_mean_ms",interval_sum*1000/count},{"frame_interval_worst_ms",interval_worst*1000},
        {"event_mean_cpu_ms",events/count},{"simulation_mean_cpu_ms",simulation/count},
        {"camera_mean_cpu_ms_per_tick",camera/static_cast<double>(std::max(std::size_t{1},ticks))},
        {"render_prep_mean_cpu_ms",prep/count},{"gl_submission_mean_cpu_ms",submit/count},
        {"gl_completion_wait_mean_cpu_ms",completion/count},
        {"present_mean_cpu_ms",present/count},{"dropped_seconds",dropped},
        {"process_cpu_seconds",cpu},{"cpu_core_equivalents",wall>0?cpu/wall:0},
        {"peak_rss_kib",static_cast<double>(peak_rss_kib())},{"gpu_timing","unavailable"}};
}
long peak_rss_kib() {
#if defined(__linux__)
    rusage usage{};
    if(getrusage(RUSAGE_SELF,&usage)==0) return usage.ru_maxrss;
#endif
    return -1;
}
}
