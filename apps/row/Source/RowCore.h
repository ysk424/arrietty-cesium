#pragma once
// Engine-independent rowing/telemetry model. SI units throughout.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include "RowTracking.h"

namespace row {
constexpr double Pi = 3.14159265358979323846;
struct Field {
    double value=0, received=-1e9;
    bool fresh(double now, double ttl=3.0) const { return now>=received && now-received<ttl; }
    void set(double v,double now) { value=v; received=now; }
};
struct Telemetry {
    Field power, pace, distance, elapsed, strokeRate, strokeCount, resistance, heart;
    unsigned packets=0, rejected=0;
};
struct PowerSample {
    double machineWatts=-1,baseWatts=0,resistance=-1,multiplier=1,gameWatts=0;
    bool usingBt=false;
};
// User-selected game gain, not a measurement of human power. Keep the machine's
// original watts separate. Only a fresh, valid Q1S dial level can change gain.
inline PowerSample samplePower(const Telemetry& t,double now,double barVelocity) {
    PowerSample p;
    p.usingBt=t.power.fresh(now) && std::isfinite(t.power.value) && t.power.value>=0;
    if(p.usingBt) p.machineWatts=t.power.value;
    p.baseWatts=p.usingBt?std::clamp(p.machineWatts,0.,1000.):
        std::isfinite(barVelocity)?std::clamp(-barVelocity*90.,0.,240.):0.;
    if(t.resistance.fresh(now) && std::isfinite(t.resistance.value) &&
        t.resistance.value>=1 && t.resistance.value<=16 && std::floor(t.resistance.value)==t.resistance.value) {
        p.resistance=t.resistance.value; p.multiplier=p.resistance;
    }
    p.gameWatts=p.baseWatts*p.multiplier;
    return p;
}
// FTMS 1.0 section 4.8: More Data=1 omits the mandatory rate/count pair.
// Each field has its own timestamp: another fragment must not revive stale power.
inline bool parseRower(const uint8_t* data,size_t size,double now,Telemetry& out) {
    if(size<2) { ++out.rejected; return false; }
    size_t at=0; bool ok=true;
    auto u=[&](size_t n)->uint32_t {
        if(n>size-at) { ok=false; return 0; }
        uint32_t v=0; for(size_t i=0;i<n;++i) v|=uint32_t(data[at++])<<(8*i); return v;
    };
    const auto flags=u(2);
    if(flags & 0xe000) { ++out.rejected; return false; }
    Telemetry next=out;
    if(!(flags&1)) { next.strokeRate.set(u(1)*.5,now); next.strokeCount.set(u(2),now); }
    if(flags&2) u(1);
    if(flags&4) next.distance.set(u(3),now);
    if(flags&8) { auto v=u(2); next.pace.set(v==65535?-1:double(v),now); }
    if(flags&16) u(2);
    if(flags&32) { const auto v=u(2); next.power.set(v==0x7fff?-1:double(int16_t(v)),now); }
    if(flags&64) u(2);
    if(flags&128) next.resistance.set(int16_t(u(2)),now);
    if(flags&256) u(2),u(2),u(1);
    if(flags&512) { auto v=u(1); next.heart.set(v>0?double(v):-1,now); }
    if(flags&1024) u(1);
    if(flags&2048) next.elapsed.set(u(2),now);
    if(flags&4096) u(2);
    if(!ok || at!=size) { ++out.rejected; return false; }
    ++next.packets; out=next; return true;
}
inline std::optional<int> parseHeart(const uint8_t* p,size_t n) {
    if(n<2 || ((p[0]&1) && n<3)) return {};
    // Sensor contact supported but not detected -> unknown, not a zero reading.
    if((p[0]&4) && !(p[0]&2)) return {};
    const int bpm=(p[0]&1)?int(p[1])+(int(p[2])<<8):p[1];
    size_t at=(p[0]&1)?3:2;
    if(p[0]&8) at+=2;
    if(at>n || ((p[0]&16) && (n-at)%2)) return {};
    return bpm>0 && bpm<256?std::optional<int>(bpm):std::nullopt;
}
struct Vec3 { double x=0,y=0,z=0; };
inline Vec3 operator-(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
struct Pose { Vec3 position,forward{1,0,0}; bool valid=false; double received=-1e9; };
enum class State { Ready, Running, Paused, TrackingLost, Calibrating };
struct Input { Pose bar,head; Telemetry telemetry; double now=0; };
struct SteeringFrame { Vec3 forward{1,0,0},center; bool valid=false; };
struct Model {
    static constexpr double StraightMargin=.08,FullLean=.20,FullTurnRadius=10.;
    State state=State::Ready;
    double speed=0,distance=0,elapsed=0,heading=0,yawRate=0;
    double barPosition=0,barVelocity=0,lean=0,steer=0,drive=0,power=0;
    unsigned strokes=0;
    bool usingBt=false;
    Vec3 forward{1,0,0},right{0,1,0},neutralHead;
    double previousBar=0,extreme=0,pullTravel=0,recoveryTravel=0;
    bool initialized=false,pulling=false,armed=false;
    BarTracking barTracking;
    static bool headTracking(const Input& in) {
        return in.head.valid && finite(in.head.position) && finite(in.head.forward)
            && in.now-in.head.received<.25 && in.now>=in.head.received;
    }
    static bool barTracked(const Input& in) {
        return in.bar.valid && finite(in.bar.position)
            && in.now-in.bar.received<.25 && in.now>=in.bar.received;
    }
    static bool tracking(const Input& in) {
        return barTracked(in) && headTracking(in);
    }
    bool start(const Input& in,const SteeringFrame& frame) {
        if(!tracking(in) || !frame.valid || !finite(frame.forward) || !finite(frame.center)) return false;
        // Only a measured machine axis and averaged neutral may start a session.
        // The caller supplies world heading separately; gaze never defines steering.
        auto f=frame.forward; const double len=std::hypot(f.x,f.y);
        if(len<.2) return false;
        forward={f.x/len,f.y/len,0}; right={-forward.y,forward.x,0};
        neutralHead=frame.center;
        previousBar=dot(in.bar.position,forward);
        barPosition=previousBar; extreme=previousBar;
        barTracking.start(previousBar,dot(in.head.position,forward),in.now);
        barVelocity=lean=steer=drive=power=yawRate=0; initialized=true;
        pullTravel=recoveryTravel=0; pulling=false; armed=true; state=State::Running; return true;
    }
    void pause() { if(state==State::Running) state=State::Paused; speed=drive=power=yawRate=0; }
    void calibrate() { pause(); state=State::Calibrating; lean=steer=0; }
    void reset() { *this=Model{}; }
    // <=20ms substeps in caller; long frame gaps stop rather than launch the boat.
    double tick(const Input& in,double dt) {
        if(state!=State::Running) return 0;
        if(!headTracking(in) || !std::isfinite(dt) || dt<=0 || dt>.1) {
            barTracking.issue=!headTracking(in)?TrackingIssue::HeadLost:TrackingIssue::FrameGap;
            state=State::TrackingLost; speed=drive=power=yawRate=0; return 0;
        }
        const auto oldSource=barTracking.source;
        const bool tracked=barTracked(in);
        if(!barTracking.tick(tracked?dot(in.bar.position,forward):0,tracked,
            dot(in.head.position,forward),in.now,dt)) {
            state=State::TrackingLost; speed=drive=power=yawRate=0; return 0;
        }
        const double raw=barTracking.delta/dt;
        barPosition=previousBar=barTracking.position;
        const bool measured=barTracking.source==BarSource::Tracker && oldSource==BarSource::Tracker;
        if(!measured) {
            // Estimated movement and return offsets cannot add stroke counts.
            // Re-arm only after a measured recovery once tracking has returned.
            pullTravel=recoveryTravel=0; armed=pulling=false;
        }
        if(barTracking.source==BarSource::Coast || barTracking.source==BarSource::Reacquiring ||
            (barTracking.source==BarSource::Tracker && oldSource!=BarSource::Tracker)) barVelocity=0;
        barVelocity+=(raw-barVelocity)*(1-std::exp(-dt/.06));
        const double lateral=dot(in.head.position-neutralHead,right);
        lean+=(std::clamp(lateral,-.35,.35)-lean)*(1-std::exp(-dt/.28));
        const double amount=std::clamp((std::abs(lean)-StraightMargin)/(FullLean-StraightMargin),0.,1.);
        steer=std::copysign(amount*amount,lean); // Gentle onset just outside the straight zone.
        // Keep a roughly 10 m full-steer radius as game watts raise speed.
        // Preserve the old low-speed authority and fade to zero at a standstill.
        const double fullYaw=std::clamp(speed/FullTurnRadius,.20,.55)*std::clamp(speed/1.2,0.,1.);
        const double targetYaw=steer*fullYaw;
        // Returning to the straight zone removes steering immediately. Ramp into
        // turns gently, without a lingering turn after the center display lights.
        if(amount==0) yawRate=0;
        else yawRate+=(targetYaw-yawRate)*(1-std::exp(-dt/.35));
        heading+=yawRate*dt;
        if(measured && barVelocity>.10) {
            recoveryTravel+=std::max(0.,raw*dt);
            if(recoveryTravel>.12) { armed=true; pulling=false; pullTravel=0; }
        }
        if(measured && barVelocity<-.10) {
            pullTravel+=std::max(0.,-raw*dt);
            if(armed && !pulling && pullTravel>.08) {
                ++strokes; pulling=true; armed=false; recoveryTravel=0;
            }
        }
        drive=std::clamp((-barVelocity-.05)/.65,0.,1.);
        const auto output=samplePower(in.telemetry,in.now,barVelocity);
        usingBt=output.usingBt;
        // Handle motion gates power: frozen positive FTMS power never propels a
        // stationary handle. The 2.3 drive factor accounts for recovery duty.
        power=output.gameWatts;
        const double thrust=(usingBt?power*2.3:power)*drive/std::max(.9,speed);
        const double drag=5.5*speed*speed+4.*speed;
        const double old=speed;
        speed=std::clamp(speed+std::clamp((thrust-drag)/105.,-1.1,1.25)*dt,0.,5.5);
        const double moved=(old+speed)*.5*dt;
        distance+=moved; elapsed+=dt; return moved;
    }
};
}
