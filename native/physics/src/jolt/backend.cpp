// Copyright (c) 2026 CORDEL contributors. MIT. Jolt symbols stay private.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include "backend.hpp"
#include <map>
#include <algorithm>
namespace cordel::physics {
namespace {
JPH::Vec3 v(Vec3 p) {return {float(p.x),float(p.y),float(p.z)};}
Vec3 v(JPH::Vec3 p) {return {p.GetX(),p.GetY(),p.GetZ()};}
class Library {
    inline static unsigned users{};
public:
    Library() {if(users++==0) {JPH::RegisterDefaultAllocator();JPH::Factory::sInstance=new JPH::Factory;JPH::RegisterTypes();}}
    ~Library() {if(--users==0) {JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}}
};
struct Broad final:JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers() const override {return 1;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override {return JPH::BroadPhaseLayer(0);}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override {return "static";}
#endif
};
struct ObjectBroad final:JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer,JPH::BroadPhaseLayer) const override {return false;}
};
struct Pairs final:JPH::ObjectLayerPairFilter {bool ShouldCollide(JPH::ObjectLayer,JPH::ObjectLayer) const override {return false;}};
struct LayerFilter final:JPH::ObjectLayerFilter {
    LayerMask layers;explicit LayerFilter(LayerMask m):layers(m) {}
    bool ShouldCollide(JPH::ObjectLayer l) const override {return (layers&l)!=0;}
};
struct BodyFilter final:JPH::BodyFilter {
    int ignore;explicit BodyFilter(int i):ignore(i) {}
    bool ShouldCollideLocked(const JPH::Body& b) const override {return int(b.GetUserData()-1)!=ignore;}
};
class JoltBackend final:public Backend {
    Library library_;Broad broad_;ObjectBroad object_broad_;Pairs pairs_;
    JPH::TempAllocatorImpl allocator_{4*1024*1024};JPH::JobSystemSingleThreaded jobs_{1024};
    JPH::PhysicsSystem system_;
    std::map<std::uint32_t,JPH::RefConst<JPH::Shape>> shapes_;
    std::map<std::uint32_t,JPH::BodyID> bodies_;
    JPH::RefConst<JPH::Shape> point_{new JPH::SphereShape(1e-5f)};
    RawHit hit(const JPH::CollideShapeResult& h,double fraction=0) const {
        return {std::uint32_t(system_.GetBodyInterface().GetUserData(h.mBodyID2)-1),v(h.mContactPointOn2),v(-h.mPenetrationAxis),fraction,h.mPenetrationDepth,h.mSubShapeID2.GetValue()};
    }
    std::vector<RawHit> collisions(const JPH::Shape* s,Vec3 p,RawFilter f) const {
        LayerFilter layer(f.layers);BodyFilter body(f.ignore);
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;JPH::CollideShapeSettings settings;
        system_.GetNarrowPhaseQuery().CollideShape(s,JPH::Vec3::sReplicate(1),JPH::RMat44::sTranslation(v(p)),settings,JPH::RVec3::sZero(),collector,{},layer,body);
        std::vector<RawHit> result;for(auto& h:collector.mHits) result.push_back(hit(h));return result;
    }
public:
    JoltBackend() {system_.Init(4096,0,4096,1024,broad_,object_broad_,pairs_);system_.SetGravity(JPH::Vec3::sZero());}
    ~JoltBackend() override {while(!bodies_.empty()) remove_body(bodies_.begin()->first);shapes_.clear();}
    const char* name() const override {return "Jolt 5.3.0";}
    void shape(std::uint32_t i,const ShapeDesc& s) override {
        if(s.kind==ShapeKind::Box) shapes_[i]=new JPH::BoxShape(v(s.half_extents),0);
        else shapes_[i]=new JPH::CapsuleShape(float(s.total_height/2-s.radius),float(s.radius));
    }
    void remove_shape(std::uint32_t i) override {shapes_.erase(i);}
    void body(std::uint32_t i,std::uint32_t shape,const BodyDesc& b) override {
        JPH::BodyCreationSettings settings(shapes_.at(shape),v(b.position),JPH::Quat(float(b.rotation.x),float(b.rotation.y),float(b.rotation.z),float(b.rotation.w)),JPH::EMotionType::Static,JPH::ObjectLayer(mask(b.layer)));
        settings.mUserData=i+1;settings.mIsSensor=b.layer==CollisionLayer::Sensor;
        auto id=system_.GetBodyInterface().CreateAndAddBody(settings,JPH::EActivation::DontActivate);
        if(id.IsInvalid()) throw std::runtime_error("Jolt body capacity exhausted");
        bodies_[i]=id;
    }
    void remove_body(std::uint32_t i) override {auto id=bodies_.at(i);system_.GetBodyInterface().RemoveBody(id);system_.GetBodyInterface().DestroyBody(id);bodies_.erase(i);}
    void position(std::uint32_t i,Vec3 p) override {system_.GetBodyInterface().SetPosition(bodies_.at(i),v(p),JPH::EActivation::DontActivate);}
    std::optional<RawHit> ray(Vec3 p,Vec3 d,RawFilter f) const override {
        LayerFilter layer(f.layers);BodyFilter body(f.ignore);JPH::RayCastResult h;
        if(!system_.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(v(p),v(d)),h,{},layer,body)) return {};
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(),h.mBodyID);
        auto point=p+d*h.mFraction;
        return RawHit{std::uint32_t(lock.GetBody().GetUserData()-1),point,v(lock.GetBody().GetWorldSpaceSurfaceNormal(h.mSubShapeID2,v(point))),h.mFraction,0,h.mSubShapeID2.GetValue()};
    }
    std::optional<RawHit> cast(std::uint32_t i,Vec3 p,Vec3 d,RawFilter f) const override {
        LayerFilter layer(f.layers);BodyFilter body(f.ignore);
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;JPH::ShapeCastSettings settings;settings.mReturnDeepestPoint=true;
        JPH::RShapeCast cast(shapes_.at(i),JPH::Vec3::sReplicate(1),JPH::RMat44::sTranslation(v(p)),v(d));
        system_.GetNarrowPhaseQuery().CastShape(cast,settings,JPH::RVec3::sZero(),collector,{},layer,body);
        if(!collector.HadHit()) return {};
        return hit(collector.mHit,collector.mHit.mFraction);
    }
    std::vector<RawHit> overlap(std::uint32_t i,Vec3 p,RawFilter f) const override {return collisions(shapes_.at(i),p,f);}
    std::vector<RawHit> point_inside(Vec3 p,RawFilter f) const override {
        auto hits=collisions(point_,p,f);std::erase_if(hits,[](auto h){return h.penetration<=1e-5;});
        std::sort(hits.begin(),hits.end(),[](auto a,auto b){return a.body<b.body;});return hits;
    }
    void step(double dt) override {
        if(system_.Update(float(dt),1,&allocator_,&jobs_)!=JPH::EPhysicsUpdateError::None) throw std::runtime_error("Jolt fixed step failed");
    }
};
}
std::unique_ptr<Backend> make_backend() {return std::make_unique<JoltBackend>();}
}
