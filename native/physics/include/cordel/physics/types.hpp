// Copyright (c) 2026 CORDEL contributors. MIT; see native/physics/LICENSE.
#pragma once
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace cordel::physics {
struct Vec3 {
    double x{},y{},z{};
    Vec3 operator+(Vec3 b) const {return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b) const {return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator*(double s) const {return {x*s,y*s,z*s};}
};
inline double dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline double length(Vec3 a) {return std::sqrt(dot(a,a));}
inline Vec3 normalized(Vec3 v) {double n=length(v);return n>1e-12?v*(1/n):Vec3{};}
struct Quaternion {double x{},y{},z{},w{1};}; // XYZW, active local -> world rotation
struct PhysicsBodyId {std::uint64_t world{};std::uint32_t slot{},generation{};auto operator<=>(const PhysicsBodyId&) const=default;};
struct PhysicsShapeId {std::uint64_t world{};std::uint32_t slot{},generation{};auto operator<=>(const PhysicsShapeId&) const=default;};
enum class CollisionLayer : std::uint32_t {WorldStatic=1,Character=2,Sensor=4,GameplayQuery=8};
using LayerMask=std::uint32_t;
constexpr LayerMask mask(CollisionLayer layer) {return static_cast<LayerMask>(layer);}
constexpr LayerMask all_layers=15;
struct QueryFilter {LayerMask layers{mask(CollisionLayer::WorldStatic)};PhysicsBodyId ignore{};};
enum class ShapeKind {Box,Capsule};
struct ShapeDesc {
    ShapeKind kind{ShapeKind::Box};Vec3 half_extents{.5,.5,.5};
    double radius{.35},total_height{1.8};
    static ShapeDesc box(Vec3 half) {ShapeDesc s;s.half_extents=half;return s;}
    static ShapeDesc capsule(double height=1.8,double radius=.35) {ShapeDesc s;s.kind=ShapeKind::Capsule;s.total_height=height;s.radius=radius;return s;}
};
struct BodyDesc {
    PhysicsShapeId shape;Vec3 position;Quaternion rotation;
    CollisionLayer layer{CollisionLayer::WorldStatic};std::string name;
};
struct LiveCounts {std::size_t worlds{},shapes{},bodies{};};
enum class ContactPhase {Begin,Persist,End};
struct ContactEvent {
    ContactPhase phase;PhysicsBodyId character,sensor;Vec3 normal;double penetration{};std::uint64_t tick{};
};
}
