// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/logic.hpp"
#include "cordel/scene.hpp"
#include "goldens.hpp"
#include <string>

namespace cordel {
class Checks {
public:
    unsigned passed{};
    void require(bool ok,const std::string& name) {
        if(!ok) throw std::runtime_error("Assertion failed: "+name);
        ++passed;
    }
    void near(double value,double expected,const std::string& name,double tolerance=1e-6) {
        require(std::isfinite(value)&&std::abs(value-expected)<=tolerance,name);
    }
    void vec(Vec3 a,Vec3 b,const std::string& name,double tolerance=1e-6) {
        require(length(a-b)<=tolerance,name);
    }
};
inline unsigned logic_checks(const std::filesystem::path& scene_path) {
    Checks check;
    for(const auto& expected:goldens::cameras) {
        Camera c;c.position={2,3,4};c.yaw=expected.yaw;c.pitch=expected.pitch;c.look(0,0);
        check.near(c.yaw,expected.clamped_yaw,"golden yaw wrap");
        check.near(c.pitch,expected.clamped_pitch,"golden pitch clamp");
        check.vec(c.forward(),expected.forward,"golden forward");
        check.vec(c.right(),expected.right,"golden right");
        auto view=c.view(),projection=c.projection(expected.width,expected.height);
        for(int i=0;i<16;++i) {
            check.near(view[i],expected.view[i],"golden view matrix");
            check.near(projection[i],expected.projection[i],"golden perspective");
        }
        auto origin=transform(view,c.position);
        check.vec({origin[0],origin[1],origin[2]},{0,0,0},"view camera origin");
        auto ahead=transform(view,c.position+c.forward());
        check.vec({ahead[0],ahead[1],ahead[2]},{0,0,-1},"view camera forward");
        check.near(projection[0]*expected.width,projection[5]*expected.height,"aspect square proportion");
    }
    for(const auto& expected:goldens::movements) {
        Camera c;c.position={};c.pitch=0;Actions actions;
        for(int i=0;i<7;++i) if(expected.actions[i]) actions.down(i,static_cast<Action>(i));
        c.move(actions,expected.dt);check.vec(c.position,expected.expected,"golden movement");
    }
    auto scene=load_scene(scene_path);
    check.require(scene.meshes.size()==7,"seven fixture meshes");
    check.require(scene.triangles()==84,"84 fixture triangles");
    std::size_t vertices=0;
    for(std::size_t i=0;i<scene.meshes.size();++i) {
        auto p=transform(scene.meshes[i].model,{});
        check.require(scene.meshes[i].name==goldens::nodes[i].name,"authored node name");
        check.vec({p[0],p[1],p[2]},goldens::nodes[i].translation,"glTF translation boundary");
        vertices+=scene.meshes[i].vertices.size();
    }
    check.require(vertices==168,"168 face vertices");
    Actions held;held.down(1,Action::Forward);held.down(1,Action::Forward);
    held.down(2,Action::Right);held.up(1);
    check.require(!held.held(Action::Forward)&&held.held(Action::Right),"repeat/up/simultaneous actions");
    held.down(3,Action::Right);held.up(2);
    check.require(held.held(Action::Right),"two keys for one action");
    Input input;input.actions=held;input.focused=input.captured=true;input.axes={.5,.5};input.mouse_x=3;
    input.lose_focus();check.require(input.actions.empty()&&!input.focused&&!input.captured&&input.axes==std::array<double,2>{}&&input.mouse_x==0,"focus loss clears all held input");
    auto analog=analog_pair(.575,0);check.near(analog[0],.5,"fractional deadzone rescaling");
    check.require(analog_pair(.1,0)==std::array<double,2>{},"deadzone zero");
    analog=analog_pair(1,1);check.near(std::hypot(analog[0],analog[1]),1,"radial diagonal bounded");
    check.require(analog_pair(std::numeric_limits<double>::quiet_NaN(),0)==std::array<double,2>{},"analog NaN ignored");
    Camera a,b;a.position=b.position={};a.pitch=b.pitch=0;held.clear();held.down(1,Action::Forward);
    for(int i=0;i<30;++i) a.move(held,1./30);
    for(int i=0;i<120;++i) b.move(held,1./120);
    check.vec(a.position,b.position,"delta-time movement at different rates");check.vec(a.position,{0,0,-3},"3 m/s");
    a.position={};a.move(Actions{},1,{.5,0});check.vec(a.position,{1.5,0,0},"analog magnitude movement");
    for(double dt:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        Vec3 before=a.position;a.move(held,dt);check.vec(a.position,before,"invalid movement dt");
    }
    for(double z:{.05,100.}) {
        auto point=transform(perspective(65,850./480,.05,100),{0,0,-z});
        check.near(point[2]/point[3],z==.05?-1:1,"OpenGL near/far NDC");
    }
    bool throws=false;try { perspective(65,0,.05,100); } catch(const std::invalid_argument&) { throws=true; }
    check.require(throws,"invalid projection rejected");
    Mat4 translate=identity;translate[3]=2;translate[7]=3;translate[11]=4;
    check.require(multiply(identity,translate)==translate,"matrix multiplication");
    auto point=transform(translate,{1,1,1});check.vec({point[0],point[1],point[2]},{3,4,5},"matrix column vector");
    FixedClock clock;unsigned ticks=0;
    for(int i=0;i<120;++i) ticks+=clock.advance(1./120).ticks;
    check.require(ticks==60,"60Hz ticks independent of 120Hz render");
    clock.reset();auto step=clock.advance(.25);
    check.require(step.ticks==3,"stall max three ticks");
    check.near(step.simulated,.05,"stall simulation 50ms");
    check.near(step.dropped,.20,"stall backlog discarded");
    check.require(step.alpha>=0&&step.alpha<1,"interpolation alpha bounded");
    step=clock.advance(FixedClock::dt);check.require(step.ticks==1,"stall no residual catch-up debt");
    clock.reset();step=clock.advance(FixedClock::dt*.5);check.require(step.ticks==0,"sub-tick accumulation");
    check.near(step.alpha,.5,"half interpolation alpha");
    for(double raw:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        check.require(clock.advance(raw).ticks==0,"invalid timing safeguards");
    clock.reset();step=clock.advance(.267);
    check.require(step.ticks==3,"long stall bounded");
    check.near(step.simulated+step.dropped+step.alpha*FixedClock::dt,.267,"time conservation");
    Simulation simulation;simulation.reset(Camera{});input.clear();input.focused=true;
    input.actions.down(1,Action::Forward);clock.reset();step=clock.advance(.25);
    Vec3 before=simulation.current.position;
    for(unsigned i=0;i<step.ticks;++i) simulation.tick(input);
    check.near(length(simulation.current.position-before),.15,"stall camera .150m");
    a.position={};b.position={2,4,6};a.yaw=179;b.yaw=-179;a.pitch=0;b.pitch=20;
    auto interpolated=interpolate(a,b,.5);check.vec(interpolated.position,{1,2,3},"position interpolation");
    check.near(interpolated.yaw,-180,"shortest yaw interpolation");check.near(interpolated.pitch,10,"pitch interpolation");
    auto rendered=simulation.render_camera(.5);
    check.vec(rendered.position,(simulation.previous.position+simulation.current.position)*.5,"render interpolates tick snapshots");
    input.mouse_x=100;input.mouse_y=-40;input.clear();input.mouse_x=100;input.mouse_y=-40;
    simulation.reset(Camera{});simulation.tick(input);simulation.tick(input);
    check.near(simulation.current.yaw,15,"mouse consumed once across catch-up ticks");
    check.near(simulation.current.pitch,-2,"positive mouse pitch sign");
    Counters counters;
    {
        Ticket first(counters,ResourceKind::Vbo);Ticket moved(std::move(first));
        check.require(counters.live[3]==1,"moved resource single owner");
        Ticket target(counters,ResourceKind::Ebo);target=std::move(moved);
        check.require(counters.live[3]==1&&counters.live[4]==0,"move assignment releases old owner");
    }
    check.require(counters.empty(),"RAII counters zero");
    check.require(counters.created==counters.destroyed,"RAII creation/deletion balance");
    return check.passed;
}
}
