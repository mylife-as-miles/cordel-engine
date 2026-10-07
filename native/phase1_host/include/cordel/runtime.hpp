// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/diagnostics.hpp"

namespace cordel {
// Used unchanged by interactive runs and graphical self-tests. Simulation is
// explicitly upstream of rendering; Renderer never advances world state.
class FrameRunner {
    FixedClock clock_;
    double previous_time_{monotonic_seconds()};
    std::size_t frames_{};
public:
    Simulation simulation;
    bool last_forced_stall{};
    double last_movement{};
    FrameSample next(Platform& platform,Renderer& renderer);
    void reset(Camera camera={}) {
        simulation.reset(camera);clock_.reset();previous_time_=monotonic_seconds();frames_=0;
    }
};
}
