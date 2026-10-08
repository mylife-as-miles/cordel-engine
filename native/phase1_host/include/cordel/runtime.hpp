// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/diagnostics.hpp"
#include "cordel/physics_runtime.hpp"

namespace cordel {
class FrameBoundary {
public:
    virtual ~FrameBoundary()=default;
    virtual void frame_begin(Platform&,Renderer&,Simulation&) = 0;
    virtual void fixed_tick(Renderer&,Simulation&) = 0;
    virtual bool paused() const = 0;
    virtual bool reset_clock() {return false;}
};
// Used unchanged by interactive runs and graphical self-tests. Simulation is
// explicitly upstream of rendering; Renderer never advances world state.
class FrameRunner {
    FixedClock clock_;
    double previous_time_{monotonic_seconds()};
    std::size_t frames_{};
public:
    PhysicsRuntime physics; // CPU world lifetime; independent of GL scene lifetime.
    Simulation simulation;
    bool last_forced_stall{};
    double last_movement{};
    FrameBoundary* boundary{}; // Optional, non-owning; ordinary Phase 1.2 uses null.
    FrameSample next(Platform& platform,Renderer& renderer);
    void reset(Camera camera={}) {
        simulation.reset(camera);clock_.reset();previous_time_=monotonic_seconds();frames_=0;
    }
};
}
