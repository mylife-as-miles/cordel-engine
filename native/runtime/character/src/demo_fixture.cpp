// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/character/demo_fixture.hpp"
#include <numbers>
#include <stdexcept>
namespace cordel::character {
std::vector<physics::FixtureBox> motor_fixture_extensions() {
 return {
 {"long_flat_pad",{0,-.25,50},{30,.25,30},{}},
 {"narrow_door_pad",{-20,-.25,-10},{3,.25,3},{}},
 {"wide_door_pad",{-30,-.25,-10},{3,.25,3},{}},
 {"wide_door_left",{-30.65,1,-10},{.25,1,.25},{}},{"wide_door_right",{-29.35,1,-10},{.25,1,.25},{}},
 {"ceiling_pad",{-20,-.25,0},{4,.25,4},{}},{"sensor_pad",{-20,-.25,10},{3,.25,4},{}},
 {"head_ceiling",{-40,3.1,0},{2,.1,2},{}},
 {"ceiling_step_pad",{60,-.25,15},{3,.25,3},{}},{"ceiling_step",{60,.15,15},{1,.15,1},{}},{"ceiling_over_step",{60,1.95,15},{2,.1,2},{}},
 {"step_wall_pad",{68,-.25,15},{3,.25,3},{}},{"step_beside_wall",{68,.15,15},{1,.15,1},{}},{"step_wall",{68.62,1.5,15},{.15,1.5,3},{}},
 {"ramp_wall",{28,1.5,.8},{2,1.5,.15},{}},
 };
}
void load_motor_fixture_extensions(physics::PhysicsWorld& w) {
 for(auto b:motor_fixture_extensions()) {auto shape=w.create_shape(physics::ShapeDesc::box(b.half));w.create_static_body({shape,b.center,b.rotation,b.layer,b.name});}
}
physics::Vec3 demo_spawn(const std::string& zone) {
 if(zone=="flat")return {0,.903,4};
 if(zone=="long_flat")return {0,.903,60};
 if(zone=="ceiling")return {-20,.903,3.5};
 if(zone=="door")return {-20,.903,-7};
 if(zone=="wide_door")return {-30,.903,-7};
 if(zone=="ledge")return {-20,.903,20};
 if(zone=="sensor")return {-20,.903,13};
 int angles[]={10,25,35,45,55};double heights[]={.1,.2,.3,.45,.6};
 for(int i=0;i<5;++i) {
  if(zone=="ramp"+std::to_string(angles[i])) {double a=angles[i]*std::numbers::pi/180;return {19+8.*i,.5-std::tan(a)+.15/std::cos(a)+.55+.35/std::cos(a)+.006,0};}
  if(zone=="step"+std::to_string(int(heights[i]*100)))return {20+8.*i,.903,18};
 }
 throw std::invalid_argument("Unknown motor greybox zone");
}
}
