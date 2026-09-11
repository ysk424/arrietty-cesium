#pragma once
#include "RowCore.h"
#include <vector>

namespace row {
enum class CalibrationPhase { Idle, Settle, Center, Axis, Complete, Failed };
enum class CalibrationIssue { None, Tracking, Timeout, LookForward, KeepStraight };

// Runs only while the boat is stopped. All measurements stay in the physical
// tracking frame; neither virtual boat yaw nor the rider's gaze rotates the fit.
class Calibration {
public:
    static constexpr double SettleSeconds=2,CenterSeconds=1,TimeoutSeconds=40;
    CalibrationPhase phase=CalibrationPhase::Idle;
    CalibrationIssue issue=CalibrationIssue::None;
    SteeringFrame frame;
    double remaining=SettleSeconds;
    unsigned strokes=0;
    bool begin(const Input& in) {
        *this=Calibration{};
        if(!Model::tracking(in)) { fail(CalibrationIssue::Tracking); return false; }
        phase=CalibrationPhase::Settle; return true;
    }
    void tick(const Input& in,double dt) {
        if(phase==CalibrationPhase::Idle || phase==CalibrationPhase::Complete || phase==CalibrationPhase::Failed) return;
        if(!Model::tracking(in) || !std::isfinite(dt) || dt<=0 || dt>.1) { fail(CalibrationIssue::Tracking); return; }
        total+=dt;
        if(total>TimeoutSeconds) { fail(CalibrationIssue::Timeout); return; }
        if(phase==CalibrationPhase::Settle) {
            remaining=std::max(0.,SettleSeconds-total);
            if(remaining==0) { phase=CalibrationPhase::Center; clearCenter(in); }
            return;
        }
        if(phase==CalibrationPhase::Center) {
            // Require a continuous quiet second, not the transient keypad reach.
            for(int i=0;i<3;++i) {
                const double p=i==0?in.head.position.x:i==1?in.head.position.y:in.head.position.z;
                low[i]=std::min(low[i],p); high[i]=std::max(high[i],p);
                if(high[i]-low[i]>.03) { clearCenter(in); return; }
            }
            centerTime+=dt;
            centerSum.x+=in.head.position.x*dt; centerSum.y+=in.head.position.y*dt; centerSum.z+=in.head.position.z*dt;
            facingSum.x+=in.head.forward.x*dt; facingSum.y+=in.head.forward.y*dt;
            remaining=std::max(0.,CenterSeconds-centerTime);
            if(remaining==0) {
                const double len=std::hypot(facingSum.x,facingSum.y);
                if(len/centerTime<.2) { issue=CalibrationIssue::LookForward; clearCenter(in); return; }
                facing={facingSum.x/len,facingSum.y/len,0};
                frame.center={centerSum.x/centerTime,centerSum.y/centerTime,centerSum.z/centerTime};
                phase=CalibrationPhase::Axis; issue=CalibrationIssue::None;
            }
            return;
        }
        axisTime+=dt;
        // Keep distinct, bounded-rate samples even if the render loop runs fast.
        if(!samples.empty() && in.bar.received-lastSample<1./120.) return;
        if(!samples.empty()) {
            const auto d=in.bar.position-samples.back();
            if(std::sqrt(dot(d,d))/(in.bar.received-lastSample)>4.5) { fail(CalibrationIssue::Tracking); return; }
        }
        lastSample=in.bar.received; samples.push_back(in.bar.position);
        if(axisTime-nextFit>=.2) { nextFit=axisTime; fitAxis(); }
    }
private:
    double total=0,centerTime=0,axisTime=0,nextFit=0,lastSample=0;
    double low[3]{},high[3]{};
    Vec3 centerSum,facingSum,facing;
    std::vector<Vec3> samples;
    void fail(CalibrationIssue reason) { phase=CalibrationPhase::Failed; issue=reason; frame.valid=false; }
    void clearCenter(const Input& in) {
        centerTime=0; centerSum=facingSum={}; remaining=CenterSeconds;
        low[0]=high[0]=in.head.position.x; low[1]=high[1]=in.head.position.y; low[2]=high[2]=in.head.position.z;
    }
    void fitAxis() {
        if(samples.size()<30) return;
        // Horizontal principal axis of the full out-and-back trajectory.
        double x=0,y=0; for(auto p:samples) { x+=p.x; y+=p.y; }
        x/=samples.size(); y/=samples.size();
        double xx=0,xy=0,yy=0;
        for(auto p:samples) { const double a=p.x-x,b=p.y-y; xx+=a*a; xy+=a*b; yy+=b*b; }
        const double spread=std::hypot(xx-yy,2*xy),major=(xx+yy+spread)*.5,minor=(xx+yy-spread)*.5;
        if(major/samples.size()<.008) return;
        const double angle=.5*std::atan2(2*xy,xx-yy);
        Vec3 axis{std::cos(angle),std::sin(angle),0};
        if(minor/major>.04) { issue=CalibrationIssue::KeepStraight; strokes=0; return; }
        // Gaze chooses only the sign of the measured line, never its angle.
        const double alignment=dot(axis,facing);
        if(std::abs(alignment)<.5) { issue=CalibrationIssue::LookForward; return; }
        if(alignment<0) { axis.x=-axis.x; axis.y=-axis.y; }
        issue=CalibrationIssue::None;
        double anchor=dot(samples.front(),axis),extreme=anchor,lo=anchor,hi=anchor;
        int direction=0; unsigned halves=0;
        for(auto p:samples) {
            const double value=dot(p,axis); lo=std::min(lo,value); hi=std::max(hi,value);
            if(direction==0) {
                if(std::abs(value-anchor)>=.30) { direction=value>anchor?1:-1; extreme=value; ++halves; }
            } else if(direction>0) {
                if(value>extreme) extreme=value;
                else if(extreme-value>=.30) { direction=-1; extreme=value; ++halves; }
            } else {
                if(value<extreme) extreme=value;
                else if(value-extreme>=.30) { direction=1; extreme=value; ++halves; }
            }
        }
        strokes=std::min(2u,halves/2);
        if(axisTime>=4 && hi-lo>=.40 && halves>=4) {
            frame.forward=axis; frame.valid=true; phase=CalibrationPhase::Complete;
        }
    }
};
}
