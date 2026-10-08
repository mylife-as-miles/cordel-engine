// Copyright (c) 2026 CORDEL contributors. MIT. Bullet symbols stay private.
#include <btBulletDynamicsCommon.h>
#include "backend.hpp"
#include <map>
#include <algorithm>
namespace cordel::physics {
namespace {
btVector3 v(Vec3 p) {return {btScalar(p.x),btScalar(p.y),btScalar(p.z)};}
Vec3 v(const btVector3& p) {return {p.x(),p.y(),p.z()};}
btTransform transform(Vec3 p,Quaternion q={}) {return btTransform(btQuaternion(btScalar(q.x),btScalar(q.y),btScalar(q.z),btScalar(q.w)),v(p));}
template<class Base> struct Filtered:Base {
    int ignore;
    template<class... Args> Filtered(RawFilter f,Args&&... args):Base(std::forward<Args>(args)...),ignore(f.ignore) {
        this->m_collisionFilterGroup=all_layers;this->m_collisionFilterMask=int(f.layers);
    }
    bool needsCollision(btBroadphaseProxy* proxy) const override {
        return static_cast<btCollisionObject*>(proxy->m_clientObject)->getUserIndex()!=ignore&&Base::needsCollision(proxy);
    }
};
struct Contacts final:Filtered<btCollisionWorld::ContactResultCallback> {
    std::vector<RawHit> hits;const btCollisionObject* query;
    Contacts(RawFilter f,const btCollisionObject* q):Filtered(f),query(q) {}
    btScalar addSingleResult(btManifoldPoint& cp,const btCollisionObjectWrapper* a,int,int,const btCollisionObjectWrapper* b,int,int) override {
        if(cp.getDistance()>0) return 0;
        bool first=a->getCollisionObject()==query;auto target=first?b->getCollisionObject():a->getCollisionObject();
        hits.push_back({std::uint32_t(target->getUserIndex()),v(first?cp.getPositionWorldOnB():cp.getPositionWorldOnA()),v(first?cp.m_normalWorldOnB:-cp.m_normalWorldOnB),0,-cp.getDistance(),std::nullopt});return 0;
    }
};
class BulletBackend final:public Backend {
    btDefaultCollisionConfiguration config_;btCollisionDispatcher dispatcher_{&config_};
    btDbvtBroadphase broad_;btSequentialImpulseConstraintSolver solver_;
    mutable btDiscreteDynamicsWorld world_{&dispatcher_,&broad_,&solver_,&config_};
    std::map<std::uint32_t,std::unique_ptr<btCollisionShape>> shapes_;
    std::map<std::uint32_t,std::unique_ptr<btCollisionObject>> bodies_;
    mutable btSphereShape point_{btScalar(1e-5)};
    std::vector<RawHit> collisions(btCollisionShape* shape,Vec3 p,RawFilter f) const {
        btCollisionObject query;query.setCollisionShape(shape);query.setWorldTransform(transform(p));query.setUserIndex(-1);
        Contacts callback(f,&query);world_.contactTest(&query,callback);return callback.hits;
    }
public:
    BulletBackend() {world_.setGravity(btVector3(0,0,0));}
    ~BulletBackend() override {while(!bodies_.empty()) remove_body(bodies_.begin()->first);shapes_.clear();}
    const char* name() const override {return "Bullet 3.25";}
    void shape(std::uint32_t i,const ShapeDesc& s) override {
        if(s.kind==ShapeKind::Box) {auto box=std::make_unique<btBoxShape>(v(s.half_extents));box->setMargin(0);shapes_[i]=std::move(box);}
        else shapes_[i]=std::make_unique<btCapsuleShape>(btScalar(s.radius),btScalar(s.total_height-2*s.radius));
    }
    void remove_shape(std::uint32_t i) override {shapes_.erase(i);}
    void body(std::uint32_t i,std::uint32_t shape,const BodyDesc& b) override {
        auto object=std::make_unique<btCollisionObject>();object->setCollisionShape(shapes_.at(shape).get());object->setWorldTransform(transform(b.position,b.rotation));object->setUserIndex(int(i));
        object->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT|(b.layer==CollisionLayer::Sensor?btCollisionObject::CF_NO_CONTACT_RESPONSE:0));
        world_.addCollisionObject(object.get(),int(mask(b.layer)),all_layers);bodies_[i]=std::move(object);
    }
    void remove_body(std::uint32_t i) override {world_.removeCollisionObject(bodies_.at(i).get());bodies_.erase(i);}
    void position(std::uint32_t i,Vec3 p) override {auto& b=*bodies_.at(i);auto pose=b.getWorldTransform();pose.setOrigin(v(p));b.setWorldTransform(pose);world_.updateSingleAabb(&b);}
    std::optional<RawHit> ray(Vec3 p,Vec3 d,RawFilter f) const override {
        Filtered<btCollisionWorld::ClosestRayResultCallback> callback(f,v(p),v(p+d));world_.rayTest(v(p),v(p+d),callback);
        if(!callback.hasHit()) return {};
        return RawHit{std::uint32_t(callback.m_collisionObject->getUserIndex()),v(callback.m_hitPointWorld),v(callback.m_hitNormalWorld),callback.m_closestHitFraction,0,std::nullopt};
    }
    std::optional<RawHit> cast(std::uint32_t i,Vec3 p,Vec3 d,RawFilter f) const override {
        Filtered<btCollisionWorld::ClosestConvexResultCallback> callback(f,v(p),v(p+d));
        world_.convexSweepTest(static_cast<btConvexShape*>(shapes_.at(i).get()),transform(p),transform(p+d),callback,0);
        if(!callback.hasHit()) return {};
        return RawHit{std::uint32_t(callback.m_hitCollisionObject->getUserIndex()),v(callback.m_hitPointWorld),v(callback.m_hitNormalWorld),callback.m_closestHitFraction,0,std::nullopt};
    }
    std::vector<RawHit> overlap(std::uint32_t i,Vec3 p,RawFilter f) const override {return collisions(shapes_.at(i).get(),p,f);}
    std::vector<RawHit> point_inside(Vec3 p,RawFilter f) const override {
        auto hits=collisions(&point_,p,f);std::erase_if(hits,[](auto h){return h.penetration<=1e-5;});
        std::sort(hits.begin(),hits.end(),[](auto a,auto b){return a.body<b.body;});return hits;
    }
    void step(double dt) override {world_.stepSimulation(btScalar(dt),0);}
};
}
std::unique_ptr<Backend> make_backend() {return std::make_unique<BulletBackend>();}
}
