// Copyright (c) 2026 CORDEL contributors. MIT; see ../../LICENSE.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace cordel {
struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
};
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline double length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
using Mat4 = std::array<double,16>; // row-major; column vectors; P * V * M * p
inline constexpr Mat4 identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 c{};
    for (int r=0;r<4;++r) for (int col=0;col<4;++col)
        for (int k=0;k<4;++k) c[r*4+col]+=a[r*4+k]*b[k*4+col];
    return c;
}
inline std::array<double,4> transform(const Mat4& m, Vec3 p) {
    return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],
            m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],
            m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11],
            m[12]*p.x+m[13]*p.y+m[14]*p.z+m[15]};
}
inline double radians(double d) { return d*std::numbers::pi/180; }
inline double wrap_degrees(double d) {
    double result=std::fmod(d+180,360);
    if(result<0) result+=360;
    return result-180;
}
inline Mat4 perspective(double fov, double aspect, double near, double far) {
    if(!(std::isfinite(fov)&&std::isfinite(aspect)&&std::isfinite(near)&&std::isfinite(far)
         &&fov>0&&fov<180&&aspect>0&&near>0&&far>near))
        throw std::invalid_argument("Invalid perspective frustum");
    double f=1/std::tan(radians(fov)/2);
    return {f/aspect,0,0,0, 0,f,0,0, 0,0,(far+near)/(near-far),2*far*near/(near-far), 0,0,-1,0};
}
enum class Action { Forward, Backward, Left, Right, Up, Down, Sprint };
struct Actions {
    std::map<int,Action> keys;
    void down(int key,Action action) { keys.insert_or_assign(key,action); }
    void up(int key) { keys.erase(key); }
    bool held(Action a) const {
        return std::any_of(keys.begin(),keys.end(),[a](auto pair){return pair.second==a;});
    }
    void clear() { keys.clear(); }
    bool empty() const { return keys.empty(); }
};
inline std::array<double,2> analog_pair(double x,double y,double deadzone=.15) {
    if(!(deadzone>=0&&deadzone<1)) throw std::invalid_argument("Invalid deadzone");
    if(!std::isfinite(x)||!std::isfinite(y)) return {};
    double len=std::hypot(x,y);
    if(len<=deadzone) return {};
    double magnitude=(std::min(len,1.)-deadzone)/(1-deadzone);
    return {x*magnitude/len,y*magnitude/len};
}
struct Input {
    Actions actions;
    std::array<double,2> axes{};
    double mouse_x{},mouse_y{};
    bool focused{}, captured{}, stall{}, quit{};
    void clear() { actions.clear(); axes={}; mouse_x=mouse_y=0; }
    void lose_focus() { focused=false; captured=false; clear(); }
};
struct Camera {
    Vec3 position{0,2.5,6};
    double yaw{},pitch{-8},field_of_view{65},near_plane{.05},far_plane{100};
    double movement_speed{3},mouse_sensitivity{.15},sprint_multiplier{3};
    Vec3 forward() const {
        double y=radians(yaw),p=radians(pitch);
        return {std::sin(y)*std::cos(p),std::sin(p),-std::cos(y)*std::cos(p)};
    }
    Vec3 right() const { double y=radians(yaw); return {std::cos(y),0,std::sin(y)}; }
    void look(double dx,double dy) {
        if(!std::isfinite(dx)||!std::isfinite(dy)) return;
        yaw=wrap_degrees(yaw+dx*mouse_sensitivity);
        pitch=std::clamp(pitch-dy*mouse_sensitivity,-85.,85.);
    }
    void move(const Actions& actions,double dt,std::array<double,2> analog={}) {
        if(!(std::isfinite(dt)&&dt>0)) return;
        double f=double(actions.held(Action::Forward))-actions.held(Action::Backward)-analog[1];
        double r=double(actions.held(Action::Right))-actions.held(Action::Left)+analog[0];
        double u=double(actions.held(Action::Up))-actions.held(Action::Down);
        Vec3 direction=forward()*f+right()*r+Vec3{0,u,0};
        double scale=movement_speed*dt/std::max(1.,length(direction));
        if(actions.held(Action::Sprint)) scale*=sprint_multiplier;
        position=position+direction*scale;
    }
    Mat4 view() const {
        Vec3 r=right(),f=forward(),u=cross(r,f),b=f*-1;
        return {r.x,r.y,r.z,-dot(r,position),u.x,u.y,u.z,-dot(u,position),
                b.x,b.y,b.z,-dot(b,position),0,0,0,1};
    }
    Mat4 projection(int w,int h) const {
        return perspective(field_of_view,double(std::max(1,w))/std::max(1,h),near_plane,far_plane);
    }
};
inline Camera interpolate(const Camera& a,const Camera& b,double alpha) {
    Camera result=b;
    alpha=std::clamp(alpha,0.,1.);
    result.position=a.position*(1-alpha)+b.position*alpha;
    // Shortest yaw arc handles +179 -> -179 without spinning backwards.
    result.yaw=wrap_degrees(a.yaw+wrap_degrees(b.yaw-a.yaw)*alpha);
    result.pitch=a.pitch+(b.pitch-a.pitch)*alpha;
    return result;
}
struct FixedStep {
    double raw{},simulated{},dropped{},alpha{};
    unsigned ticks{};
};
class FixedClock {
    double accumulator_{};
public:
    static constexpr double dt=1./60.;
    static constexpr unsigned max_ticks=3;
    FixedStep advance(double elapsed) {
        FixedStep step;
        step.raw=std::isfinite(elapsed)&&elapsed>0?elapsed:0;
        accumulator_+=step.raw;
        // Bound before integer conversion; avoids overflow for extreme samples.
        while(accumulator_+1e-12>=dt&&step.ticks<max_ticks) {
            accumulator_-=dt; ++step.ticks;
        }
        if(accumulator_+1e-12>=dt) {
            double remaining=std::fmod(std::max(0.,accumulator_),dt);
            step.dropped=accumulator_-remaining;
            accumulator_=remaining;
        }
        accumulator_=std::max(0.,accumulator_);
        step.simulated=step.ticks*dt;
        step.alpha=std::clamp(accumulator_/dt,0.,std::nextafter(1.,0.));
        return step;
    }
    void reset() { accumulator_=0; }
};
struct Simulation {
    Camera previous,current;
    void tick(Input& input) {
        previous=current;
        if(input.focused) {
            // Mouse deltas accumulate across render frames; consume once, not
            // once per catch-up tick. Pose rotation also changes only on ticks.
            current.look(std::exchange(input.mouse_x,0),std::exchange(input.mouse_y,0));
            current.move(input.actions,FixedClock::dt,analog_pair(input.axes[0],input.axes[1]));
        }
    }
    Camera render_camera(double alpha) const { return interpolate(previous,current,alpha); }
    void reset(Camera c={}) { previous=current=c; }
};
enum class ResourceKind : std::size_t { Program,Shader,Vao,Vbo,Ebo,Scene,Count };
struct Counters {
    std::array<int,static_cast<std::size_t>(ResourceKind::Count)> live{},created{},destroyed{};
    bool empty() const { return std::all_of(live.begin(),live.end(),[](int n){return n==0;}); }
    void acquire(ResourceKind k) { auto i=static_cast<std::size_t>(k); ++live[i]; ++created[i]; }
    void release(ResourceKind k) { auto i=static_cast<std::size_t>(k); --live[i]; ++destroyed[i]; }
};
// Noncopyable accounting token. GL owners supply actual deletion separately.
class Ticket {
    Counters* counters_{};
    ResourceKind kind_{};
public:
    Ticket()=default;
    Ticket(Counters& c,ResourceKind k):counters_(&c),kind_(k) { c.acquire(k); }
    ~Ticket() { reset(); }
    Ticket(const Ticket&)=delete;
    Ticket& operator=(const Ticket&)=delete;
    Ticket(Ticket&& b) noexcept: counters_(std::exchange(b.counters_,nullptr)),kind_(b.kind_) {}
    Ticket& operator=(Ticket&& b) noexcept {
        if(this!=&b) { reset(); counters_=std::exchange(b.counters_,nullptr); kind_=b.kind_; }
        return *this;
    }
    void reset() { if(counters_) { counters_->release(kind_); counters_=nullptr; } }
};
} // namespace cordel
