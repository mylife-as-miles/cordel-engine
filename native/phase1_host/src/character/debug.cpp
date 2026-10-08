// Copyright (c) 2026 CORDEL contributors. MIT.
// CPU geometry extraction only. No physics queries or movement in rendering.
#include "cordel/motor_debug.hpp"
namespace cordel {
namespace {
Vec3 rotate(physics::Quaternion q,Vec3 p) {Vec3 v{q.x,q.y,q.z};return p+cross(v,p)*(2*q.w)+cross(v,cross(v,p))*2;}
Mesh box_mesh(std::string name,Vec3 half,physics::Quaternion q={},std::array<float,4> color={.55f,.55f,.6f,1}) {
    Mesh m;m.name=std::move(name);m.color=color;
    const Vec3 normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const int faces[][4]={{1,3,7,5},{0,4,6,2},{2,6,7,3},{0,1,5,4},{4,5,7,6},{0,2,3,1}};
    for(int f=0;f<6;++f) {unsigned base=unsigned(m.vertices.size());auto n=rotate(q,normals[f]);
        for(int i:faces[f]) {Vec3 p=rotate(q,{i&1?half.x:-half.x,i&2?half.y:-half.y,i&4?half.z:-half.z});m.vertices.push_back({{float(p.x),float(p.y),float(p.z)},{float(n.x),float(n.y),float(n.z)}});}
        for(unsigned i:{0u,1u,2u,0u,2u,3u}) m.indices.push_back(base+i);
    }return m;
}
Mat4 translation(Vec3 p) {auto m=identity;m[3]=p.x;m[7]=p.y;m[11]=p.z;return m;}
void line(Renderer&r,std::string name,Vec3 a,Vec3 b,std::array<float,4> color) {
    Vec3 d=b-a;auto m=identity;
    // Unit thin box's local Y is the segment. Translation and scale are visual.
    Vec3 y=d*.5,x=cross(d,{0,0,1});if(length(x)<1e-6)x={1,0,0};x=x*(.01/std::max(1e-6,length(x)));Vec3 z=cross(x,d);z=z*(.01/std::max(1e-6,length(z)));
    m[0]=x.x;m[4]=x.y;m[8]=x.z;m[1]=y.x;m[5]=y.y;m[9]=y.z;m[2]=z.x;m[6]=z.y;m[10]=z.z;
    auto c=(a+b)*.5;m[3]=c.x;m[7]=c.y;m[11]=c.z;r.set_transform(name,m,color);
}
}
SceneData motor_debug_scene() {
    SceneData scene;
    auto fixture=physics::collision_fixture();auto extras=character::motor_fixture_extensions();fixture.insert(fixture.end(),extras.begin(),extras.end());
    for(auto b:fixture) {auto m=box_mesh(b.name,native_vector(b.half),b.rotation,b.layer==physics::CollisionLayer::Sensor?std::array<float,4>{.9f,.4f,.1f,1}:std::array<float,4>{.55f,.55f,.6f,1});m.model=translation(native_vector(b.center));scene.meshes.push_back(std::move(m));}
    // Low-poly reference capsule: .35 m hemispheres joined by 1.10 m cylinder.
    Mesh m;m.name="motor_capsule";m.color={.2f,.8f,.3f,1};constexpr unsigned segments=16,rings=14;
    for(unsigned j=0;j<=rings;++j) {double a=-std::numbers::pi/2+std::numbers::pi*j/rings;double y=.35*std::sin(a)+(j<rings/2?-.55:.55);double r=.35*std::cos(a);
        for(unsigned i=0;i<=segments;++i){double b=2*std::numbers::pi*i/segments;double x=r*std::cos(b),z=r*std::sin(b);m.vertices.push_back({{float(x),float(y),float(z)},{float(std::cos(a)*std::cos(b)),float(std::sin(a)),float(std::cos(a)*std::sin(b))}});}
    }
    for(unsigned j=0;j<rings;++j)for(unsigned i=0;i<segments;++i){unsigned a=j*(segments+1)+i,b=a+segments+1;for(unsigned v:{a,b,a+1,a+1,b,b+1})m.indices.push_back(v);}
    scene.meshes.push_back(std::move(m));
    for(auto name:{"ground_normal","ground_probe","desired_move","actual_move","collision_0","collision_1","collision_2","collision_3","collision_4","collision_5","step_candidate"})scene.meshes.push_back(box_mesh(name,{1,1,1}));
    return scene;
}
void update_motor_debug(Renderer&r,const character::CharacterMotor&m,double alpha) {
    auto p=native_vector(m.interpolated(alpha));auto s=m.state();auto d=m.diagnostics();
    r.set_transform("motor_capsule",translation(p),s.grounded?std::array<float,4>{.2f,.8f,.3f,1}:std::array<float,4>{.9f,.3f,.2f,1});
    line(r,"ground_normal",p,p+native_vector(s.ground_normal),{.2f,1,.2f,1});
    line(r,"ground_probe",p,native_vector(d.probe_end),{1,1,.1f,1});
    line(r,"desired_move",p,p+native_vector(d.desired_displacement)*8,{.2f,.4f,1,1});
    line(r,"actual_move",p,p+native_vector(d.actual_displacement)*8,{.2f,1,1,1});
    for(unsigned i=0;i<6;++i){auto name="collision_"+std::to_string(i);r.set_visible(name,i<d.normal_count);if(i<d.normal_count)line(r,name,p,p+native_vector(d.collision_normals[i]),{1,.1f,.3f,1});}
    r.set_visible("step_candidate",s.step_result==character::StepResult::Accepted);if(s.step_result==character::StepResult::Accepted) {auto marker=translation(native_vector(d.step_candidate));marker[0]=marker[5]=marker[10]=.035;r.set_transform("step_candidate",marker,{1,.6f,.1f,1});}
}
}
