// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/json.hpp"
#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

namespace cordel::narrative {
inline constexpr const char* version="cordel.narrative/0.1";
inline constexpr std::size_t max_line=16384,queue_limit=64;
class Protocol {
    Json schema_;
public:
    explicit Protocol(const std::filesystem::path& schema);
    void validate(const Json&) const;
    Json decode(const std::string& line) const {auto m=Json::parse(line);validate(m);return m;}
};
// Thread-confined world consumes complete messages; I/O thread has no world/GL pointers.
class Client {
    Protocol protocol_;
    int pid_{-1},read_{-1},write_{-1};
    std::thread io_;
    mutable std::mutex mutex_;
    std::deque<Json> incoming_;
    std::deque<std::string> outgoing_;
    std::string error_;
    std::atomic<bool> stopping_{false},dead_{false};
    std::size_t sequence_{},peer_sequence_{},high_water_{},out_high_water_{};
    double death_time_{};
    int exit_status_{};
    bool forced_{};
    bool protocol_termination_{};
    void fail(std::string error);
    void loop();
public:
    Client(const std::filesystem::path& python,const std::filesystem::path& bootstrap,
           const std::filesystem::path& schema,const std::filesystem::path& output);
    ~Client();
    Client(const Client&)=delete;
    Client& operator=(const Client&)=delete;
    Json send(std::string type,Json::Object payload={},std::string session="control",std::string correlation="");
    bool enqueue(const Json& message);
    // Only used by the deterministic malformed/transport gate.
    bool send_raw(std::string line);
    std::vector<Json> drain(std::size_t limit=8);
    std::string error() const;
    bool dead() const {return dead_;}
    int pid() const {return pid_;}
    void terminate_for_test();
    // World-thread nonblocking failure policy; reaping occurs during host teardown.
    void terminate_protocol_failure();
    void close();
    Json diagnostics() const;
};
}
