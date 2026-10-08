// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "types.hpp"
#include <optional>
namespace cordel::physics {
struct RayQuery {Vec3 origin,displacement;QueryFilter filter;};
struct RayHit {
    PhysicsBodyId body;Vec3 point,normal;double fraction{},distance{};
    std::optional<std::uint32_t> subshape;bool started_inside{};
};
struct CapsuleCastQuery {PhysicsShapeId shape;Vec3 center,displacement;QueryFilter filter;};
struct CapsuleCastHit : RayHit {double penetration{};};
struct OverlapQuery {PhysicsShapeId shape;Vec3 center;QueryFilter filter;};
struct OverlapHit {PhysicsBodyId body;Vec3 point,normal;double penetration{};std::optional<std::uint32_t> subshape;};
}
