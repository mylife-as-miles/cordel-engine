// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/character/motor.hpp"
#include "cordel/character/demo_fixture.hpp"
#include "cordel/physics/fixture.hpp"
#include "cordel/logic.hpp"
#include "cordel/json.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
using cordel::Json;using cordel::physics::PhysicsWorld;using cordel::physics::Vec3;
using namespace cordel::character;
namespace {
Json::Array checks,results,performance;std::filesystem::path output;
Json vec(Vec3 p) {return Json::Array{p.x,p.y,p.z};}
void save(std::string file,Json j) {if(output.empty()) return;std::ofstream f(output/file);f<<j.dump()<<'\n';if(!f) throw std::runtime_error("Evidence write failed");}
void check(bool ok,std::string name) {checks.emplace_back(Json::Object{{"name",name},{"passed",ok}});if(!ok) throw std::runtime_error(name);}
template<class F> void rejects(F&& f,std::string name) {bool threw=false;try{f();}catch(const std::exception&){threw=true;}check(threw,name);}
void extra_fixture(PhysicsWorld& w) {load_motor_fixture_extensions(w);}
struct Run {
 CharacterMotorState final;double penetration{},max_y{},min_y{};unsigned ground_ticks{},air_ticks{},steps{},recoveries{},caps{},begin{},persist{},end{};std::vector<double> costs;
};
Run execute(PhysicsWorld& w,Vec3 start,Vec3 input,unsigned ticks,bool sprint=false,double vertical=0) {
 CharacterMotor m(w,start);if(vertical) m.reset(start,vertical);
 Run r;r.max_y=r.min_y=start.y;
 for(unsigned i=0;i<ticks;++i) {
  m.simulate(w,{input,sprint},PhysicsWorld::fixed_dt);w.step();
  for(auto e:w.take_contact_events()) {r.begin+=e.phase==cordel::physics::ContactPhase::Begin;r.persist+=e.phase==cordel::physics::ContactPhase::Persist;r.end+=e.phase==cordel::physics::ContactPhase::End;}
  auto s=m.state();auto d=m.diagnostics();
  r.ground_ticks+=s.grounded;r.air_ticks+=!s.grounded;r.steps+=s.step_result==StepResult::Accepted;r.recoveries+=s.penetration_recovery_count;r.caps+=d.slide_cap;
  r.max_y=std::max(r.max_y,s.position.y);r.min_y=std::min(r.min_y,s.position.y);r.costs.push_back(d.cpu_ms);
  check(std::isfinite(s.position.x)&&std::isfinite(s.position.y)&&std::isfinite(s.position.z),"finite motor tick");
  // Check an independently owned reference capsule against world geometry.
  auto probe=w.create_shape(cordel::physics::ShapeDesc::capsule());
  for(auto h:w.overlap({probe,s.position,{}})) r.penetration=std::max(r.penetration,h.penetration);
  w.destroy_shape(probe);
  check(r.penetration<.0001,"no capsule penetration");
  r.final=s;
 }
 return r;
}
Json summary(const Run&r) {return Json::Object{{"final",vec(r.final.position)},{"grounded",r.final.grounded},{"walkable",r.final.walkable_ground},{"normal",vec(r.final.ground_normal)},
 {"vertical_velocity",r.final.vertical_velocity},{"ground_ticks",r.ground_ticks},{"air_ticks",r.air_ticks},{"accepted_step_ticks",r.steps},{"recovery_iterations",r.recoveries},
 {"slide_caps",r.caps},{"max_penetration",r.penetration},{"min_y",r.min_y},{"max_y",r.max_y},{"sensor_begin",r.begin},{"sensor_persist",r.persist},{"sensor_end",r.end}};}
Vec3 ramp_start(int i,double localx) {double degrees[]={10,25,35,45,55};double a=cordel::radians(degrees[i]);return {20+8.*i+localx,.5+localx*std::tan(a)+.15/std::cos(a)+.55+.35/std::cos(a)+.006,0};}
}
int main(int argc,char**argv) {
 try {
  if(argc==3&&std::string(argv[1])=="--output") {output=argv[2];std::filesystem::create_directories(output);}else if(argc!=1) throw std::invalid_argument("Usage: cordel-motor-tests [--output directory]");
  CharacterMotorConfig config;config.validate();
  save("motor-config.json",Json::Object{{"height_m",config.height},{"radius_m",config.radius},{"cylinder_length_m",config.height-2*config.radius},
   {"movement_mps",config.speed},{"sprint_mps",config.sprint_speed},{"gravity_mps2",config.gravity},{"walkable_slope_degrees",config.slope_degrees},
   {"step_height_m",config.step_height},{"skin_m",config.skin},{"ground_probe_m",config.probe_distance},{"max_snap_m",config.snap_distance},
   {"recovery_limit_m",config.recovery_limit},{"recovery_tolerance_m",config.recovery_tolerance},{"slide_iterations",config.slide_limit},{"recovery_iterations",config.recovery_iterations},{"fixed_hz",60}});
  {
   PhysicsWorld w;auto fixture=cordel::physics::load_fixture(w);extra_fixture(w);
   auto scenario=[&](std::string name,Vec3 p,Vec3 input,unsigned ticks,Vec3 lo,Vec3 hi,int grounded=-1,bool sprint=false,double vertical=0) {
    auto r=execute(w,p,input,ticks,sprint,vertical);
    auto end=r.final.position;bool ok=end.x>=lo.x&&end.x<=hi.x&&end.y>=lo.y&&end.y<=hi.y&&end.z>=lo.z&&end.z<=hi.z&&(grounded<0||r.final.grounded==bool(grounded));
    results.emplace_back(Json::Object{{"name",name},{"initial",vec(p)},{"input_sequence",Json::Array{Json::Object{{"first_tick",0},{"ticks",ticks},{"planar",vec(input)},{"sprint",sprint}}}},
     {"expected_min",vec(lo)},{"expected_max",vec(hi)},{"expected_grounded",grounded},{"position_tolerance_m",.005},{"result",summary(r)},{"passed",ok}});
    std::cout<<name<<" "<<summary(r).dump()<<'\n';check(ok,name);
    std::sort(r.costs.begin(),r.costs.end());performance.emplace_back(Json::Object{{"scenario",name},{"samples",r.costs.size()},{"mean_ms",std::accumulate(r.costs.begin(),r.costs.end(),0.)/r.costs.size()},{"median_ms",r.costs[r.costs.size()/2]},{"worst_ms",r.costs.back()}});
    return r;
   };
   scenario("flat_forward_5s",{0,.903,60},{0,0,-1},300,{-.001,.902,37.499},{.001,.904,37.501},1);
   scenario("sprint_5s",{0,.903,60},{0,0,-1},300,{-.001,.902,24.999},{.001,.904,25.001},1,true);
   scenario("diagonal_normalization",{0,.903,60},{1,0,-1},60,{3.181,.902,56.817},{3.183,.904,56.819},1);
   auto idle=scenario("idle_grounded",{0,.903,0},{},300,{-.001,.902,-.001},{.001,.904,.001},1);check(idle.recoveries==0,"idle has no repeated recovery");
   scenario("wall_direct",{0,.903,0},{0,0,-1},120,{-.001,.902,-3.405},{.001,.904,-3.39},1);
   scenario("wall_diagonal",{0,.903,0},{.2,0,-1},120,{1.6,.902,-3.405},{1.9,.904,-3.39},1);
   scenario("wall_shallow",{-2,.903,-3.35},{1,0,-.05},40,{.98,.902,-3.405},{1.01,.904,-3.39},1);
   scenario("inside_corner",{0,.903,0},{1,0,-1},120,{2.64,.902,-3.41},{2.66,.904,-3.39},1);
   scenario("outside_corner",{3.65,.903,1},{0,0,-1},100,{3.84,.902,-6.3},{3.87,.904,-6.2},1);
   scenario("fall_landing",{0,4,0},{},120,{-.001,.902,-.001},{.001,.904,.001},1);
   for(int i=0;i<5;++i) {
    double x=20+8*i,h[]={.1,.2,.3,.45,.6};bool pass=i<3;
    auto r=scenario("step_"+std::to_string(int(h[i]*100)),{x,.903,18},{0,0,-1},60,{x-.001,.902,pass?13.49:16.34},{x+.001,1.51,pass?14.:16.37},1);
    check(pass?r.max_y>.903+h[i]-.06:r.steps==0,"step height gate "+std::to_string(i));
   }
   auto downsmall=scenario("step_down_10",{20,1.003,15.5},{0,0,-1},30,{19.999,.902,13.249},{20.001,.904,13.251},1);check(downsmall.air_ticks==0,"10cm step down adhesion");
   auto downlarge=scenario("step_down_30",{36,1.203,15.5},{0,0,-1},30,{35.999,.8,13.249},{36.001,1.203,13.251},1);check(downlarge.air_ticks==0,"30cm edge descent remains capsule-supported");
   auto drop=scenario("step_down_60",{52,1.503,15.5},{0,0,-1},30,{51.999,.89,13.249},{52.001,1.503,13.251},0);check(drop.air_ticks>0,"60cm step down becomes airborne");
   for(int i=0;i<5;++i) {
    int angles[]={10,25,35,45,55};auto p=ramp_start(i,-1);
    auto r=scenario("ramp_"+std::to_string(angles[i])+"_up",p,{1,0,0},20,{p.x+(i<4?1.49:-1),-2,-.001},{p.x+(i<4?1.51:.01),4,.001},i<4?1:0);
    check(i<4?r.ground_ticks==20:(!r.final.walkable_ground&&r.final.position.y<p.y),"ramp classification "+std::to_string(angles[i]));
    if(i<4) {auto q=ramp_start(i,.5);auto d=scenario("ramp_"+std::to_string(angles[i])+"_down",q,{-1,0,0},20,{q.x-1.51,-2,-.001},{q.x-1.49,4,.001},1);check(d.ground_ticks==20,"downhill adhesion");}
   }
   auto wall_slope=scenario("wall_plus_slope",ramp_start(1,-1),{1,0,1},20,{28.05,.9,.28},{28.08,2,.31},1);check(wall_slope.ground_ticks==20,"wall+slope grounding stable");
   scenario("edge_landing",{-20,4,18.1},{},120,{-20.001,.899,18.099},{-19.999,.904,18.101},1);
   scenario("low_ceiling",{-20,.903,3.5},{0,0,-1},80,{-20.001,.8999,2.1},{-19.999,.904,2.5},1);
   scenario("ceiling_over_step",{60,.903,18},{0,0,-1},70,{59.999,.902,16.1},{60.001,1.2,16.5},1);
   scenario("step_beside_wall",{68,.903,18},{0,0,-1},60,{67.999,.902,13.49},{68.001,1.25,14},1);
   auto head=scenario("head_impact",{-40,2.09,0},{},2,{-40.001,2.08,-.001},{-39.999,2.101,.001},0,false,3);check(head.final.vertical_velocity<=0,"ceiling clamps vertical velocity");
   scenario("narrow_door_blocked",{-20,.903,-7},{0,0,-1},80,{-20.01,.902,-9.58},{-19.99,.904,-9.55},1);
   scenario("wide_door_pass",{-30,.903,-7},{0,0,-1},70,{-30.001,.902,-12.251},{-29.999,.904,-12.249},1);
   scenario("wide_door_glancing",{-30.1,.903,-7},{.02,0,-1},70,{-30.1,.902,-12.26},{-29.9,.904,-12.1},1);
   auto ledge=scenario("ledge_walkoff",{-20,.903,20},{0,0,-1},70,{-20.001,-5,14.74},{-19.999,.903,14.76},0);check(ledge.air_ticks>10&&ledge.final.vertical_velocity<0,"ledge gravity");
   scenario("ledge_parallel",{-20,.903,18.2},{1,0,0},10,{-19.251,.902,18.199},{-19.249,.904,18.201},1);
   scenario("ledge_diagonal",{-20,.903,20},{1,0,-1},60,{-16.83,-3,16.81},{-16.81,.903,16.83},0);
   scenario("small_floor_penetration",{0,.88,0},{},1,{-.001,.902,-.001},{.001,.904,.001},1);
   scenario("small_wall_penetration",{0,.903,-3.43},{},1,{-.001,.902,-3.405},{.001,.904,-3.39},1);
   scenario("small_corner_penetration",{2.67,.88,-3.43},{},1,{2.64,.902,-3.405},{2.66,.904,-3.39},1);
   auto sensor=scenario("sensor_enter_exit",{-20,.903,13},{0,0,-1},80,{-20.001,.902,6.99},{-19.999,.904,7.01},1);check(sensor.begin==1&&sensor.persist>0&&sensor.end==1,"sensor begin persist end during motion");
   // Invalid input/config and stale ownership never enter the backend.
   CharacterMotor m(w,{0,.903,0});double nan=std::numeric_limits<double>::quiet_NaN();
   for(double dt:{0.,-1.,nan,.1}) rejects([&]{m.simulate(w,{},dt);},"invalid dt");
   rejects([&]{m.simulate(w,{{nan,0,0},false},1./60);},"NaN input");rejects([&]{m.reset({nan,0,0});},"NaN position");rejects([&]{m.reset({},nan);},"NaN velocity");
   for(int i=0;i<4;++i) {CharacterMotorConfig c;if(i==0)c.radius=-1;if(i==1)c.slope_degrees=60;if(i==2)c.step_height=2;if(i==3)c.skin=.5;rejects([&]{CharacterMotor bad(w,{},c);},"invalid config");}
   CharacterMotor deep(w,{0,.3,0});rejects([&]{deep.simulate(w,{},1./60);},"deep penetration fails bounded");check(deep.state().position.y==.3,"failed recovery is transactional");
   auto before=m.state().position;auto interpolated=m.interpolated(.5);check(cordel::physics::length(before-m.state().position)==0&&std::isfinite(interpolated.y),"interpolation leaves authoritative state unchanged");
   w.destroy_body(m.body());rejects([&]{m.simulate(w,{},1./60);},"stale body rejected");
  }
  // Identical fixed-tick input sequence under independently varied presentation.
  Json::Array rates;
  for(auto scenario:{"flat","corner","ramp","step","ledge"}) {
   Vec3 reference{};bool first=true;
   for(auto name:{"30","60","120","variable","uncapped"}) {
    PhysicsWorld w;cordel::physics::load_fixture(w);extra_fixture(w);
    Vec3 start=std::string(scenario)=="flat"?Vec3{0,.903,60}:std::string(scenario)=="ramp"?ramp_start(2,-1):std::string(scenario)=="step"?Vec3{36,.903,18}:std::string(scenario)=="ledge"?Vec3{-20,.903,20}:Vec3{0,.903,0};
    CharacterMotor m(w,start);cordel::FixedClock clock;unsigned ticks=0,frames=0;double dropped=0;
    while(ticks<300) {double dt=std::string(name)=="30"?1./30:std::string(name)=="120"?1./120:std::string(name)=="variable"?(frames%2?1./40:1./120):std::string(name)=="uncapped"?1./1000:1./60;
     auto batch=clock.advance(dt);dropped+=batch.dropped;
     for(unsigned j=0;j<batch.ticks&&ticks<300;++j) {
      Vec3 in{0,0,-1};if(std::string(scenario)=="corner")in={1,0,-1};
      if(std::string(scenario)=="ramp")in=ticks<20?Vec3{1,0,0}:ticks<40?Vec3{-1,0,0}:Vec3{};
      if(std::string(scenario)=="step")in=ticks<60?Vec3{0,0,-1}:ticks<120?Vec3{0,0,1}:Vec3{};
      if(std::string(scenario)=="ledge"&&ticks>=70)in={};
      m.simulate(w,{in,false},1./60);w.step();w.take_contact_events();++ticks;
     }
     m.interpolated(batch.alpha);++frames;
    }
    if(first){reference=m.state().position;first=false;}double error=cordel::physics::length(m.state().position-reference);check(error<1e-9&&dropped==0,"presentation independence "+std::string(scenario)+" "+name);
    rates.emplace_back(Json::Object{{"scenario",scenario},{"schedule",name},{"frames",frames},{"fixed_ticks",ticks},{"final",vec(m.state().position)},{"error_m",error},{"dropped_seconds",dropped}});
   }
  }save("presentation-rate-results.json",rates);
  Json::Array cycles;
  for(int i=0;i<5;++i) {{PhysicsWorld w;cordel::physics::load_fixture(w);CharacterMotor m(w,{0,.903,0});for(int n=0;n<60;++n){m.simulate(w,{},1./60);w.step();w.take_contact_events();}}auto live=PhysicsWorld::global_live();check(!live.worlds&&!live.shapes&&!live.bodies&&CharacterMotor::live_count()==0,"lifecycle zero");cycles.emplace_back(Json::Object{{"cycle",i},{"motors",CharacterMotor::live_count()},{"worlds",live.worlds},{"shapes",live.shapes},{"bodies",live.bodies}});}save("lifecycle-results.json",cycles);
  {auto w=std::make_unique<PhysicsWorld>();auto m=std::make_unique<CharacterMotor>(*w,Vec3{0,2,0});w.reset();PhysicsWorld other;rejects([&]{m->simulate(other,{},1./60);},"destroyed world rejected without dereference");m.reset();}
  // 60 simulated seconds, bounded diagnostics, repeated contacts/turns/ramp/ledge/sensor.
  {PhysicsWorld w;auto f=cordel::physics::load_fixture(w);extra_fixture(w);CharacterMotor m(w,{0,.903,0});auto baseline=w.live();unsigned resets=0,sensors=0;double maxpen=0;unsigned ticks=0;
   for(;ticks<3600;++ticks) {unsigned phase=(ticks/120)%6,k=ticks%120;if(k==0){Vec3 p=phase==0?Vec3{0,.903,0}:phase==1?ramp_start(2,-1):phase==2?Vec3{-20,.903,20}:phase==3?Vec3{-20,.903,13}:phase==4?Vec3{36,.903,18}:Vec3{0,.903,0};m.reset(p);++resets;}
    Vec3 in=phase==0?Vec3{1,0,-1}:phase==1?(k<20?Vec3{1,0,0}:k<40?Vec3{-1,0,0}:Vec3{}):phase==5?Vec3{}:Vec3{0,0,-1};
    m.simulate(w,{in,false},1./60);w.step();sensors+=w.take_contact_events().size();for(auto h:w.overlap({f.capsule,m.state().position,{}})) maxpen=std::max(maxpen,h.penetration);
    check(std::isfinite(m.state().position.y)&&w.live().bodies==baseline.bodies&&w.live().shapes==baseline.shapes,"soak bounded finite");
   }check(maxpen<.0001,"soak no penetration accumulation");save("soak-results.json",Json::Object{{"simulated_seconds",60},{"fixed_ticks",ticks},{"reset_boundaries",resets},{"max_penetration_m",maxpen},{"sensor_events",sensors},{"diagnostics_capacity",6},{"normal_clock_drops",0},{"execution","deterministic tick-time soak, not 60s wall-time hardware soak"}});
  }
  // Repeated representative tick fragments, excluding reset/fixture/test costs.
  {PhysicsWorld w;cordel::physics::load_fixture(w);extra_fixture(w);CharacterMotor m(w,{0,.903,0});
   for(auto kind:{"idle","flat","wall_slide","ramp","step_negotiation","recovery"}) {
    std::vector<double> costs;
    for(unsigned i=0;i<300;++i) {
     Vec3 p=std::string(kind)=="flat"?Vec3{0,.903,60}:std::string(kind)=="ramp"?ramp_start(2,-1):std::string(kind)=="step_negotiation"?Vec3{36,.903,16.4}:std::string(kind)=="recovery"?Vec3{0,.88,0}:std::string(kind)=="wall_slide"?Vec3{0,.903,-3.397}:Vec3{0,.903,0};
     m.reset(p);Vec3 input=std::string(kind)=="idle"||std::string(kind)=="recovery"?Vec3{}:std::string(kind)=="ramp"?Vec3{1,0,0}:std::string(kind)=="wall_slide"?Vec3{.2,0,-1}:Vec3{0,0,-1};
     m.simulate(w,{input,false},1./60);costs.push_back(m.diagnostics().cpu_ms);
     if(std::string(kind)=="step_negotiation")check(m.state().step_result==StepResult::Accepted,"profile exercises accepted step");
     if(std::string(kind)=="recovery")check(m.state().penetration_recovery_count>0,"profile exercises recovery");
    }
    std::sort(costs.begin(),costs.end());performance.emplace_back(Json::Object{{"workload",kind},{"samples",costs.size()},{"mean_ms",std::accumulate(costs.begin(),costs.end(),0.)/costs.size()},{"median_ms",costs[costs.size()/2]},{"worst_ms",costs.back()},{"scope","motor.simulate CPU wall time including its collision queries; no render/reset/world.step"}});
   }
  }
  save("scenario-results.json",results);save("performance-results.json",performance);save("tests.json",Json::Object{{"success",true},{"assertions",checks.size()}});
  std::cout<<"Passed "<<checks.size()<<" assertions\n";return 0;
 }catch(const std::exception&e){save("scenario-results.json",results);save("tests.json",Json::Object{{"success",false},{"assertions",checks.size()},{"error",e.what()}});std::cerr<<"Motor gate failed: "<<e.what()<<'\n';return 1;}
}
