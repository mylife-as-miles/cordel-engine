// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "queries.hpp"
#include <memory>
namespace cordel::physics {
// Single authoritative thread. Backend internals never escape this boundary.
class PhysicsWorld {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    static constexpr double fixed_dt=1./60.;
    PhysicsWorld();~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&)=delete;PhysicsWorld& operator=(const PhysicsWorld&)=delete;
    const char* backend_name() const;
    PhysicsShapeId create_shape(const ShapeDesc&);
    PhysicsBodyId create_static_body(const BodyDesc&);
    void destroy_body(PhysicsBodyId);
    void destroy_shape(PhysicsShapeId); // Reject while used by a body.
    void set_position(PhysicsBodyId,Vec3); // Debug probe repositioning; not a character motor.
    bool valid(PhysicsBodyId) const;bool valid(PhysicsShapeId) const;
    std::optional<RayHit> raycast(const RayQuery&) const;
    std::optional<CapsuleCastHit> cast_capsule(const CapsuleCastQuery&) const;
    std::vector<OverlapHit> overlap(const OverlapQuery&) const;
    void step(double fixed_delta=fixed_dt);
    std::vector<ContactEvent> take_contact_events(); // Query-derived capsule/sensor pairs.
    std::uint64_t ticks() const;
    LiveCounts live() const;static LiveCounts global_live();
    // Expiration guard for non-owning runtime services; authoritative-thread only.
    std::weak_ptr<const void> lifetime_token() const;
    void clear();
};
}
