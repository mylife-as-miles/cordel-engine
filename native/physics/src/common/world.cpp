// Copyright (c) 2026 CORDEL contributors. MIT.
#include "backend.hpp"
#include <algorithm>
#include <atomic>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
namespace cordel::physics {
namespace {
std::atomic<std::uint64_t> next_world{1};
std::atomic<std::size_t> worlds{},shapes{},bodies{};
bool finite(Vec3 v) {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&length(v)<1e6;}
void point(Vec3 v) {if(!finite(v)) throw std::invalid_argument("Invalid physics vector");}
void displacement(Vec3 v) {point(v);if(length(v)<1e-8) throw std::invalid_argument("Zero query displacement");}
template<class T> struct Slot {std::uint32_t generation{1};std::optional<T> value;};
template<class T> std::uint32_t free_slot(std::vector<Slot<T>>& slots) {
    for(std::uint32_t i=0;i<slots.size();++i) if(!slots[i].value&&slots[i].generation) return i;
    if(slots.size()>=4096) throw std::length_error("Physics handle capacity exceeded");
    slots.emplace_back();return static_cast<std::uint32_t>(slots.size()-1);
}
}
struct PhysicsWorld::Impl {
    std::shared_ptr<const void> lifetime{std::make_shared<int>(0)};
    std::uint64_t owner{next_world++},ticks{};
    std::thread::id thread{std::this_thread::get_id()};
    std::unique_ptr<Backend> backend{make_backend()};
    std::vector<Slot<ShapeDesc>> shapes;std::vector<Slot<BodyDesc>> bodies;
    using Pair=std::pair<PhysicsBodyId,PhysicsBodyId>;
    std::map<Pair,OverlapHit> previous;std::vector<ContactEvent> events;
    void check_thread() const {if(thread!=std::this_thread::get_id()) throw std::logic_error("Physics world accessed off authoritative thread");}
    template<class Id,class T> bool valid(Id id,const std::vector<Slot<T>>& slots) const {
        return id.world==owner&&id.slot<slots.size()&&slots[id.slot].value&&id.generation==slots[id.slot].generation;
    }
    PhysicsBodyId id(std::uint32_t slot) const {return {owner,slot,bodies.at(slot).generation};}
    RawFilter filter(QueryFilter f) const {
        check_thread();if(f.layers&~all_layers) throw std::invalid_argument("Unknown layer mask");
        if(f.ignore.world&&!valid(f.ignore,bodies)) throw std::invalid_argument("Stale ignored body");
        return {f.layers,f.ignore.world?int(f.ignore.slot):-1};
    }
    const ShapeDesc& capsule(PhysicsShapeId id) const {
        check_thread();if(!valid(id,shapes)||shapes[id.slot].value->kind!=ShapeKind::Capsule) throw std::invalid_argument("Invalid capsule handle");
        return *shapes[id.slot].value;
    }
};
PhysicsWorld::PhysicsWorld():impl_(std::make_unique<Impl>()) {++worlds;}
PhysicsWorld::~PhysicsWorld() {clear();impl_.reset();--worlds;}
std::weak_ptr<const void> PhysicsWorld::lifetime_token() const {impl_->check_thread();return impl_->lifetime;}
const char* PhysicsWorld::backend_name() const {impl_->check_thread();return impl_->backend->name();}
PhysicsShapeId PhysicsWorld::create_shape(const ShapeDesc& s) {
    auto& p=*impl_;p.check_thread();
    if(s.kind==ShapeKind::Box) {point(s.half_extents);if(std::min({s.half_extents.x,s.half_extents.y,s.half_extents.z})<=0) throw std::invalid_argument("Invalid box dimensions");}
    else if(s.kind!=ShapeKind::Capsule||!std::isfinite(s.radius)||!std::isfinite(s.total_height)||s.radius<=0||s.total_height<=2*s.radius||s.total_height>=1e6) throw std::invalid_argument("Invalid capsule dimensions");
    auto slot=free_slot(p.shapes);p.backend->shape(slot,s);p.shapes[slot].value=s;++shapes;
    return {p.owner,slot,p.shapes[slot].generation};
}
PhysicsBodyId PhysicsWorld::create_static_body(const BodyDesc& b) {
    auto& p=*impl_;p.check_thread();point(b.position);
    double q=b.rotation.x*b.rotation.x+b.rotation.y*b.rotation.y+b.rotation.z*b.rotation.z+b.rotation.w*b.rotation.w;
    if(!p.valid(b.shape,p.shapes)||!std::isfinite(q)||std::abs(q-1)>1e-6||!(mask(b.layer)==1||mask(b.layer)==2||mask(b.layer)==4||mask(b.layer)==8)) throw std::invalid_argument("Invalid body descriptor");
    if(b.layer==CollisionLayer::Character&&(p.shapes[b.shape.slot].value->kind!=ShapeKind::Capsule||std::abs(b.rotation.x)+std::abs(b.rotation.y)+std::abs(b.rotation.z)>1e-9)) throw std::invalid_argument("Character sensor probe requires upright capsule");
    auto slot=free_slot(p.bodies);p.backend->body(slot,b.shape.slot,b);p.bodies[slot].value=b;++bodies;return p.id(slot);
}
bool PhysicsWorld::valid(PhysicsBodyId id) const {impl_->check_thread();return impl_->valid(id,impl_->bodies);}
bool PhysicsWorld::valid(PhysicsShapeId id) const {impl_->check_thread();return impl_->valid(id,impl_->shapes);}
void PhysicsWorld::destroy_body(PhysicsBodyId id) {
    auto& p=*impl_;p.check_thread();if(!valid(id)) throw std::invalid_argument("Invalid/stale body destruction");
    p.backend->remove_body(id.slot);auto& s=p.bodies[id.slot];s.value.reset();++s.generation;--bodies;
}
void PhysicsWorld::destroy_shape(PhysicsShapeId id) {
    auto& p=*impl_;p.check_thread();if(!valid(id)) throw std::invalid_argument("Invalid/stale shape destruction");
    for(auto& body:p.bodies) if(body.value&&body.value->shape==id) throw std::logic_error("Shape still owns body references");
    p.backend->remove_shape(id.slot);auto& s=p.shapes[id.slot];s.value.reset();++s.generation;--shapes;
}
void PhysicsWorld::set_position(PhysicsBodyId id,Vec3 position) {
    impl_->check_thread();point(position);if(!valid(id)) throw std::invalid_argument("Invalid/stale body position");
    impl_->backend->position(id.slot,position);impl_->bodies[id.slot].value->position=position;
}
std::optional<RayHit> PhysicsWorld::raycast(const RayQuery& q) const {
    point(q.origin);displacement(q.displacement);auto f=impl_->filter(q.filter);
    auto inside=impl_->backend->point_inside(q.origin,f);
    if(!inside.empty()) return RayHit{impl_->id(inside.front().body),q.origin,{},0,0,inside.front().subshape,true};
    auto h=impl_->backend->ray(q.origin,q.displacement,f);if(!h) return {};
    return RayHit{impl_->id(h->body),h->point,normalized(h->normal),h->fraction,length(q.displacement)*h->fraction,h->subshape,false};
}
std::vector<OverlapHit> PhysicsWorld::overlap(const OverlapQuery& q) const {
    impl_->capsule(q.shape);point(q.center);auto f=impl_->filter(q.filter);
    std::map<std::uint32_t,RawHit> unique;
    for(auto h:impl_->backend->overlap(q.shape.slot,q.center,f)) if(h.penetration>=0) {
        auto it=unique.find(h.body);if(it==unique.end()||h.penetration>it->second.penetration) unique[h.body]=h;
    }
    std::vector<OverlapHit> hits;for(auto [slot,h]:unique) hits.push_back({impl_->id(slot),h.point,normalized(h.normal),h.penetration,h.subshape});return hits;
}
std::optional<CapsuleCastHit> PhysicsWorld::cast_capsule(const CapsuleCastQuery& q) const {
    impl_->capsule(q.shape);point(q.center);displacement(q.displacement);auto f=impl_->filter(q.filter);
    auto initial=overlap({q.shape,q.center,q.filter});
    auto deepest=std::max_element(initial.begin(),initial.end(),[](auto a,auto b){return a.penetration<b.penetration;});
    if(deepest!=initial.end()&&deepest->penetration>1e-4) {
        CapsuleCastHit hit;static_cast<RayHit&>(hit)={deepest->body,deepest->point,deepest->normal,0,0,deepest->subshape,true};hit.penetration=deepest->penetration;return hit;
    }
    auto h=impl_->backend->cast(q.shape.slot,q.center,q.displacement,f);if(!h) return {};
    CapsuleCastHit hit;static_cast<RayHit&>(hit)={impl_->id(h->body),h->point,normalized(h->normal),std::clamp(h->fraction,0.,1.),length(q.displacement)*std::clamp(h->fraction,0.,1.),h->subshape,false};
    hit.penetration=std::max(0.,h->penetration);return hit;
}
void PhysicsWorld::step(double dt) {
    auto& p=*impl_;p.check_thread();if(!std::isfinite(dt)||std::abs(dt-fixed_dt)>1e-12) throw std::invalid_argument("Physics requires CORDEL fixed 60 Hz delta");
    if(!p.events.empty()) throw std::logic_error("Drain contact events before next fixed tick");
    p.backend->step(dt);++p.ticks;
    std::map<Impl::Pair,OverlapHit> current;
    for(std::uint32_t i=0;i<p.bodies.size();++i) if(p.bodies[i].value&&p.bodies[i].value->layer==CollisionLayer::Character) {
        auto& b=*p.bodies[i].value;
        for(auto h:overlap({b.shape,b.position,{mask(CollisionLayer::Sensor),p.id(i)}})) {
            if(current.size()>=4096) throw std::length_error("Physics sensor pair capacity exceeded");
            current[{p.id(i),h.body}]=h;
        }
    }
    // Bounded per tick. Caller consumes before next tick; no historical event accumulation.
    p.events.clear();
    for(auto [pair,h]:current) p.events.push_back({p.previous.contains(pair)?ContactPhase::Persist:ContactPhase::Begin,pair.first,pair.second,h.normal,h.penetration,p.ticks});
    for(auto [pair,h]:p.previous) if(!current.contains(pair)) p.events.push_back({ContactPhase::End,pair.first,pair.second,h.normal,0,p.ticks});
    p.previous=std::move(current);
}
std::vector<ContactEvent> PhysicsWorld::take_contact_events() {impl_->check_thread();return std::exchange(impl_->events,{});}
std::uint64_t PhysicsWorld::ticks() const {impl_->check_thread();return impl_->ticks;}
LiveCounts PhysicsWorld::live() const {
    impl_->check_thread();
    return {1,std::size_t(std::count_if(impl_->shapes.begin(),impl_->shapes.end(),[](auto& s){return s.value.has_value();})),std::size_t(std::count_if(impl_->bodies.begin(),impl_->bodies.end(),[](auto& s){return s.value.has_value();}))};
}
LiveCounts PhysicsWorld::global_live() {return {worlds.load(),shapes.load(),bodies.load()};}
void PhysicsWorld::clear() {
    auto& p=*impl_;p.check_thread();for(std::uint32_t i=0;i<p.bodies.size();++i) if(p.bodies[i].value) destroy_body(p.id(i));
    for(std::uint32_t i=0;i<p.shapes.size();++i) if(p.shapes[i].value) destroy_shape({p.owner,i,p.shapes[i].generation});
    p.events.clear();p.previous.clear();
}
}
