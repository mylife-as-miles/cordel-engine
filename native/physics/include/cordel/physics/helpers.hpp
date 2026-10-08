// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "world.hpp"
#include <algorithm>
#include <numbers>
#include <stdexcept>
namespace cordel::physics {
inline double slope_angle(Vec3 normal) {
    if(!std::isfinite(length(normal))||length(normal)<1e-9) throw std::invalid_argument("Invalid slope normal");
    return std::acos(std::clamp(normalized(normal).y,-1.,1.))*180/std::numbers::pi;
}
inline bool walkable(Vec3 normal,double maximum=45) {
    if(!std::isfinite(maximum)||maximum<0||maximum>90) throw std::invalid_argument("Invalid maximum slope");
    return slope_angle(normal)<=maximum+1e-3;
}
inline bool clearance(const PhysicsWorld& world,PhysicsShapeId capsule,Vec3 center) {
    return world.overlap({capsule,center,{}}).empty();
}
inline std::optional<CapsuleCastHit> landing(const PhysicsWorld& world,PhysicsShapeId capsule,Vec3 center,double distance) {
    if(!std::isfinite(distance)||distance<=0) throw std::invalid_argument("Invalid landing distance");
    return world.cast_capsule({capsule,center,{0,-distance,0},{}});
}
struct StepFeasibility {bool can_step{};std::optional<CapsuleCastHit> landing_hit;};
// Query experiment only: no motion, snapping, recovery, velocity or motor state.
inline StepFeasibility step_feasibility(const PhysicsWorld& world,PhysicsShapeId shape,Vec3 center,Vec3 forward,double raise) {
    if(!(std::isfinite(raise)&&raise>0)) throw std::invalid_argument("Invalid step height");
    if(world.cast_capsule({shape,center,{0,raise,0},{}})) return {};
    Vec3 raised=center+Vec3{0,raise,0};
    if(!clearance(world,shape,raised)||world.cast_capsule({shape,raised,forward,{}})) return {};
    auto hit=landing(world,shape,raised+forward,raise+.1);
    return {hit&&walkable(hit->normal),hit};
}
}
