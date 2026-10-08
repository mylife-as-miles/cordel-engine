// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/json.hpp"
#include "cordel/platform.hpp"
#include "cordel/renderer.hpp"
#include <fstream>
#include "cordel/physics/types.hpp"

namespace cordel {
Json json_vector(Vec3 v);
Json json_size(std::array<int,2> size);
Json json_resources(const Counters& counters);
void write_json(const std::filesystem::path& path,const Json& value);
class Trace {
    std::ofstream stream_;
    double start_{monotonic_seconds()};
public:
    explicit Trace(const std::filesystem::path& directory);
    void event(std::string name,Json::Object fields={});
};
struct FrameSample {
    std::size_t id{};
    FixedStep step;
    double events_ms{},simulation_ms{},camera_ms{},prep_ms{},submit_ms{},completion_ms{},present_ms{};
    double physics_ms{};std::uint64_t physics_ticks{};physics::LiveCounts physics_live;
    double ground_distance{-1};std::size_t physics_overlaps{};
    double motor_ms{};std::uint64_t motor_ticks{};Vec3 motor_position{};bool motor_grounded{};
};
Json::Object frame_record(const FrameSample&,const Platform&,const Camera&,const Counters&);
struct Statistics {
    std::size_t frames{},ticks{};
    double interval_sum{},interval_worst{},dropped{};
    double events{},simulation{},camera{},prep{},submit{},completion{},present{};
    void record(const FrameSample& frame);
    Json summary(double wall,double process_cpu) const;
};
long peak_rss_kib();
void run_self_test(Platform&,Renderer&,Counters&,Trace&,
                   const std::filesystem::path& scene,const std::filesystem::path& output);
}
