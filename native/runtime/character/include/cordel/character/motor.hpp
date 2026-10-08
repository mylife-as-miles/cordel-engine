// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/physics/world.hpp"
#include <array>
namespace cordel::character {
using physics::Vec3;
struct CharacterMotorConfig {
    double height{1.8},radius{.35},speed{4.5},sprint_speed{7},gravity{9.81};
    double slope_degrees{45},step_height{.35},skin{.003},probe_distance{.025},snap_distance{.15};
    double recovery_limit{.2},recovery_tolerance{.00005};
    unsigned slide_limit{6},recovery_iterations{6};
    void validate() const;
};
struct CharacterMotorInput {Vec3 planar{};bool sprint{};};
enum class MoveResult {Moved,Blocked,SlideLimit,InvalidPenetration};
enum class StepResult {None,Accepted,Rejected};
struct CharacterMotorState {
    Vec3 position{},previous_position{},horizontal_velocity{},ground_normal{0,1,0};
    double vertical_velocity{},ground_distance{-1};
    bool grounded{},walkable_ground{},supported{};
    physics::PhysicsBodyId ground_body{};
    MoveResult last_move_result{MoveResult::Moved};
    unsigned penetration_recovery_count{},slide_iteration_count{};
    StepResult step_result{StepResult::None};
    std::uint64_t ticks{};
};
// Fixed-size CPU-only packet. Rendering may inspect this; it cannot advance motion.
struct CharacterMotorDiagnostics {
    Vec3 desired_displacement{},actual_displacement{},probe_end{},step_candidate{};
    std::array<Vec3,6> collision_normals{};unsigned normal_count{};
    double cpu_ms{},recovery_distance{};bool ceiling_hit{},slide_cap{};
};
class CharacterMotor {
    physics::PhysicsWorld* owner_;std::weak_ptr<const void> lifetime_;
    physics::PhysicsShapeId capsule_{};physics::PhysicsBodyId body_{};
    CharacterMotorConfig config_;CharacterMotorState state_;CharacterMotorDiagnostics debug_;
    physics::QueryFilter filter() const;
    std::optional<physics::CapsuleCastHit> cast(Vec3,Vec3) const;
    Vec3 support_normal(const physics::CapsuleCastHit&) const;
    bool recover(Vec3&);
    void ground(Vec3&,double distance,bool snap);
    bool step(Vec3&,Vec3);
    Vec3 move(Vec3,Vec3,bool allow_step);
public:
    CharacterMotor(physics::PhysicsWorld&,Vec3 position,CharacterMotorConfig={});
    ~CharacterMotor();
    CharacterMotor(const CharacterMotor&)=delete;CharacterMotor& operator=(const CharacterMotor&)=delete;
    void reset(Vec3 position,double vertical_velocity=0);
    void simulate(physics::PhysicsWorld&,const CharacterMotorInput&,double fixed_dt);
    const CharacterMotorState& state() const {return state_;}
    const CharacterMotorDiagnostics& diagnostics() const {return debug_;}
    const CharacterMotorConfig& config() const {return config_;}
    physics::PhysicsBodyId body() const {return body_;}
    Vec3 interpolated(double alpha) const;
    static std::size_t live_count();
};
}
