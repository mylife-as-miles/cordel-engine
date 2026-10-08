// Copyright (c) 2026 CORDEL contributors. MIT. Same source linked separately to each candidate.
#include "cordel/physics/fixture.hpp"
#include "cordel/physics/helpers.hpp"
#include "cordel/logic.hpp"
#include "cordel/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#ifndef _WIN32
#include <sys/resource.h>
#endif
using namespace cordel::physics;
using Json=cordel::Json;
namespace {
using Clock=std::chrono::steady_clock;
double now() {return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();}
Json vector(Vec3 v) {return Json::Array{v.x,v.y,v.z};}
struct Gate {
    unsigned assertions{};Json::Array checks,queries,lifecycle,performance;
    void require(bool ok,std::string name,Json detail={}) {
        ++assertions;checks.emplace_back(Json::Object{{"name",name},{"passed",ok},{"detail",detail}});
        if(!ok) throw std::runtime_error(name+" "+detail.dump());
    }
    void near(double a,double b,std::string name,double tolerance=.003) {require(std::abs(a-b)<=tolerance,name,Json::Object{{"actual",a},{"expected",b},{"tolerance",tolerance}});}
    template<class F> void rejects(F f,std::string name) {bool caught=false;try {f();}catch(const std::exception&) {caught=true;}require(caught,name);}
    void hit(std::string name,const std::optional<RayHit>& h,PhysicsBodyId expected,double distance,Vec3 normal) {
        require(h.has_value(),name+" hit");require(h->body==expected,name+" stable body ID");
        near(h->distance,distance,name+" distance");near(length(h->normal-normal),0,name+" outward normal",.01);
        near(length(h->normal),1,name+" unit normal",.001);
        queries.emplace_back(Json::Object{{"name",name},{"fraction",h->fraction},{"distance",h->distance},{"normal",vector(h->normal)},{"point",vector(h->point)},{"body_slot",h->body.slot},{"subshape",h->subshape?Json(*h->subshape):Json{}},{"started_inside",h->started_inside}});
    }
    void cast(std::string name,PhysicsWorld& world,PhysicsShapeId capsule,Vec3 start,Vec3 delta,PhysicsBodyId expected,double distance,Vec3 normal,double tolerance=.006) {
        auto h=world.cast_capsule({capsule,start,delta,{}});require(h.has_value(),name+" cast hit");
        require(h->body==expected,name+" cast body ID");near(h->distance,distance,name+" cast distance",tolerance);near(length(h->normal-normal),0,name+" cast normal",.02);
        queries.emplace_back(Json::Object{{"name",name},{"fraction",h->fraction},{"distance",h->distance},{"normal",vector(h->normal)},{"point",vector(h->point)},{"penetration",h->penetration},{"started_inside",h->started_inside},{"body_slot",h->body.slot}});
    }
};
void queries(Gate& g,PhysicsWorld& w,const Fixture& f) {
    auto shape=f.capsule;auto body=[&](std::string name){return f.bodies.at(name);};
    g.require(collision_fixture().size()==25,"shared 25 box fixture");
    g.hit("ray_ground",w.raycast({{0,3,0},{0,-5,0},{}}),body("ground"),3,{0,1,0});
    g.hit("ray_wall",w.raycast({{0,1,0},{0,0,-8},{}}),body("wall"),3.75,{0,0,1});
    g.require(!w.raycast({{100,3,0},{0,-5,0},{}}),"ray miss");
    auto inside=w.raycast({{0,1,-4},{0,0,-2},{}});
    g.require(inside&&inside->started_inside&&inside->fraction==0&&length(inside->normal)==0,"ray inside normalized to zero distance / undefined zero normal");
    g.require(!w.raycast({{0,3,0},{0,-5,0},{mask(CollisionLayer::Sensor)}}),"ray ignores world with sensor filter");
    g.hit("ray_sensor",w.raycast({{-20,1,13},{0,0,-6},{mask(CollisionLayer::Sensor)}}),body("sensor"),2,{0,0,1});
    g.require(!w.raycast({{0,3,0},{0,-5,0},{mask(CollisionLayer::WorldStatic),body("ground")}}),"ignore body filter");
    g.require(!w.raycast({{0,3,0},{0,-5,0},{0}}),"empty layer mask");
    g.rejects([&]{w.raycast({{},{},{}});},"reject zero ray");
    g.rejects([&]{w.raycast({{NAN,0,0},{0,-1,0},{}});},"reject nonfinite ray");
    g.rejects([&]{w.raycast({{},{0,-1,0},{16}});},"reject unknown layer");
    g.cast("sweep_wall",w,shape,{0,1,0},{0,0,-8},body("wall"),3.4,{0,0,1});
    g.cast("sweep_diagonal",w,shape,{0,1,0},{1,0,-8},body("wall"),3.4*std::sqrt(65.)/8,{0,0,1});
    auto corner=w.cast_capsule({shape,{2,1,-2},{2,0,-2},{}});
    g.require(corner&&corner->body==body("corner"),"sweep corner closest side");
    g.near(corner->distance,.65*std::sqrt(2.),"corner distance");
    g.near(length(corner->normal-Vec3{-1,0,0}),0,"corner side normal",.02);
    auto multi=w.overlap({shape,{2.9,1,-3.6},{}});g.require(multi.size()==2,"corner yields two contacts");
    g.cast("sweep_ground",w,shape,{0,2,0},{0,-3,0},body("ground"),1.1,{0,1,0});
    g.cast("sweep_ceiling",w,shape,{-20,.3,0},{0,2,0},body("ceiling"),.3,{0,-1,0});
    g.require(!w.overlap({shape,{100,3,0},{}}).size(),"empty overlap");
    auto ground=w.overlap({shape,{0,.8,0},{}});g.require(ground.size()==1&&ground[0].body==body("ground"),"ground overlap");g.near(ground[0].penetration,.1,"ground penetration");g.near(length(ground[0].normal-Vec3{0,1,0}),0,"ground recovery normal",.01);
    auto wall=w.overlap({shape,{0,1,-3.6},{}});g.require(wall.size()==1&&wall[0].body==body("wall"),"wall overlap");g.near(wall[0].penetration,.2,"wall penetration");
    g.require(w.overlap({shape,Vec3{0,1,-3.6}+wall[0].normal*(wall[0].penetration+.005),{}}).empty(),"wall penetration recovery data clears obstacle");
    auto embedded=w.cast_capsule({shape,{0,1,-3.6},{0,0,-1},{}});g.require(embedded&&embedded->started_inside&&embedded->fraction==0,"initial sweep penetration");g.near(embedded->penetration,.2,"initial sweep depth");
    auto overlaps=w.overlap({shape,{0,1,12},{}});g.require(overlaps.size()==2,"multiple overlapping obstacles");
    for(auto h:overlaps) {g.require(h.penetration>0&&length(h.normal)>.99,"positive penetration and finite unit normal");g.queries.emplace_back(Json::Object{{"name","obstacle_overlap"},{"body_slot",h.body.slot},{"normal",vector(h.normal)},{"penetration",h.penetration}});}
    g.require(w.overlap({shape,{-20,1,10},{}}).empty(),"sensor excluded by default");
    g.require(w.overlap({shape,{-20,1,10},{mask(CollisionLayer::Sensor)}}).size()==1,"sensor included explicitly");
    auto query_body=w.create_static_body({shape,{100,1,0},{},CollisionLayer::GameplayQuery,"query_layer"});
    g.require(w.raycast({{100,1,2},{0,0,-4},{mask(CollisionLayer::GameplayQuery)}})->body==query_body,"gameplay query layer hit");
    g.require(!w.raycast({{100,1,2},{0,0,-4},{}}),"gameplay query layer excluded");w.destroy_body(query_body);
    unsigned i=0;
    for(double angle:{10.,25.,35.,45.,55.}) {
        double x=20.+8*i++;auto name="ramp_"+std::to_string(int(angle));
        auto h=w.raycast({{x,4,0},{0,-6,0},{}});g.require(h&&h->body==body(name),name+" downward ray");g.near(slope_angle(h->normal),angle,name+" slope",.003);g.require(walkable(h->normal)==(angle<=45),name+" classification");
        auto cast=w.cast_capsule({shape,{x,4,0},{0,-6,0},{}});g.require(cast&&cast->body==body(name),name+" capsule landing");g.near(slope_angle(cast->normal),angle,name+" capsule normal",.05);g.require(walkable(cast->normal)==(angle<=45),name+" capsule walkability");
        g.queries.emplace_back(Json::Object{{"name",name},{"ray_angle",slope_angle(h->normal)},{"capsule_angle",slope_angle(cast->normal)},{"walkable",walkable(cast->normal)}});
    }
    for(double angle:{44.999,45.,45.01,90.}) {double a=angle*std::numbers::pi/180;g.require(walkable({std::sin(a),std::cos(a),0})==(angle<=45.001),"slope boundary "+std::to_string(angle));}
    g.rejects([]{slope_angle({});},"zero slope normal rejected");
    g.rejects([]{walkable({0,1,0},-1);},"negative maximum slope rejected");
    g.rejects([&]{landing(w,shape,{0,2,0},-1);},"negative landing distance rejected");
    g.rejects([&]{w.create_static_body({shape,{0,3,0},{0,0,.5,std::sqrt(.75)},CollisionLayer::Character,"tilted"});},"tilted character sensor probe rejected");
    i=0;
    for(double height:{.1,.2,.3,.45,.6}) {
        double x=20.+8*i++;auto name="step_"+std::to_string(int(std::round(height*100)));
        auto forward=w.cast_capsule({shape,{x,.92,18},{0,0,-3},{}});g.require(forward&&forward->body==body(name),name+" forward blocked");
        auto feasibility=step_feasibility(w,shape,{x,.92,18},{0,0,-3},.35);
        g.require(feasibility.can_step==(height<=.3),name+" raised clearance feasibility");
        g.queries.emplace_back(Json::Object{{"name",name},{"height",height},{"raise",.35},{"can_step",feasibility.can_step},{"forward_fraction",forward->fraction}});
    }
    g.require(!clearance(w,shape,{-20,.9,0}),"1.5m ceiling rejects 1.8m capsule");
    g.require(clearance(w,shape,{100,.92,0}),"empty ground clearance");
    g.require(w.cast_capsule({shape,{-20,1,-7},{0,0,-6},{}}).has_value(),"0.60m doorway blocks 0.70m capsule");
    w.set_position(body("door_left"),{-20.65,1,-10});w.set_position(body("door_right"),{-19.35,1,-10});
    g.require(!w.cast_capsule({shape,{-20,1,-7},{0,0,-6},{}}),"0.80m doorway passes 0.70m capsule");
    w.set_position(body("door_left"),{-20.55,1,-10});w.set_position(body("door_right"),{-19.45,1,-10});
    g.require(landing(w,shape,{-20,2,20},4).has_value(),"ledge ground present");g.require(!landing(w,shape,{-20,2,24},4),"pit ground absent");
    auto probe=w.create_static_body({shape,{-20,1,13},{},CollisionLayer::Character,"sensor_probe"});
    g.require(w.raycast({{-20,1,15},{0,0,-4},{mask(CollisionLayer::Character)}})->body==probe,"character layer query");
    w.step();g.require(w.take_contact_events().empty(),"no sensor contact outside");
    w.set_position(probe,{-20,1,10});w.step();auto events=w.take_contact_events();g.require(events.size()==1&&events[0].phase==ContactPhase::Begin&&events[0].character==probe&&events[0].sensor==body("sensor"),"sensor begin stable pair");
    w.step();g.rejects([&]{w.step();},"undrained sensor events reject next tick");events=w.take_contact_events();g.require(events.size()==1&&events[0].phase==ContactPhase::Persist,"sensor persist");
    w.set_position(probe,{-20,1,13});w.step();events=w.take_contact_events();g.require(events.size()==1&&events[0].phase==ContactPhase::End,"sensor end");
    w.destroy_body(probe);w.step();g.require(w.take_contact_events().empty(),"sensor state cleared");
    g.rejects([&]{w.step(.1);},"reject presentation delta physics step");
}
void lifetime(Gate& g) {
    PhysicsBodyId old_body;PhysicsShapeId old_shape;
    for(int cycle=0;cycle<5;++cycle) {
        {PhysicsWorld world;auto fixture=load_fixture(world);g.require(!world.valid(old_body)&&!world.valid(old_shape),"previous world IDs rejected");
        auto shape=world.create_shape(ShapeDesc::capsule());auto body=world.create_static_body({shape,{0,2,0},{},CollisionLayer::Character,"lifetime"});
        g.rejects([&]{world.destroy_shape(shape);},"shape destruction with body rejected");
        world.destroy_body(body);g.require(!world.valid(body),"removed body invalid");g.rejects([&]{world.destroy_body(body);},"duplicate body destruction rejected");
        auto next=world.create_static_body({shape,{0,2,0},{},CollisionLayer::Character,"reused"});g.require(next.slot==body.slot&&next.generation!=body.generation,"body slot generation advances");g.rejects([&]{world.set_position(body,{});},"stale body mutation rejected");world.destroy_body(next);
        world.destroy_shape(shape);g.rejects([&]{world.destroy_shape(shape);},"duplicate shape destruction rejected");auto next_shape=world.create_shape(ShapeDesc::capsule());g.require(next_shape.slot==shape.slot&&next_shape.generation!=shape.generation,"shape slot generation advances");g.rejects([&]{world.overlap({shape,{},{}});},"stale query shape rejected");
        g.rejects([&]{world.destroy_body({});},"invalid ID destruction rejected");
        g.rejects([&]{world.create_shape(ShapeDesc::capsule(.2,.35));},"invalid capsule dimensions");
        g.rejects([&]{world.create_shape(ShapeDesc::box({0,1,1}));},"invalid box dimensions");
        std::exception_ptr off_thread;std::thread thread([&]{try {world.raycast({{0,3,0},{0,-5,0},{}});}catch(...) {off_thread=std::current_exception();}});thread.join();g.require(bool(off_thread),"foreign thread query rejected");
        g.require(world.raycast({{0,3,0},{0,-5,0},{}}).has_value(),"lifecycle query");old_body=fixture.bodies.at("ground");old_shape=next_shape;
        world.clear();auto live=world.live();g.require(live.bodies==0&&live.shapes==0,"clear removes all bodies and shapes");}
        auto global=PhysicsWorld::global_live();g.require(global.worlds==0&&global.shapes==0&&global.bodies==0,"world destruction zero owned handles");
        g.lifecycle.emplace_back(Json::Object{{"cycle",cycle+1},{"live_worlds",global.worlds},{"live_shapes",global.shapes},{"live_bodies",global.bodies}});
    }
}
template<class F> void benchmark(Gate& g,std::string name,unsigned count,F query) {
    for(int i=0;i<200;++i) query();std::vector<double> samples; samples.reserve(count);
    double start=now();for(unsigned i=0;i<count;++i) {double before=now();query();samples.push_back((now()-before)*1e6);}double total=(now()-start)*1000;
    std::sort(samples.begin(),samples.end());g.performance.emplace_back(Json::Object{{"name",name},{"count",count},{"batch_ms",total},{"mean_us",std::accumulate(samples.begin(),samples.end(),0.)/count},{"median_us",(samples[(count-1)/2]+samples[count/2])/2},{"worst_us",samples.back()},{"warmup",200}});
}
}
int main(int argc,char** argv) {
    Gate g;std::string backend;double init_ms=0;
    try {
        {double before=now();PhysicsWorld w;auto fixture=load_fixture(w);init_ms=(now()-before)*1000;backend=w.backend_name();queries(g,w,fixture);w.clear();}
        lifetime(g);
        {PhysicsWorld w;auto f=load_fixture(w);
        for(unsigned count:{100u,1000u}) {
            benchmark(g,"ray",count,[&]{if(!w.raycast({{0,3,0},{0,-5,0},{}})) throw std::runtime_error("benchmark miss");});
            benchmark(g,"capsule_sweep",count,[&]{if(!w.cast_capsule({f.capsule,{0,1,0},{0,0,-8},{}})) throw std::runtime_error("benchmark miss");});
            benchmark(g,"overlap",count,[&]{if(w.overlap({f.capsule,{0,1,12},{}}).size()!=2) throw std::runtime_error("benchmark overlap");});
            benchmark(g,"fixed_step_static",count,[&]{w.step();w.take_contact_events();});
        }
        cordel::FixedClock clock;auto s=clock.advance(.25);for(unsigned t=0;t<s.ticks;++t) w.step();g.require(s.ticks==3,"existing fixed clock caps physics catchup at three ticks");g.near(s.simulated,.05,"stall simulates 50ms",1e-9);g.near(s.dropped,.2,"stall drops 200ms",1e-9);
        auto before=w.ticks();for(int i=0;i<120;++i) {auto scheduled=clock.advance(1./120.);for(unsigned t=0;t<scheduled.ticks;++t) w.step();}g.require(w.ticks()-before==60,"120 presentation frames produce 60 physics ticks");
        }
        auto live=PhysicsWorld::global_live();g.require(live.worlds==0&&live.bodies==0&&live.shapes==0,"final zero physics handles");
        double peak_rss=0;
#ifndef _WIN32
        rusage usage{};getrusage(RUSAGE_SELF,&usage);peak_rss=double(usage.ru_maxrss);
#endif
        Json::Array geometry;for(auto& b:collision_fixture()) geometry.emplace_back(Json::Object{{"name",b.name},{"center",vector(b.center)},{"half_extents",vector(b.half)},{"rotation_xyzw",Json::Array{b.rotation.x,b.rotation.y,b.rotation.z,b.rotation.w}},{"layer",mask(b.layer)}});
        Json result=Json::Object{{"passed",true},{"backend",backend},{"assertions",g.assertions},{"fixture_bodies",25},{"coordinate_convention","RH +Y up +X right -Z forward; metre"},{"capsule_total_height",1.8},{"capsule_radius",.35},{"capsule_cylinder_length",1.1},{"scene_initialization_ms",init_ms},{"peak_rss_kib",peak_rss},{"fixture",geometry},{"checks",g.checks},{"queries",g.queries},{"lifecycle",g.lifecycle},{"performance",g.performance},{"fixed_dt",PhysicsWorld::fixed_dt},{"final_live",Json::Array{live.worlds,live.shapes,live.bodies}}};
        if(argc==2) {std::ofstream out(argv[1]);out<<result.dump()<<'\n';if(!out) throw std::runtime_error("Cannot write result");}
        std::cout<<backend<<": "<<g.assertions<<" assertions passed; five teardown cycles; final handles 0\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"PHYSICS GATE FAILED at assertion "<<g.assertions<<": "<<error.what()<<'\n';if(argc==2) {std::ofstream out(argv[1]);out<<Json(Json::Object{{"passed",false},{"checks",g.checks},{"error",error.what()}}).dump()<<'\n';}return 1;}
}
