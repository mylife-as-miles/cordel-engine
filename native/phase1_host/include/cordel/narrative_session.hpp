// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/narrative.hpp"
#include "cordel/runtime.hpp"
#include <set>

namespace cordel::narrative {
enum class InputMode {Gameplay,NarrativeAcknowledge,NarrativeChoice};
const char* mode_name(InputMode);
struct WorldState {
    bool beacon_enabled{},paused{};
    std::size_t tick{},event_watermark{},fact_revision{},commands_applied{};
};
class Session final:public FrameBoundary {
    Client& client_;
    Trace& trace_;
    std::string start_id_,hello_id_;
    std::set<std::string> received_,used_sessions_;
    std::map<std::string,Json> command_results_;
    std::deque<Json> commands_;
    std::map<std::string,std::string> host_requests_;
    std::map<std::string,double> reply_times_;
    bool ready_{},release_input_{};
    bool capture_checkpoint_{},restore_checkpoint_{},reset_clock_{};
    void accept(const Json&);
    void apply(const Json&,Renderer&);
    void finish(std::string status,std::string reason="");
public:
    WorldState world;
    std::string id{"control"},status{"idle"},failure;
    InputMode input_mode{InputMode::Gameplay};
    Json pending;
    std::deque<Json> observed;
    std::deque<Json> latency_samples;
    Json checkpoint;
    WorldState saved_world;
    Camera saved_camera;
    bool has_checkpoint{};
    bool console{};
    std::size_t ignored{},errors{};
    Session(Client&,Trace&);
    bool ready() const {return ready_;}
    void start(std::string session_id,std::string scenario="story");
    Json send(std::string type,Json::Object payload={},std::string correlation="");
    void acknowledge();
    void select(std::string choice);
    void event(std::string name="beacon_reached",std::string correlation="");
    void cancel();
    void frame_begin(Platform&,Renderer&,Simulation&) override;
    void fixed_tick(Renderer&,Simulation&) override;
    bool paused() const override {return world.paused;}
    bool reset_clock() override {return std::exchange(reset_clock_,false);}
    std::string pending_type() const {return pending.is("object")?pending.at("type").string():"";}
    std::string pending_id() const {return pending.is("object")?pending.at("message_id").string():"";}
    void log(std::string event,Json::Object fields={});
};
void run_narrative(Platform&,Renderer&,Counters&,Trace&,const std::filesystem::path& scene,
    const std::filesystem::path& output,const std::filesystem::path& python,bool self_test,double seconds,bool motor=false);
}
