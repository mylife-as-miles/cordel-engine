// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/runtime.hpp"
namespace cordel {
inline Vec3 native_vector(physics::Vec3 p) {return {p.x,p.y,p.z};}
inline character::CharacterMotorInput motor_input(const Input& input) {
    if(!input.focused) return {};
    auto a=analog_pair(input.axes[0],input.axes[1]);
    return {{double(input.actions.held(Action::Right))-input.actions.held(Action::Left)+a[0],0,
             double(input.actions.held(Action::Backward))-input.actions.held(Action::Forward)+a[1]},input.actions.held(Action::Sprint)};
}
SceneData motor_debug_scene();
void update_motor_debug(Renderer&,const character::CharacterMotor&,double alpha);
void run_motor_test(Platform&,Renderer&,Counters&,Trace&,const std::filesystem::path&);
}
