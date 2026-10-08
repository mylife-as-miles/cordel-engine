// Copyright (c) 2026 CORDEL contributors. MIT. Private adapter contract.
#pragma once
#include "cordel/physics/world.hpp"
namespace cordel::physics {
struct RawHit {std::uint32_t body{};Vec3 point,normal;double fraction{},penetration{};std::optional<std::uint32_t> subshape;};
struct RawFilter {LayerMask layers;int ignore{-1};};
class Backend {
public:
    virtual ~Backend()=default;virtual const char* name() const=0;
    virtual void shape(std::uint32_t,const ShapeDesc&)=0;
    virtual void remove_shape(std::uint32_t)=0;
    virtual void body(std::uint32_t,std::uint32_t,const BodyDesc&)=0;
    virtual void remove_body(std::uint32_t)=0;
    virtual void position(std::uint32_t,Vec3)=0;
    virtual std::optional<RawHit> ray(Vec3,Vec3,RawFilter) const=0;
    virtual std::optional<RawHit> cast(std::uint32_t,Vec3,Vec3,RawFilter) const=0;
    virtual std::vector<RawHit> overlap(std::uint32_t,Vec3,RawFilter) const=0;
    virtual std::vector<RawHit> point_inside(Vec3,RawFilter) const=0;
    virtual void step(double)=0;
};
std::unique_ptr<Backend> make_backend(); // Exactly one adapter linked per executable.
}
