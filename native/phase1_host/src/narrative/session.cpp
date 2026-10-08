// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/narrative_session.hpp"
#include <iostream>

namespace cordel::narrative {
const char* mode_name(InputMode m) {
    switch(m) {case InputMode::Gameplay:return "Gameplay";case InputMode::NarrativeAcknowledge:return "NarrativeAcknowledge";case InputMode::NarrativeChoice:return "NarrativeChoice";}
    return "invalid";
}
Session::Session(Client& client,Trace& trace):client_(client),trace_(trace) {
    hello_id_=client_.send("hello",{{"client","CORDEL ENGINE 0.1.0-dev"}}).at("message_id").string();
}
void Session::log(std::string event,Json::Object fields) {
    fields["session_id"]=id;fields["simulation_tick"]=world.tick;
    fields["input_mode"]=mode_name(input_mode);fields["native_monotonic"]=monotonic_seconds();
    trace_.event(std::move(event),std::move(fields));
}
void Session::start(std::string session_id,std::string scenario) {
    if(!ready_||status=="active"||status=="starting"||status=="cancelling") throw std::logic_error("Session start requires idle handshake");
    if(session_id=="control"||used_sessions_.contains(session_id)||used_sessions_.size()>=128)
        throw std::invalid_argument("Fresh session ID required; worker ledger limited to 128 sessions");
    used_sessions_.insert(session_id);
    id=std::move(session_id);status="starting";failure.clear();received_.clear();command_results_.clear();
    commands_.clear();host_requests_.clear();pending=Json();input_mode=InputMode::Gameplay;world.paused=false;
    reply_times_.clear();
    has_checkpoint=false;checkpoint=Json();capture_checkpoint_=restore_checkpoint_=reset_clock_=false;
    start_id_=send("start_session",{{"scenario",std::move(scenario)}}).at("message_id").string();
}
Json Session::send(std::string type,Json::Object payload,std::string correlation) {
    if(!correlation.empty()&&(type=="dialogue_ack"||type=="choice_result"||type=="command_result"||type=="gameplay_event")) {
        if(reply_times_.size()==64) reply_times_.erase(reply_times_.begin());
        reply_times_[correlation]=monotonic_seconds();
    }
    Json m=client_.send(type,std::move(payload),id,std::move(correlation));
    if(type=="checkpoint_request"||type=="checkpoint_restore"||type=="rollback_request"||type=="cancel_session") {
        if(host_requests_.size()>=32) {finish("failed","too many outstanding requests");return m;}
        host_requests_[m.at("message_id").string()]=type;
    }
    log("native_send",{{"message",m}});return m;
}
void Session::finish(std::string state,std::string reason) {
    if(state=="failed"&&status=="active"&&!client_.dead()) client_.send("cancel_session",{},id);
    status=std::move(state);failure=std::move(reason);pending=Json();commands_.clear();host_requests_.clear();
    input_mode=InputMode::Gameplay;world.paused=false;release_input_=true;
    log("session_"+status,{{"reason",failure},{"pending_count",0}});
}
void Session::acknowledge() {
    if(pending_type()!="dialogue"||status!="active") return;
    send("dialogue_ack",{},pending_id());log("dialogue_acknowledged",{{"correlation_id",pending_id()}});
    pending=Json();input_mode=InputMode::Gameplay;
}
void Session::select(std::string choice) {
    if(pending_type()!="choice"||status!="active") return;
    send("choice_result",{{"choice_id",choice}},pending_id());
    log("choice_selected",{{"choice_id",choice},{"correlation_id",pending_id()}});
    if(choice=="continue"||choice=="stay") {pending=Json();input_mode=InputMode::Gameplay;}
}
void Session::event(std::string name,std::string correlation) {
    if(pending_type()!="wait_for_event"||status!="active") return;
    if(correlation.empty()) correlation=pending_id();
    bool correct=name=="beacon_reached"&&correlation==pending_id()&&world.beacon_enabled;
    send("gameplay_event",{{"event_id",name},{"simulation_tick",world.tick},
        {"watermark",correct?++world.event_watermark:world.event_watermark}},correlation);
    log("gameplay_event_sent",{{"event_id",name},{"correlation_id",correlation},{"correct_condition",correct}});
    if(correct) pending=Json();
}
void Session::cancel() {
    if(status!="active"&&status!="starting") return;
    send("cancel_session");status="cancelling";pending=Json();commands_.clear();input_mode=InputMode::Gameplay;
    world.paused=false;release_input_=true;log("session_cancel_requested");
}
void Session::accept(const Json& m) {
    const auto& type=m.at("type").string();auto session=m.at("session_id").string();
    auto correlation=m.contains("correlation_id")?m.at("correlation_id").string():"";
    if(type=="hello_ack") {
        if(session!="control"||correlation!=hello_id_||ready_||m.at("payload").at("display_started").boolean()) {
            finish("failed","invalid/headful worker handshake");return;
        }
        ready_=true;log("handshake",{{"message",m}});return;
    }
    if(session!=id||status=="completed"||status=="cancelled"||status=="failed") {++ignored;log("stale_message_ignored",{{"message",m}});return;}
    if(type=="error") {
        ++errors;log("worker_error",{{"message",m}});
        if(m.at("payload").at("fatal").boolean()) finish("failed",m.at("payload").at("detail").string());
        return;
    }
    auto message_id=m.at("message_id").string();
    if(received_.contains(message_id)) {
        ++ignored;
        if(type=="narrative_command"&&command_results_.contains(message_id))
            send("command_result",command_results_.at(message_id).at("payload").object(),message_id);
        log("duplicate_message_ignored",{{"message_id",message_id}});return;
    }
    if(received_.size()>=256) {finish("failed","session message ledger bound exceeded");return;}
    received_.insert(message_id);
    if(type=="session_started") {
        if(correlation!=start_id_||status!="starting") {finish("failed","bad start correlation");return;}
        status="active";log("session_started",{{"message",m}});
    } else if(type=="session_completed") {
        if(correlation!=start_id_) {finish("failed","bad completion correlation");return;}
        finish("completed");
    } else if(type=="session_cancelled"||type=="checkpoint_data"||type=="checkpoint_restored"||type=="rollback_rejected") {
        std::string expected=type=="session_cancelled"?"cancel_session":type=="checkpoint_data"?"checkpoint_request":type=="checkpoint_restored"?"checkpoint_restore":"rollback_request";
        if(!host_requests_.contains(correlation)||host_requests_.at(correlation)!=expected) {++ignored;log("unknown_correlation",{{"message",m}});return;}
        host_requests_.erase(correlation);
        if(type=="session_cancelled") finish("cancelled");
        if(type=="checkpoint_data") {checkpoint=m.at("payload").at("checkpoint");capture_checkpoint_=true;log("checkpoint_created",{{"message",m}});}
        if(type=="checkpoint_restored") {
            if(!has_checkpoint||m.at("payload").at("checkpoint").dump()!=checkpoint.dump()) {finish("failed","checkpoint restore mismatch");return;}
            restore_checkpoint_=true;pending=Json();input_mode=InputMode::Gameplay;log("checkpoint_restored",{{"message",m}});
        }
        if(type=="rollback_rejected") log("rollback_rejected",{{"message",m}});
    } else if(type=="dialogue"||type=="choice"||type=="wait_for_event"||type=="narrative_command") {
        if(status!="active") {++ignored;return;}
        if(pending.is("object")) {finish("failed","worker issued overlapping waits");return;}
        if(type=="choice") {
            const auto& choices=m.at("payload").at("choices").array();
            if(choices.size()!=2||!choices[0].contains("id")||!choices[1].contains("id")||
               !choices[0].at("id").is("string")||!choices[1].at("id").is("string")||
               choices[0].at("id").string()!="continue"||choices[1].at("id").string()!="stay") {
                finish("failed","invalid fixture choice shape");return;
            }
        }
        pending=m;
        input_mode=type=="dialogue"?InputMode::NarrativeAcknowledge:type=="choice"?InputMode::NarrativeChoice:InputMode::Gameplay;
        if(type=="narrative_command") commands_.push_back(m);
        log(type=="wait_for_event"?"wait_started":type+"_received",{{"message",m},{"native_receive_time",monotonic_seconds()}});
        if(console&&(type=="dialogue"||type=="choice")) std::cout<<"CORDEL narrative: "<<m.at("payload").dump()<<" | Enter: acknowledge; 1/2: choose; WASD: camera; F9: cancel\n";
    } else if(type=="resumed") {
        Json::Object sample=m.at("payload").object();sample["session_id"]=id;sample["correlation_id"]=correlation;
        if(reply_times_.contains(correlation)) {
            sample["native_ack_to_resume_observed_ms"]=(monotonic_seconds()-reply_times_.at(correlation))*1000;
            reply_times_.erase(correlation);
        }
        if(latency_samples.size()==128) latency_samples.pop_front();
        latency_samples.emplace_back(sample);log("wait_resumed",{{"message",m},{"latency",sample}});
    }
    else if(type=="diagnostic") log("diagnostic_received",{{"index",m.at("payload").at("index")}});
    else {finish("failed","unexpected worker message direction/type");}
}
void Session::apply(const Json& m,Renderer& renderer) {
    const auto& p=m.at("payload");const auto& command=p.at("command").string();const auto& target=p.at("target").string();
    bool success=false;
    if(command=="set_beacon_enabled"&&target=="tall_gold") {
        success=renderer.set_visible(target,p.at("value").boolean());
        if(success) {world.beacon_enabled=p.at("value").boolean();++world.fact_revision;++world.commands_applied;}
    } else if(command=="pause_world"&&target=="world") {world.paused=true;success=true;}
    else if(command=="resume_world"&&target=="world") {world.paused=false;success=true;}
    auto corr=m.at("message_id").string();
    auto ack=send("command_result",{{"success",success},{"simulation_tick",world.tick}},corr);
    command_results_.emplace(corr,ack);pending=Json();
    log("command_applied",{{"command",command},{"target",target},{"success",success},{"beacon_enabled",world.beacon_enabled},
        {"command_id",corr},{"apply_count",world.commands_applied},{"paused",world.paused}});
    if(success&&command=="set_beacon_enabled") send("world_fact",{{"fact_id","beacon_enabled"},{"revision",world.fact_revision},
        {"value_type","boolean"},{"value",world.beacon_enabled},{"simulation_tick",world.tick}});
}
void Session::frame_begin(Platform& platform,Renderer& renderer,Simulation& simulation) {
    auto incoming=client_.drain();
    for(const auto& m:incoming) {
        log("native_receive",{{"message",m}});
        if(observed.size()==256) observed.pop_front();
        observed.push_back(m);
        try {accept(m);} catch(const std::exception& e) {finish("failed",std::string("semantic validation: ")+e.what());}
    }
    if(capture_checkpoint_) {saved_world=world;saved_camera=simulation.current;has_checkpoint=true;capture_checkpoint_=false;}
    if(restore_checkpoint_) {
        world=saved_world;simulation.reset(saved_camera);renderer.set_visible("tall_gold",world.beacon_enabled);
        restore_checkpoint_=false;reset_clock_=true;
        log("world_checkpoint_restored",{{"beacon_enabled",world.beacon_enabled},{"event_watermark",world.event_watermark},{"camera",json_vector(saved_camera.position)}});
    }
    if((client_.dead()||!client_.error().empty())&&status!="failed"&&status!="cancelled"&&status!="completed") {
        if(!client_.error().empty()) client_.terminate_protocol_failure();
        finish("failed",client_.error().empty()?"worker EOF/process death":client_.error());log("worker_exit_detected",{{"transport",client_.diagnostics()}});
    }
    if(release_input_) {platform.release();platform.narrative_choice=0;platform.narrative_ack=false;platform.narrative_cancel=false;release_input_=false;}
    // Paused simulation still has a world-thread control boundary for resume/cancel.
    if(world.paused&&!commands_.empty()) {auto m=commands_.front();commands_.pop_front();apply(m,renderer);}
    if(std::exchange(platform.narrative_ack,false)) acknowledge();
    int choice=std::exchange(platform.narrative_choice,0);if(choice) select(choice==1?"continue":"stay");
    if(std::exchange(platform.narrative_cancel,false)) cancel();
}
void Session::fixed_tick(Renderer& renderer,Simulation&) {
    ++world.tick;
    if(!commands_.empty()) {auto m=commands_.front();commands_.pop_front();apply(m,renderer);}
}
}
