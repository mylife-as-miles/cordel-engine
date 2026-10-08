// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/narrative_session.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <csignal>

namespace cordel::narrative {
namespace {
class Gate {
public:
    Json::Array checks,continuity,latency,cancellations,failures,lifecycle;
    Json checkpoint,backpressure;
    void check(bool ok,std::string name,Json detail={}) {
        checks.emplace_back(Json::Object{{"name",name},{"passed",ok},{"detail",detail}});
        if(!ok) throw std::runtime_error("Narrative gate failed: "+name+" "+detail.dump());
    }
};
class Probe {
    Platform& platform_;
    Renderer& renderer_;
    Trace& trace_;
    Gate& gate_;
    double next_log_{};
public:
    Client client;
    Session session;
    FrameRunner runner;
    std::size_t frames{},ticks{};
    double dropped{},worst{};
    FrameSample last;
    Probe(Platform& p,Renderer& r,Trace& t,Gate& g,const std::filesystem::path& python,
          const std::filesystem::path& output,const std::filesystem::path& bootstrap=CORDEL_NARRATIVE_BOOTSTRAP):
        platform_(p),renderer_(r),trace_(t),gate_(g),client(python,bootstrap,CORDEL_NARRATIVE_SCHEMA,output),session(client,t) {
        runner.boundary=&session;
        platform_.input.clear();platform_.input.focused=true;
    }
    ~Probe() {runner.boundary=nullptr;platform_.release();}
    void step() {
        last=runner.next(platform_,renderer_);++frames;ticks+=last.step.ticks;dropped+=last.step.dropped;worst=std::max(worst,last.step.raw);
        if(monotonic_seconds()>=next_log_) {
            next_log_=monotonic_seconds()+.5;
            session.log("world_frame",{{"frame_id",frames},{"ticks_this_frame",last.step.ticks},{"total_executed_ticks",ticks},
                {"raw_dt",last.step.raw},{"dropped_dt",last.step.dropped},{"interpolation_alpha",last.step.alpha},
                {"camera",json_vector(runner.simulation.current.position)},{"paused",session.world.paused}});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    void until(const std::function<bool()>& predicate,std::string name,double timeout=5) {
        double start=monotonic_seconds();
        while(!predicate()&&monotonic_seconds()-start<timeout) step();
        gate_.check(predicate(),std::move(name),Json::Object{{"status",session.status},{"failure",session.failure},{"transport",client.diagnostics()}});
    }
    void pending(std::string type) {until([&]{return session.pending_type()==type;},"wait for "+type);}
    void seen(std::string type) {until([&]{return std::any_of(session.observed.begin(),session.observed.end(),[&](const Json& m){return m.at("type").string()==type;});},"receive "+type);}
    Json last_message(std::string type) {
        for(auto it=session.observed.rbegin();it!=session.observed.rend();++it) if(it->at("type").string()==type) return *it;
        throw std::runtime_error("No observed "+type);
    }
    void hold(std::string type,double duration=.4,bool paused=false) {
        pending(type);auto initial_frames=frames,initial_ticks=ticks;
        auto initial_dropped=dropped;auto initial_physics_ticks=runner.physics.ticks();Vec3 before=runner.simulation.current.position;
        double start=monotonic_seconds(),local_worst=0;
        while(monotonic_seconds()-start<duration) {step();local_worst=std::max(local_worst,last.step.raw);}
        Json record=Json::Object{{"session_id",session.id},{"wait_type",type},{"wall_seconds",monotonic_seconds()-start},
            {"physics_ticks",std::size_t(runner.physics.ticks()-initial_physics_ticks)},{"simulation_ticks",ticks-initial_ticks},{"render_frames",frames-initial_frames},{"dropped_seconds",dropped-initial_dropped},
            {"worst_render_interval_ms",local_worst*1000},{"camera_distance",length(runner.simulation.current.position-before)},
            {"explicit_pause",paused},{"wait_still_pending",session.pending_type()==type}};
        gate_.continuity.push_back(record);session.log("continuity_sample",{{"sample",record}});
        gate_.check(frames-initial_frames>=10,"render continues during "+type,record);
        gate_.check(paused?ticks==initial_ticks:ticks-initial_ticks>=12,"simulation policy during "+type,record);
        gate_.check(runner.physics.ticks()-initial_physics_ticks==ticks-initial_ticks,"physics follows native ticks during "+type,record);
        gate_.check(session.pending_type()==type,"wait remains unacknowledged during continuity sample");
    }
    void handshake() {until([&]{return session.ready();},"headless RenPy handshake");}
    void key(SDL_Scancode code,bool down=true) {
        SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
        event.key.windowID=SDL_GetWindowID(platform_.window());event.key.scancode=code;
        gate_.check(SDL_PushEvent(&event),"queue narrative/gameplay keyboard event");
    }
    void story_to_choice(std::string id) {
        session.start(std::move(id));pending("dialogue");session.acknowledge();pending("wait_for_event");
        session.event();pending("dialogue");session.acknowledge();pending("choice");
    }
    void finish_story(std::string branch,bool keyboard=false) {
        if(keyboard) key(branch=="continue"?SDL_SCANCODE_1:SDL_SCANCODE_2);else session.select(branch);
        pending("dialogue");
        if(keyboard) key(SDL_SCANCODE_RETURN);else session.acknowledge();
        until([&]{return session.status=="completed";},"story completes");
        auto payload=last_message("session_completed").at("payload");
        gate_.check(payload.at("outcome").string()==branch,"branch outcome "+branch);
        gate_.check(payload.at("counter").number()==(branch=="continue"?1:2),"RenPy variable mutation "+branch);
        gate_.check(payload.at("beacon_fact").boolean(),"typed world fact reached narrative");
    }
    void collect_latency() {
        for(const auto& sample:session.latency_samples) gate_.latency.push_back(sample);
        session.latency_samples.clear();
        session.observed.clear();
    }
    void shutdown() {
        int pid=client.pid();client.close();auto d=client.diagnostics();
        gate_.check(d.at("worker_reaped").boolean(),"worker reaped at shutdown",d);
        gate_.check(!d.at("forced_termination").boolean(),"shutdown needs no additional forced kill",d);
        gate_.check(kill(pid,0)<0&&errno==ESRCH,"no orphan narrative process");
        gate_.lifecycle.push_back(d);
    }
};
void normal_tests(Platform& p,Renderer& r,Trace& trace,Gate& gate,const std::filesystem::path& python,const std::filesystem::path& out) {
    Probe probe(p,r,trace,gate,python,out/"normal");probe.handshake();
    // Same scene starts with a hidden debug beacon; command activates real draw state.
    gate.check(r.set_visible("tall_gold",false),"native beacon target exists");
    r.render(Camera{},850,480);r.complete_frame();auto hidden=r.read_pixels(850,480);
    probe.session.start("story-continue");probe.pending("dialogue");
    p.input.focused=true;probe.key(SDL_SCANCODE_W);
    probe.hold("dialogue");gate.check(length(probe.runner.simulation.current.position-Camera{}.position)>.5,"camera continues during dialogue");
    p.input.clear();probe.runner.simulation.reset();
    auto opening=probe.session.pending_id();
    probe.session.send("dialogue_ack",{},"unknown-correlation");
    probe.until([&]{return probe.session.errors>=1;},"unknown correlation rejected");
    probe.key(SDL_SCANCODE_RETURN);probe.pending("wait_for_event");
    gate.check(probe.session.world.beacon_enabled&&probe.session.world.commands_applied==1,"world command applied once at tick boundary");
    r.render(Camera{},850,480);r.complete_frame();auto visible=r.read_pixels(850,480);
    gate.check(visible!=hidden,"command changes actual rendered geometry");
    probe.session.send("dialogue_ack",{},opening); // Duplicate ack must not advance event wait.
    probe.session.event("unrelated_event");
    probe.client.send("dialogue_ack",{},"stale-session",opening);
    probe.client.send_raw("{\"protocol_version\":\"cordel.narrative/0.1\",\"session_id\":\"story-continue\",\"message_id\":\"old\",\"sequence\":1,\"type\":\"dialogue_ack\",\"payload\":{},\"correlation_id\":\"old\"}\n");
    probe.until([&]{return probe.session.errors>=4;},"duplicate ack and unrelated event rejected");
    probe.hold("wait_for_event");probe.session.event();auto resolved=probe.last_message("wait_for_event");
    probe.pending("dialogue");
    probe.session.send("gameplay_event",{{"event_id","beacon_reached"},{"simulation_tick",probe.session.world.tick},{"watermark",probe.session.world.event_watermark}},resolved.at("message_id").string());
    probe.session.acknowledge();probe.pending("choice");
    p.input.focused=true;probe.key(SDL_SCANCODE_W);auto before_choice=probe.runner.simulation.current.position;
    probe.hold("choice");gate.check(length(probe.runner.simulation.current.position-before_choice)>.5,"WASD remains gameplay-owned during choice");
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;focus.window.windowID=SDL_GetWindowID(p.window());
    gate.check(SDL_PushEvent(&focus),"queue focus loss during narrative choice");probe.step();
    gate.check(p.input.actions.empty()&&probe.session.input_mode==InputMode::NarrativeChoice,"focus clears gameplay without stealing narrative mode");
    focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;gate.check(SDL_PushEvent(&focus),"queue focus regain");probe.step();
    auto errors=probe.session.errors;probe.session.select("invalid");
    probe.until([&]{return probe.session.errors>errors;},"invalid stable choice rejected");
    probe.finish_story("continue",true);probe.collect_latency();
    for(int cycle=0;cycle<4;++cycle) {
        probe.session.observed.clear();probe.story_to_choice("choice-stay-"+std::to_string(cycle));
        probe.finish_story("stay");probe.collect_latency();
    }
    // Real worker deliberately re-emits exactly the command ID with a newer envelope sequence.
    auto prior=probe.session.world.commands_applied;
    probe.session.start("duplicate-command","replay");probe.pending("dialogue");probe.session.acknowledge();probe.pending("wait_for_event");
    gate.check(probe.session.ignored>=1&&probe.session.world.commands_applied==prior+1,"duplicate command did not repeat world side effect");
    probe.session.event();probe.pending("dialogue");probe.session.acknowledge();probe.pending("choice");probe.finish_story("continue");probe.collect_latency();
    for(const auto& wait:{"dialogue","wait_for_event","choice"}) {
        probe.session.observed.clear();std::string id="cancel-"+std::string(wait);
        probe.session.start(id);probe.pending("dialogue");
        if(std::string(wait)!="dialogue") {probe.session.acknowledge();probe.pending("wait_for_event");}
        if(std::string(wait)=="choice") {probe.session.event();probe.pending("dialogue");probe.session.acknowledge();probe.pending("choice");}
        auto stale=probe.session.pending_id();auto frames=probe.frames,ticks=probe.ticks;
        probe.session.cancel();probe.until([&]{return probe.session.status=="cancelled";},"cancel during "+std::string(wait));
        gate.check(probe.session.input_mode==InputMode::Gameplay&&probe.session.pending_type().empty(),"cancellation releases ownership");
        auto effects=probe.session.world.commands_applied;
        probe.client.send("dialogue_ack",{},id,stale);probe.step();probe.step();
        gate.check(probe.session.status=="cancelled"&&probe.session.pending_type().empty()&&
            probe.session.world.commands_applied==effects,"late acknowledgement cannot resurrect cancelled session");
        gate.cancellations.emplace_back(Json::Object{{"wait",wait},{"status",probe.session.status},{"pending_count",0},
            {"additional_frames",probe.frames-frames},{"additional_ticks",probe.ticks-ticks},{"late_ack_ignored",true}});
        bool rejected=false;
        try {probe.session.start(id);} catch(const std::invalid_argument&) {rejected=true;}
        gate.check(rejected,"native rejects retired session ID reuse");
    }
    probe.session.observed.clear();probe.story_to_choice("checkpoint");
    probe.session.send("checkpoint_request");probe.until([&]{return probe.session.has_checkpoint;},"checkpoint capture");
    Json captured=probe.session.checkpoint;auto saved=probe.session.saved_world;auto camera=probe.session.saved_camera;
    gate.check(captured.at("outcome").string()=="unset"&&captured.at("counter").number()==0,"safe narrative checkpoint values");
    probe.session.select("continue");probe.pending("dialogue");
    probe.session.world.beacon_enabled=false;r.set_visible("tall_gold",false);
    probe.runner.simulation.current.position.x+=4;probe.session.world.event_watermark+=9;
    probe.session.send("checkpoint_restore",{{"checkpoint",captured}});
    probe.pending("choice"); // Restore unwinds worker Context and resumes only the named safe label.
    gate.check(probe.session.world.beacon_enabled&&probe.session.world.event_watermark==saved.event_watermark,"native checkpoint restore");
    gate.check(length(probe.runner.simulation.current.position-camera.position)<1e-8,"camera checkpoint restore");
    std::erase_if(probe.session.observed,[](const Json& m){return m.at("type").string()=="checkpoint_data";});
    probe.session.send("checkpoint_request");probe.seen("checkpoint_data");
    gate.check(probe.last_message("checkpoint_data").at("payload").at("checkpoint").dump()==captured.dump(),"fresh narrative checkpoint restore values");
    probe.session.send("rollback_request");probe.seen("rollback_rejected");
    gate.check(probe.session.world.commands_applied==saved.commands_applied,"restore never replays beacon command");
    gate.check(probe.last_message("rollback_rejected").at("payload").at("reason").string().find("world-effect")!=std::string::npos,"rollback stops at world-effect boundary");
    gate.checkpoint=Json::Object{{"captured",captured},{"native_beacon_restored",true},{"camera_restored",true},
        {"event_watermark",saved.event_watermark},{"world_tick_at_capture",saved.tick},{"restored_native_tick_then_advanced",probe.session.world.tick},
        {"narrative_restored",true},{"no_command_replay",true},{"rollback_rejected",true}};
    probe.finish_story("stay");probe.collect_latency();
    for(const auto& scenario:{"exception","invalid_target"}) {
        probe.session.observed.clear();probe.session.start("failure-"+std::string(scenario),scenario);
        if(std::string(scenario)=="exception") {probe.pending("dialogue");probe.session.acknowledge();}
        probe.until([&]{return probe.session.status=="failed";},"controlled narrative failure "+std::string(scenario));
        auto ticks=probe.ticks,frames=probe.frames;for(int i=0;i<20;++i) probe.step();
        gate.check(probe.ticks>ticks+10&&probe.frames>=frames+20,"world survives narrative exception/rejection");
        gate.check(probe.session.input_mode==InputMode::Gameplay&&probe.session.pending_type().empty(),"failure releases narrative waits");
        gate.failures.emplace_back(Json::Object{{"scenario",scenario},{"error",probe.session.failure},{"additional_ticks",probe.ticks-ticks},{"additional_frames",probe.frames-frames}});
    }
    probe.session.observed.clear();probe.session.start("explicit-pause","pause");probe.pending("dialogue");
    gate.check(probe.session.world.paused,"explicit world pause applied");probe.hold("dialogue",.4,true);
    probe.session.acknowledge();probe.pending("dialogue");
    gate.check(!probe.session.world.paused,"resume applied at paused world-thread control boundary");
    probe.hold("dialogue");gate.check(probe.last.step.ticks<=3&&probe.last.step.dropped==0,"resume has no accumulated pause debt");
    probe.session.acknowledge();probe.until([&]{return probe.session.status=="completed";},"pause story completes");probe.collect_latency();
    probe.shutdown();
}
void death_test(Platform& p,Renderer& r,Trace& trace,Gate& gate,const std::filesystem::path& python,const std::filesystem::path& out) {
    Probe probe(p,r,trace,gate,python,out/"worker-death");probe.handshake();probe.session.start("worker-death");probe.pending("dialogue");
    double killed=monotonic_seconds();probe.client.terminate_for_test();probe.until([&]{return probe.session.status=="failed";},"detect worker death");
    double detection=(monotonic_seconds()-killed)*1000;auto frames=probe.frames,ticks=probe.ticks;
    for(int i=0;i<20;++i) probe.step();
    gate.check(probe.ticks>ticks+10&&probe.frames>=frames+20,"native world survives killed worker");
    gate.failures.emplace_back(Json::Object{{"scenario","worker_death"},{"detection_ms",detection},
        {"additional_ticks",probe.ticks-ticks},{"additional_frames",probe.frames-frames},{"ownership_released",probe.session.input_mode==InputMode::Gameplay}});
    probe.shutdown();
}
void shutdown_wait_tests(Platform& p,Renderer& r,Trace& trace,Gate& gate,const std::filesystem::path& python,const std::filesystem::path& out) {
    Probe probe(p,r,trace,gate,python,out/"shutdown-wait");probe.handshake();probe.session.start("shutdown-wait");probe.pending("dialogue");
    probe.shutdown();gate.check(true,"native shutdown while worker waits leaves no orphan");
}
void stress_test(Platform& p,Renderer& r,Trace& trace,Gate& gate,const std::filesystem::path& python,const std::filesystem::path& out) {
    Probe probe(p,r,trace,gate,python,out/"backpressure");probe.handshake();long before=peak_rss_kib();
    probe.session.start("finite-flood","flood");probe.until([&]{return probe.session.status=="failed";},"finite worker flood fails bounded queue");
    auto diagnostics=probe.client.diagnostics();auto ticks=probe.ticks,frames=probe.frames;
    for(int i=0;i<20;++i) probe.step();
    gate.check(diagnostics.at("incoming_high_water").number()<=queue_limit,"incoming queue never exceeds 64",diagnostics);
    gate.check(diagnostics.at("error").string().find("overflow")!=std::string::npos,"backpressure identifies overflow");
    gate.check(probe.ticks>ticks+10,"world continues after queue overflow");
    int pid=probe.client.pid();double start=monotonic_seconds();probe.client.close();
    gate.check(kill(pid,0)<0&&errno==ESRCH,"flood worker has no orphan");
    gate.backpressure=Json::Object{{"transport",probe.client.diagnostics()},{"finite_emission_limit",4096},
        {"rss_before_kib",double(before)},{"rss_after_kib",double(peak_rss_kib())},{"cleanup_seconds",monotonic_seconds()-start},
        {"additional_ticks",probe.ticks-ticks},{"additional_frames",probe.frames-frames},
        {"memory_policy","64 messages per queue, 16KiB each, one partial read and write; overflow ends transport"}};
}
void malformed_tests(Platform& p,Renderer& r,Trace& trace,Gate& gate,const std::filesystem::path& python,const std::filesystem::path& out) {
    auto script=std::filesystem::path(CORDEL_NARRATIVE_BOOTSTRAP).parent_path().parent_path()/"tests/adversarial_worker.py";
    for(const auto& kind:{"invalid_json","oversized","wrong_version","unknown_type","sequence","payload"}) {
        setenv("CORDEL_ADVERSARY",kind,1);
        Probe probe(p,r,trace,gate,python,out/("malformed-"+std::string(kind)),script);unsetenv("CORDEL_ADVERSARY");
        probe.until([&]{return probe.session.status=="failed";},"native rejects malformed worker "+std::string(kind));
        auto ticks=probe.ticks;for(int i=0;i<5;++i) probe.step();
        gate.check(probe.ticks>ticks,"world continues after malformed protocol");
        int pid=probe.client.pid();probe.client.close();gate.check(kill(pid,0)<0&&errno==ESRCH,"malicious peer reaped");
        gate.failures.emplace_back(Json::Object{{"scenario","malformed_"+std::string(kind)},{"error",probe.session.failure},{"transport",probe.client.diagnostics()}});
    }
}
}
void run_narrative(Platform& p,Renderer& r,Counters& counters,Trace& trace,const std::filesystem::path& scene,
                   const std::filesystem::path& output,const std::filesystem::path& python,bool self_test,double seconds) {
    r.load(scene);SDL_GL_SetSwapInterval(0);Gate gate;
    trace.event("native_started",{{"milestone","Phase 1.3 Narrative Ownership"},{"worker_transport","nonblocking anonymous pipes; dedicated I/O thread"},
        {"fixed_hz",60},{"native_authoritative",true},{"gpu_timing","unavailable"}});
    try {
        if(self_test) {
            gate.check(r.scene().meshes.size()==7&&r.scene().triangles()==84,"unchanged seven-mesh/84-triangle fixture");
            normal_tests(p,r,trace,gate,python,output);death_test(p,r,trace,gate,python,output);
            shutdown_wait_tests(p,r,trace,gate,python,output);stress_test(p,r,trace,gate,python,output);
            malformed_tests(p,r,trace,gate,python,output);
        } else {
            Probe probe(p,r,trace,gate,python,output/"interactive");probe.handshake();r.set_visible("tall_gold",false);
            probe.session.console=true;probe.session.start("interactive-story");double start=monotonic_seconds();
            while(!p.input.quit&&(seconds<=0||monotonic_seconds()-start<seconds)) {
                probe.step();
                if(probe.session.pending_type()=="wait_for_event") {
                    // Native world condition: camera within 2 m of the authored beacon.
                    if(length(probe.runner.simulation.current.position-Vec3{3,2.5,-5})<2) probe.session.event();
                }
                p.title("CORDEL Phase 1.3 | "+std::string(mode_name(probe.session.input_mode))+" | Enter / 1 / 2 | WASD remains gameplay-owned");
            }
            probe.shutdown();
        }
        p.release();r.unload();gate.check(counters.empty(),"native resources still zero after narrative shutdown");
        write_json(output/"scenario-results.json",Json::Object{{"success",true},{"checks",gate.checks},{"check_count",gate.checks.size()}});
        write_json(output/"continuity-results.json",gate.continuity);write_json(output/"latency-samples.json",gate.latency);
        write_json(output/"cancellation-results.json",gate.cancellations);write_json(output/"checkpoint-results.json",gate.checkpoint);
        write_json(output/"failure-results.json",gate.failures);write_json(output/"backpressure-results.json",gate.backpressure);
        write_json(output/"worker-lifecycle.json",gate.lifecycle);
        trace.event("shutdown",{{"success",true},{"checks",gate.checks.size()},{"resources",json_resources(counters)}});
    } catch(...) {
        p.release();r.unload();
        write_json(output/"scenario-results.json",Json::Object{{"success",false},{"checks",gate.checks}});throw;
    }
}
}
