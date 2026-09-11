#include "RowAudio.h"
#include <iostream>
#include <cstdlib>
#include <limits>
static int checks=0;
void check(bool ok,const char* name) { ++checks; if(!ok) { std::cerr<<"FAIL "<<name<<'\n'; std::exit(1); } }
int main() {
    row::AudioMix mix; row::Model model;
    for(int i=0;i<600;++i) mix.tick(model,.01);
    check(mix.gain[1]==0 && mix.gain[2]==0 && !mix.catchNow,"no exercise sounds at rest");
    check(mix.gain[3]>.19 && mix.gain[4]>.12,"quiet water and wind at rest");
    for(const auto state:{row::State::Calibrating,row::State::Paused,row::State::TrackingLost}) {
        model.state=state; model.drive=1; model.speed=5.5; ++model.strokes;
        mix.tick(model,.01);
        check(!mix.catchNow && mix.gain[1]==0 && mix.gain[2]==0,"non-running poses cannot sound like exercise");
    }
    model.state=row::State::Running; model.drive=0; model.speed=0;
    mix.tick(model,.01); check(!mix.catchNow,"start/resume does not replay old strokes");
    ++model.strokes; model.drive=1; model.speed=2;
    mix.tick(model,.01); check(mix.catchNow,"registered stroke produces one catch");
    mix.tick(model,.01); check(!mix.catchNow,"catch is not repeated on every drive frame");
    for(int i=0;i<100;++i) mix.tick(model,.01);
    const double slow=mix.gain[2],pitch=mix.boatPitch;
    check(mix.gain[1]>.33 && slow>0,"drive and motion are audible");
    model.drive=0; model.speed=4;
    for(int i=0;i<200;++i) mix.tick(model,.01);
    check(mix.gain[1]<1e-6 && mix.gain[2]>slow && mix.boatPitch>pitch,"recovery keeps faster coasting water but releases pull");
    model.speed=0;
    for(int i=0;i<600;++i) mix.tick(model,.01);
    check(mix.gain[2]<1e-7,"water-cutting fades to silence at zero speed");
    model.drive=1; model.speed=5.5;
    for(int i=0;i<200;++i) mix.tick(model,.01);
    model.pause(); mix.tick(model,.01);
    check(!mix.catchNow && mix.gain[1]==0 && mix.gain[2]==0,"pause clears drive and boat immediately");
    model.reset(); mix.tick(model,.01);
    model.state=row::State::Running; mix.tick(model,.01); ++model.strokes; mix.tick(model,.01);
    check(mix.catchNow,"first new stroke after reset is audible");
    const double nan=std::numeric_limits<double>::quiet_NaN();
    model.drive=model.speed=nan; mix.tick(model,nan);
    check(std::isfinite(mix.boatPitch) && std::all_of(mix.gain.begin(),mix.gain.end(),[](double x){return std::isfinite(x);}),"bad values cannot corrupt sound gains");
    double max=0; for(double gain:row::AudioMix::MaxGain) max+=gain;
    check(max*row::AudioMix::SourcePeak<.76,"five simultaneous full-level sources retain over 2 dB digital headroom");
    check(model.distance==0 && model.elapsed==0,"sound updates never advance metrics");
    std::cout<<"PASS "<<checks<<" audio checks\n";
}
