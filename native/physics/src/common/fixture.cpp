// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/physics/fixture.hpp"
#include <numbers>
namespace cordel::physics {
std::vector<FixtureBox> collision_fixture() {
    std::vector<FixtureBox> boxes{
        {"ground",{0,-.25,0},{8,.25,8},{}},
        {"wall",{0,1.5,-4},{3,1.5,.25},{}},
        {"corner",{3.25,1.5,-2},{.25,1.5,2},{}},
        {"ceiling",{-20,1.65,0},{2,.15,2},{}},
        {"door_left",{-20.55,1,-10},{.25,1,.25},{}},
        {"door_right",{-19.45,1,-10},{.25,1,.25},{}},
        {"ledge",{-20,-.25,20},{2,.25,2},{}},
        {"sensor",{-20,1,10},{1,1,1},{},CollisionLayer::Sensor},
        {"overlap_a",{-.2,1,12},{.5,1,.5},{}},
        {"overlap_b",{.2,1,12},{.5,1,.5},{}},
    };
    unsigned i=0;
    for(double angle:{10.,25.,35.,45.,55.}) {
        double half_angle=angle*std::numbers::pi/360;
        boxes.push_back({"ramp_"+std::to_string(int(angle)),{20.+8*i++,.5,0},{2,.15,2},{0,0,std::sin(half_angle),std::cos(half_angle)}});
    }
    i=0;
    for(double height:{.10,.20,.30,.45,.60}) {
        double x=20.+8*i++;
        auto name="step_"+std::to_string(int(std::round(height*100)));
        boxes.push_back({name,{x,height/2,15},{1,height/2,1},{}});
        boxes.push_back({name+"_floor",{x,-.25,15},{3,.25,3},{}});
    }
    return boxes;
}
Fixture load_fixture(PhysicsWorld& world) {
    Fixture fixture;fixture.capsule=world.create_shape(ShapeDesc::capsule());
    for(const auto& box:collision_fixture()) {
        auto shape=world.create_shape(ShapeDesc::box(box.half));
        fixture.bodies[box.name]=world.create_static_body({shape,box.center,box.rotation,box.layer,box.name});
    }
    return fixture;
}
}
