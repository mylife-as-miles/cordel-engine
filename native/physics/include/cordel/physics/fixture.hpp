// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "world.hpp"
#include <map>
namespace cordel::physics {
struct FixtureBox {std::string name;Vec3 center,half;Quaternion rotation;CollisionLayer layer{CollisionLayer::WorldStatic};};
std::vector<FixtureBox> collision_fixture();
struct Fixture {
    PhysicsShapeId capsule;
    std::map<std::string,PhysicsBodyId> bodies;
};
Fixture load_fixture(PhysicsWorld&);
}
