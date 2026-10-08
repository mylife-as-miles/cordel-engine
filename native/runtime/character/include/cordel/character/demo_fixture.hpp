// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/physics/fixture.hpp"
namespace cordel::character {
// Greybox test data, separate from movement semantics. Supplemental pads make
// the separated Phase 2.1 ceiling/door/sensor query fixtures playable.
std::vector<physics::FixtureBox> motor_fixture_extensions();
void load_motor_fixture_extensions(physics::PhysicsWorld&);
physics::Vec3 demo_spawn(const std::string& zone);
}
