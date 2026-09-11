#include "RowCalibration.h"
#include "RowAudio.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int checks=0;
void check(bool ok,const char* message) {
    ++checks; if(!ok) { std::cerr<<"FAIL "<<message<<'\n'; std::exit(1); }
}
row::Input pose(double now,double lateral=0,double angle=0) {
    const row::Vec3 axis{std::cos(angle),std::sin(angle),0},right{-axis.y,axis.x,0};
    const double bar=.4*std::cos(2*row::Pi*now/2.4),head=bar/1.3;
    row::Input in; in.now=now;
    in.bar={{3+axis.x*bar,-2+axis.y*bar,.65},axis,true,now};
    in.head={{3+axis.x*head+right.x*lateral,-2+axis.y*head+right.y*lateral,1},axis,true,now};
    in.telemetry.power.set(80,now); in.telemetry.resistance.set(6,now); return in;
}
row::Model warm(double angle=0) {
    row::Model m;
    m.start(pose(0,0,angle),{{std::cos(angle),std::sin(angle),0},{3,-2,1},true});
    for(int i=1;i<=540;++i) m.tick(pose(i*.01,0,angle),.01);
    return m;
}
}
int main() {
    auto m=warm();
    check(m.state==row::State::Running && m.barTracking.learned,"visible paired movement enables assistance");
    check(std::abs(m.barTracking.gain-1.3)<1e-8,"learned relation uses calibrated fore-aft position");
    const auto before=m;
    row::AudioMix audio; audio.tick(m,.01);
    bool assisted=false,noCatch=true;
    for(int i=541;i<=590;++i) {
        auto in=pose(i*.01); in.bar.valid=false;
        m.tick(in,.01); audio.tick(m,.01);
        assisted|=m.barTracking.source==row::BarSource::HmdAssist;
        noCatch &= !audio.catchNow;
    }
    check(assisted && m.state==row::State::Running && m.drive>0,"short occlusion follows HMD drive without Enter");
    check(m.elapsed>before.elapsed && m.distance>before.distance,"short occlusion keeps exercise and boat moving");
    check(m.strokes==before.strokes && noCatch,"estimated movement cannot add strokes or catch sounds");
    check(std::abs(m.barPosition-row::dot(pose(5.9).bar.position,m.forward))<.001,"paired synthetic head predicts missing bar");
    const auto strokeCount=m.strokes;
    double peakReturnDrive=0;
    for(int i=591;i<=610;++i) {
        auto in=pose(i*.01); in.bar.position.x+=.25;
        m.tick(in,.01); peakReturnDrive=std::max(peakReturnDrive,m.drive);
    }
    check(m.state==row::State::Running && m.barTracking.source==row::BarSource::Tracker,"stable same-Tracker return resumes automatically");
    check(m.strokes==strokeCount && peakReturnDrive<.01,"return position offset cannot create a pull");
    check(!m.barTracking.learned,"reacquisition discards old fit, never trains on estimates");
    check(row::dot(m.forward,before.forward)==1 && row::dot(m.neutralHead-before.neutralHead,m.neutralHead-before.neutralHead)==0,
        "occlusion and return preserve calibrated axis and neutral");

    for(int reason=0;reason<5;++reason) {
        m=warm(); auto in=pose(5.41);
        if(reason==0) in.head.valid=false;
        if(reason==1) in.head.received-=.3;
        if(reason==2) in.head.position.x=std::numeric_limits<double>::quiet_NaN();
        if(reason==3) in.head.received+=1;
        if(reason==4) in.head.forward.y=std::numeric_limits<double>::infinity();
        m.tick(in,.01);
        check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::HeadLost && m.drive==0 && m.speed==0,
            "invalid or stale HMD cannot support an occlusion");
    }
    m=warm(); auto in=pose(5.41); in.bar.valid=false; m.tick(in,.2);
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::FrameGap,"hitch watchdog remains independent of occlusion support");
    m=warm(); in=pose(5.41); in.bar.position.x+=2; m.tick(in,.01);
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::BarJump,"valid bar discontinuity is not concealed by HMD assist");
    m=warm(); in=pose(5.41); in.bar.valid=false; in.head.position.x+=2; m.tick(in,.01);
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::HeadJump,"head recenter during loss cannot generate movement");

    for(int invalidBar=0;invalidBar<4;++invalidBar) {
        m=warm(); in=pose(5.41);
        if(invalidBar==0) in.bar.valid=false;
        if(invalidBar==1) in.bar.received-=.3;
        if(invalidBar==2) in.bar.position.y=std::numeric_limits<double>::quiet_NaN();
        if(invalidBar==3) in.bar.received+=1;
        m.tick(in,.01);
        check(m.state==row::State::Running && m.barTracking.source==row::BarSource::HmdAssist,"missing invalid or stale Tracker enters explicit assist");
    }
    m=warm();
    for(int i=541;i<=680;++i) { in=pose(i*.01); in.bar.valid=false; m.tick(in,.01); }
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::BarTimeout && m.speed==0,"long loss stops after bounded grace period");
    const double stoppedTime=m.elapsed,stoppedDistance=m.distance;
    m.tick(pose(6.81),.01);
    check(m.state==row::State::TrackingLost && m.elapsed==stoppedTime && m.distance==stoppedDistance,"late return cannot auto restart a stopped session");

    m=warm();
    for(int i=541;i<=680;++i) { in=pose(i*.01); in.bar.valid=i%5==0; m.tick(in,.01); }
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::BarTimeout,"flickering good poses cannot indefinitely extend the grace period");
    m=warm();
    for(int i=541;i<=550;++i) { in=pose(i*.01); in.bar.valid=false; m.tick(in,.01); }
    in=pose(5.51); in.bar.position.x+=2; m.tick(in,.01);
    check(m.state==row::State::TrackingLost && m.barTracking.issue==row::TrackingIssue::BarJump,"teleported return does not silently change the physical frame");

    row::Model untrained; in=pose(0); untrained.start(in,{{1,0,0},{3,-2,1},true}); untrained.speed=2;
    for(int i=1;i<=30;++i) { in=pose(i*.01); in.bar.valid=false; untrained.tick(in,.01); }
    check(untrained.state==row::State::Running && untrained.barTracking.source==row::BarSource::Coast && untrained.drive==0,
        "untrained loss coasts without inventing a head-to-bar relation");
    check(untrained.speed<2 && untrained.speed>0 && untrained.strokes==0,"untrained grace only decelerates, even with positive BT power");
    untrained.pause(); const double paused=untrained.elapsed;
    untrained.tick(pose(.31),.01);
    check(untrained.state==row::State::Paused && untrained.elapsed==paused,"Enter pause cannot be undone by tracker return");
    untrained.start(pose(.32),{{1,0,0},{3,-2,1},true});
    check(!untrained.barTracking.learned && untrained.barTracking.source==row::BarSource::Tracker,"resume resets the assistance fit");
    untrained.reset(); check(untrained.state==row::State::Ready && !untrained.barTracking.learned,"stop clears assistance state");

    m=warm(); const auto fixedHead=pose(5.4).head.position;
    for(int i=541;i<=635;++i) { in=pose(i*.01); in.bar.valid=false; in.head.position=fixedHead; m.tick(in,.01); }
    check(m.state==row::State::Running && m.drive<1e-5 && std::abs(m.barPosition-before.barPosition)<1e-8,
        "stopped head cannot extrapolate a continuing pull from cached BT watts");
    m=warm(); in=pose(5.41); in.bar.valid=false; m.tick(in,.01);
    in=pose(5.42); m.tick(in,.01); const auto returnHead=in.head.position;
    for(int i=543;i<=570;++i) { in=pose(i*.01); in.bar.valid=false; in.head.position=returnHead; m.tick(in,.01); }
    check(m.barTracking.source==row::BarSource::Coast && m.drive==0,"flicker after partial return coasts instead of pulling from its correction offset");

    for(double angle:{0.,1.2}) {
        m=warm(angle);
        for(int i=541;i<=590;++i) { in=pose(i*.01,0,angle); in.bar.valid=false; in.head.forward={0,-1,0}; m.tick(in,.01); }
        check(std::abs(m.heading)<1e-10 && std::abs(m.lean)<1e-10,"fore-aft assist and head yaw never steer, including rotated machine axis");
        m=warm(angle);
        for(int i=541;i<=630;++i) { in=pose(i*.01,.079,angle); in.bar.valid=false; m.tick(in,.01); }
        check(m.steer==0 && m.heading==0,"8 cm straight zone remains during assistance");
        m=warm(angle);
        for(int i=541;i<=630;++i) { in=pose(i*.01,.2,angle); in.bar.valid=false; m.tick(in,.01); }
        check(m.steer>0 && m.heading>0,"real lateral HMD steering remains available during assistance");
    }
    row::Calibration setup; setup.begin(pose(0)); in=pose(.01); in.bar.valid=false; setup.tick(in,.01);
    check(setup.phase==row::CalibrationPhase::Failed && !setup.frame.valid,"initial calibration still requires the real Tracker");
    m=warm();
    for(int i=541;i<=722;++i) m.tick(pose(i*.01),.01);
    for(int i=723;i<=836;++i) { in=pose(i*.01); in.bar.valid=false; m.tick(in,.01); }
    check(m.state==row::State::Running && m.barTracking.source==row::BarSource::HmdAssist,
        "normal 60 cm fore-aft head excursion is not mistaken for recentering");
    check(std::abs(m.barPosition-row::dot(pose(7.22).bar.position,m.forward))<=.500001,
        "full head excursion still obeys the half-metre estimated bar displacement cap");
    std::cout<<"PASS "<<checks<<" tracking checks\n";
}
