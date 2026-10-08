// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/physics/fixture.hpp"
namespace cordel {
// Native world-owned query diagnostic. It deliberately does not constrain the
// Phase 1 free camera or render the collision-only fixture. No motor yet.
class PhysicsRuntime {
    physics::PhysicsWorld world_;
    physics::Fixture fixture_{physics::load_fixture(world_)};
    physics::PhysicsBodyId probe_{world_.create_static_body({fixture_.capsule,{0,2.5,6},{},physics::CollisionLayer::Character,"native_debug_probe"})};
public:
    std::optional<physics::RayHit> ground;
    std::size_t overlaps{},sensor_events{};
    void fixed_tick(physics::Vec3 center) {
        world_.set_position(probe_,center);
        world_.step();
        ground=world_.raycast({center,{0,-100,0},{}});
        overlaps=world_.overlap({fixture_.capsule,center,{}}).size();
        sensor_events=world_.take_contact_events().size();
    }
    std::uint64_t ticks() const {return world_.ticks();}
    physics::LiveCounts live() const {return world_.live();}
    const char* backend() const {return world_.backend_name();}
    void clear() {world_.clear();ground.reset();overlaps=sensor_events=0;}
};
}
