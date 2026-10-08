// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/physics/fixture.hpp"
namespace cordel {
// Native world-owned collision/query service. The default free-camera probe
// remains independent; explicit motor mode replaces it with the motor proxy.
class PhysicsRuntime {
    physics::PhysicsWorld world_;
    physics::Fixture fixture_{physics::load_fixture(world_)};
    physics::PhysicsBodyId probe_{world_.create_static_body({fixture_.capsule,{0,2.5,6},{},physics::CollisionLayer::Character,"native_debug_probe"})};
public:
    physics::PhysicsWorld& world() {return world_;}
    void disable_camera_probe() {if(world_.valid(probe_)) world_.destroy_body(probe_);}
    void motor_tick(physics::Vec3 center) {
        world_.step();ground=world_.raycast({center,{0,-100,0},{}});
        overlaps=world_.overlap({fixture_.capsule,center,{}}).size();
        sensor_events=world_.take_contact_events().size();
    }
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
