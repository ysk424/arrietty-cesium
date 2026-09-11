#pragma once
// Short, explicitly reported handle occlusion support. Scalar positions are
// projections onto the fixed, calibrated physical machine axis (metres).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace row {
enum class BarSource { Tracker, HmdAssist, Coast, Reacquiring };
inline const char* barSourceName(BarSource source) {
    switch(source) {
    case BarSource::HmdAssist: return "hmd_assist";
    case BarSource::Coast: return "coast";
    case BarSource::Reacquiring: return "reacquiring";
    default: return "tracker";
    }
}
enum class TrackingIssue { None, HeadLost, FrameGap, BarTimeout, BarJump, HeadJump };
inline const char* trackingIssueName(TrackingIssue issue) {
    switch(issue) {
    case TrackingIssue::HeadLost: return "head_lost";
    case TrackingIssue::FrameGap: return "frame_gap";
    case TrackingIssue::BarTimeout: return "bar_timeout";
    case TrackingIssue::BarJump: return "bar_jump";
    case TrackingIssue::HeadJump: return "head_jump";
    default: return "none";
    }
}
struct BarTracking {
    static constexpr double MaxGap=1.25,StableReturn=.15;
    BarSource source=BarSource::Tracker;
    TrackingIssue issue=TrackingIssue::None;
    double position=0,delta=0,gapSeconds=0,gain=1,confidence=0;
    bool learned=false;

    void start(double bar,double head,double now) {
        *this=BarTracking{}; position=lastBar=bar; lastHead=head;
        observe(bar,head,now);
    }
    // Caller must require a fresh HMD, finite projections and dt in (0, .1].
    // Never uses yaw, changes neutral, or selects an alternative tracked device.
    bool tick(double bar,bool valid,double head,double now,double dt) {
        delta=0;
        if(!gap && valid) {
            delta=bar-lastBar;
            if(std::abs(delta/dt)>4.5) { issue=TrackingIssue::BarJump; return false; }
            if(std::abs(delta/dt)>.10) lastMotion=now;
            position=lastBar=bar; lastHead=head;
            observe(bar,head,now); return true;
        }
        if(!gap) {
            gap=true; gapBegan=now; gapSeconds=0; stable=0;
            anchorBar=position; anchorHead=lastHead;
            low=std::min(low,anchorBar); high=std::max(high,anchorBar);
            assist=learned && now-lastMotion<.5;
            source=assist?BarSource::HmdAssist:BarSource::Coast;
        }
        gapSeconds=now-gapBegan;
        if(gapSeconds>=MaxGap) { issue=TrackingIssue::BarTimeout; return false; }
        const double headDelta=head-lastHead;
        if(std::abs(headDelta/dt)>2.5 || std::abs(head-anchorHead)>.75) {
            issue=TrackingIssue::HeadJump; return false;
        }
        lastHead=head;
        if(valid) {
            if(std::abs(bar-anchorBar)>1.2) { issue=TrackingIssue::BarJump; return false; }
            assist=false; // If visibility flickers again, coast until stable.
            // One good pose cannot renew the timeout. Require continuous valid,
            // physically plausible readings from the configured Tracker.
            if(!returning || std::abs((bar-returnBar)/dt)>4.5) stable=0;
            else stable+=dt;
            returning=true; returnBar=bar;
            source=BarSource::Reacquiring;
            // Visual position correction is deliberately excluded from delta:
            // reacquisition coasts and cannot create drive or a catch event.
            position+=(bar-position)*(1-std::exp(-dt/.06));
            if(stable>=StableReturn) {
                position=lastBar=bar; lastHead=head;
                gap=false; gapSeconds=0; source=BarSource::Tracker;
                // Discard pre-gap training: inferred samples never teach the fit.
                samples={}; count=next=0; learned=false; confidence=0;
                observe(bar,head,now);
            }
            return true;
        }
        stable=0; returning=false;
        source=assist?BarSource::HmdAssist:BarSource::Coast;
        if(assist) {
            // Anchor at the last real pair. No velocity extrapolation when the
            // head stops, and no movement beyond the recent measured bar range.
            const double step=std::clamp(gain*headDelta,-2.*dt,2.*dt);
            const double bounded=std::clamp(position+step,std::max(low-.05,anchorBar-.5),std::min(high+.05,anchorBar+.5));
            delta=bounded-position; position=bounded;
        }
        return true;
    }
private:
    struct Sample { double bar=0,head=0,time=-1e9; };
    std::array<Sample,64> samples{};
    size_t count=0,next=0;
    double lastBar=0,lastHead=0,lastMotion=-1e9,gapBegan=0;
    double anchorBar=0,anchorHead=0,low=0,high=0,stable=0,returnBar=0;
    bool gap=false,assist=false,returning=false;
    void observe(double bar,double head,double now) {
        if(count && now-samples[(next+samples.size()-1)%samples.size()].time<.1) return;
        samples[next]={bar,head,now}; next=(next+1)%samples.size(); count=std::min(count+1,samples.size());
        double sumBar=0,sumHead=0,n=0,minHead=head,maxHead=head;
        low=high=bar;
        for(size_t i=0;i<count;++i) if(now-samples[i].time<=6.) {
            const auto& s=samples[i]; sumBar+=s.bar; sumHead+=s.head; ++n;
            low=std::min(low,s.bar); high=std::max(high,s.bar);
            minHead=std::min(minHead,s.head); maxHead=std::max(maxHead,s.head);
        }
        learned=false; confidence=0;
        if(n<10 || high-low<.2 || maxHead-minHead<.12) return;
        double hh=0,bb=0,hb=0;
        for(size_t i=0;i<count;++i) if(now-samples[i].time<=6.) {
            const double h=samples[i].head-sumHead/n,b=samples[i].bar-sumBar/n;
            hh+=h*h; bb+=b*b; hb+=h*b;
        }
        if(hh<1e-6 || bb<1e-6) return;
        confidence=hb/std::sqrt(hh*bb);
        const double fitted=hb/hh;
        learned=confidence>=.8 && fitted>=.5 && fitted<=2.;
        if(learned) gain=fitted;
    }
};
}
