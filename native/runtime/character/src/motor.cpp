// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/character/motor.hpp"
#include "cordel/physics/helpers.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
namespace cordel::character {
namespace {
std::atomic<std::size_t> motors{};
bool finite(Vec3 p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&physics::length(p)<1e5;}
void vector_valid(Vec3 p) {if(!finite(p)) throw std::invalid_argument("Invalid motor vector");}
}
void CharacterMotorConfig::validate() const {
    for(double n:{height,radius,speed,sprint_speed,gravity,slope_degrees,step_height,skin,probe_distance,snap_distance,recovery_limit,recovery_tolerance})
        if(!std::isfinite(n)||n<=0) throw std::invalid_argument("Nonpositive/nonfinite motor configuration");
    if(height<=2*radius||slope_degrees>45||step_height>=height*.5||skin>=radius*.1||probe_distance>=radius||snap_distance>step_height||recovery_limit>=radius||recovery_tolerance>=skin||slide_limit<1||slide_limit>6||recovery_iterations<1||recovery_iterations>16)
        throw std::invalid_argument("Invalid motor limits");
}
CharacterMotor::CharacterMotor(physics::PhysicsWorld& w,Vec3 position,CharacterMotorConfig c):owner_(&w),lifetime_(w.lifetime_token()),config_(c) {
    c.validate();vector_valid(position);
    capsule_=w.create_shape(physics::ShapeDesc::capsule(c.height,c.radius));
    try {body_=w.create_static_body({capsule_,position,{},physics::CollisionLayer::Character,"cordel_motor"});}
    catch(...) {w.destroy_shape(capsule_);throw;}
    state_.position=state_.previous_position=position;++motors;
}
CharacterMotor::~CharacterMotor() {
    if(!lifetime_.expired()) {
        if(owner_->valid(body_)) owner_->destroy_body(body_);
        if(owner_->valid(capsule_)) owner_->destroy_shape(capsule_);
    }
    --motors;
}
std::size_t CharacterMotor::live_count() {return motors.load();}
physics::QueryFilter CharacterMotor::filter() const {return {physics::mask(physics::CollisionLayer::WorldStatic),body_};}
std::optional<physics::CapsuleCastHit> CharacterMotor::cast(Vec3 p,Vec3 d) const {
    if(physics::length(d)<1e-8) return {};
    return owner_->cast_capsule({capsule_,p,d,filter()});
}
void CharacterMotor::reset(Vec3 p,double velocity) {
    vector_valid(p);if(!std::isfinite(velocity)) throw std::invalid_argument("Invalid motor velocity");
    if(lifetime_.expired()||!owner_->valid(body_)||!owner_->valid(capsule_)) throw std::logic_error("Motor physics world/handles expired");
    state_={};state_.position=state_.previous_position=p;state_.vertical_velocity=velocity;debug_={};owner_->set_position(body_,p);
}
bool CharacterMotor::recover(Vec3& p) {
    for(unsigned i=0;i<config_.recovery_iterations;++i) {
        auto hits=owner_->overlap({capsule_,p,filter()});
        auto h=std::max_element(hits.begin(),hits.end(),[](auto a,auto b){return a.penetration<b.penetration;});
        if(h==hits.end()||h->penetration<=config_.recovery_tolerance) return true;
        double distance=h->penetration+config_.skin;
        if(physics::length(h->normal)<.9||debug_.recovery_distance+distance>config_.recovery_limit) return false;
        p=p+h->normal*distance;debug_.recovery_distance+=distance;++state_.penetration_recovery_count;
    }
    auto hits=owner_->overlap({capsule_,p,filter()});
    return std::none_of(hits.begin(),hits.end(),[&](auto h){return h.penetration>config_.recovery_tolerance;});
}
Vec3 CharacterMotor::support_normal(const physics::CapsuleCastHit& h) const {
    // Capsule/box edge normals describe separation, not necessarily the top
    // surface. Probe just inside that contact; steep ramp face stays steep.
    Vec3 result=h.normal;bool found=false;
    // An exact box corner may choose either face. Four 2 mm offsets probe the
    // contact neighbourhood; they do not change capsule dimensions or position.
    for(Vec3 offset:{Vec3{-.002,0,0},Vec3{.002,0,0},Vec3{0,0,-.002},Vec3{0,0,.002},Vec3{}}) {
        Vec3 origin=h.point+offset+Vec3{0,.02,0};
        auto face=owner_->raycast({origin,{0,-.04,0},filter()});
        if(face&&!face->started_inside&&face->body==h.body&&(!found||face->normal.y>result.y)) {result=face->normal;found=true;}
    }
    return result;
}
void CharacterMotor::ground(Vec3& p,double distance,bool snap) {
    state_.grounded=state_.walkable_ground=state_.supported=false;state_.ground_body={};state_.ground_distance=-1;state_.ground_normal={0,1,0};
    debug_.probe_end=p+Vec3{0,-distance,0};
    auto h=cast(p,{0,-distance,0});if(!h) return;
    state_.ground_distance=h->distance;state_.ground_normal=(h->point.y<=p.y-config_.height*.5+config_.step_height+config_.skin)?support_normal(*h):h->normal;state_.ground_body=h->body;
    if(state_.ground_normal.y<=.01) return;
    state_.walkable_ground=physics::walkable(state_.ground_normal,config_.slope_degrees);
    state_.supported=h->distance<=config_.probe_distance;
    if(state_.walkable_ground&&(h->distance<=config_.probe_distance||snap)) {
        if(state_.vertical_velocity<=0) {
            p=p+Vec3{0,-std::max(0.,h->distance-config_.skin),0};
            state_.grounded=state_.supported=true;state_.vertical_velocity=0;
        }
    }
}
bool CharacterMotor::step(Vec3& p,Vec3 horizontal) {
    state_.step_result=StepResult::Rejected;
    // Up, across and down must all be collision-query validated.
    Vec3 up{0,config_.step_height+config_.skin,0};
    if(cast(p,up)) return false;
    Vec3 raised=p+up;
    auto forward=cast(raised,horizontal);if(forward) return false;
    Vec3 candidate=raised+horizontal;
    auto down=cast(candidate,{0,-(config_.step_height+config_.snap_distance+config_.skin),0});
    if(!down||!physics::walkable(support_normal(*down),config_.slope_degrees)) return false;
    // Limit the tread rise from the capsule foot, not each incremental lip contact.
    if(down->point.y-(p.y-config_.height*.5)>config_.step_height+config_.skin) return false;
    candidate=candidate+Vec3{0,-std::max(0.,down->distance-config_.skin),0};
    if(candidate.y-p.y>config_.step_height+config_.skin||candidate.y<p.y-config_.snap_distance) return false;
    for(auto h:owner_->overlap({capsule_,candidate,filter()})) if(h.penetration>config_.recovery_tolerance) return false;
    debug_.step_candidate=candidate;p=candidate;state_.step_result=StepResult::Accepted;state_.vertical_velocity=0;return true;
}
Vec3 CharacterMotor::move(Vec3 p,Vec3 remaining,bool allow_step) {
    for(unsigned i=0;i<config_.slide_limit&&physics::length(remaining)>1e-7;++i) {
        ++state_.slide_iteration_count;
        auto h=cast(p,remaining);
        if(!h) {p=p+remaining;return p;}
        auto normal=h->normal;
        if(debug_.normal_count<debug_.collision_normals.size()) debug_.collision_normals[debug_.normal_count++]=normal;
        double n=physics::length(remaining);double fraction=std::max(0.,h->fraction-config_.skin/n);
        p=p+remaining*fraction;remaining=remaining*(1-fraction);
        Vec3 horizontal{remaining.x,0,remaining.z};
        if(allow_step&&normal.y<.7&&physics::length(horizontal)>1e-6&&step(p,horizontal)) return p;
        if(normal.y<-.5&&remaining.y>0) {state_.vertical_velocity=0;debug_.ceiling_hit=true;}
        double inward=physics::dot(remaining,normal);
        if(inward<0) remaining=remaining-normal*inward;
        // When a second plane blocks the first slide, retain only their crease.
        // This removes opposing-door/corner oscillation without adding speed.
        for(unsigned j=0;j+1<debug_.normal_count;++j) {
            Vec3 old=debug_.collision_normals[j];
            if(physics::dot(remaining,old)<-1e-7) {
                Vec3 crease{normal.y*old.z-normal.z*old.y,normal.z*old.x-normal.x*old.z,normal.x*old.y-normal.y*old.x};
                crease=physics::normalized(crease);remaining=crease*physics::dot(remaining,crease);
            }
        }
        for(unsigned j=0;j<debug_.normal_count;++j)
            if(physics::dot(remaining,debug_.collision_normals[j])<-1e-7) remaining={};
        // A steep face is never a source of upward walking displacement.
        if(normal.y>0&&!physics::walkable(normal,config_.slope_degrees)&&remaining.y>0) {
            remaining.y=0;Vec3 planar_normal=physics::normalized(Vec3{normal.x,0,normal.z});
            double into=physics::dot(remaining,planar_normal);if(into<0) remaining=remaining-planar_normal*into;
        }
        if(fraction==0&&inward>=-1e-8) {state_.last_move_result=MoveResult::Blocked;return p;}
        state_.last_move_result=MoveResult::Blocked;
    }
    if(physics::length(remaining)>1e-7) {state_.last_move_result=MoveResult::SlideLimit;debug_.slide_cap=true;}
    return p;
}
void CharacterMotor::simulate(physics::PhysicsWorld& w,const CharacterMotorInput& input,double dt) {
    vector_valid(input.planar);
    if(std::abs(input.planar.y)>1e-9||!std::isfinite(dt)||std::abs(dt-physics::PhysicsWorld::fixed_dt)>1e-12) throw std::invalid_argument("Motor requires planar input and fixed 60 Hz delta");
    if(lifetime_.expired()||&w!=owner_||!w.valid(body_)||!w.valid(capsule_)) throw std::logic_error("Motor physics world/handles expired");
    vector_valid(state_.position);if(!std::isfinite(state_.vertical_velocity)) throw std::logic_error("Invalid motor state");
    auto start=std::chrono::steady_clock::now();debug_={};auto before=state_;state_.previous_position=state_.position;
    state_.penetration_recovery_count=state_.slide_iteration_count=0;state_.step_result=StepResult::None;state_.last_move_result=MoveResult::Moved;
    Vec3 p=state_.position;
    if(!recover(p)) {state_=before;state_.last_move_result=MoveResult::InvalidPenetration;throw std::runtime_error("Motor penetration exceeds bounded recovery");}
    bool was_grounded=before.grounded;
    ground(p,config_.probe_distance,false);
    Vec3 direction=input.planar;double magnitude=physics::length(direction);if(magnitude>1) direction=direction*(1/magnitude);
    state_.horizontal_velocity=direction*(input.sprint?config_.sprint_speed:config_.speed);
    Vec3 displacement=state_.horizontal_velocity*dt;
    if(state_.grounded) {
        // Preserve requested horizontal velocity, derive the ramp elevation.
        displacement.y=-(state_.ground_normal.x*displacement.x+state_.ground_normal.z*displacement.z)/state_.ground_normal.y;
        state_.vertical_velocity=0;
    } else {
        state_.vertical_velocity-=config_.gravity*dt;
        Vec3 fall{0,state_.vertical_velocity*dt,0};
        if(state_.supported&&!state_.walkable_ground) {
            fall=fall-state_.ground_normal*physics::dot(fall,state_.ground_normal);
            Vec3 n=physics::normalized(Vec3{state_.ground_normal.x,0,state_.ground_normal.z});
            double uphill=physics::dot(displacement,n);if(uphill<0) displacement=displacement-n*uphill;
        }
        displacement=displacement+fall;
    }
    debug_.desired_displacement=displacement;
    p=move(p,displacement,state_.grounded);
    ground(p,(was_grounded||state_.grounded)?config_.snap_distance:config_.probe_distance,was_grounded||state_.grounded);
    // A sweep collision may land just beyond probe distance because skin is
    // measured along the sweep. Ground probe remains small and explicit.
    vector_valid(p);state_.position=p;++state_.ticks;w.set_position(body_,p);
    debug_.actual_displacement=p-before.position;
    debug_.cpu_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
Vec3 CharacterMotor::interpolated(double alpha) const {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid motor interpolation alpha");
    alpha=std::clamp(alpha,0.,1.);return state_.previous_position*(1-alpha)+state_.position*alpha;
}
}
