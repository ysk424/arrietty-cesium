#include "RowKelvin.h"
#include "RowBowWhitewater.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {
int checks=0;
void check(bool condition,const char* name) {
    ++checks;
    if(!condition) { std::cerr<<"FAIL "<<name<<'\n'; std::exit(1); }
}
double energy(const row::KelvinWake& w) {
    double e=0; for(float h:w.height) e+=double(h)*h; return e;
}
void dump(const row::KelvinWake& w,const std::filesystem::path& dir,const std::string& name,double boatX,double boatY,double heading=0) {
    if(dir.empty()) return;
    std::filesystem::create_directories(dir);
    std::ofstream out(dir/(name+".f32"),std::ios::binary);
    out.write(reinterpret_cast<const char*>(w.height.data()),std::streamsize(w.height.size()*sizeof(float)));
    check(bool(out),"synthetic height field exported");
    std::ofstream meta(dir/(name+".json"));
    meta<<"{\"origin\":["<<w.originX<<','<<w.originY<<"],\"boat\":["<<boatX<<','<<boatY
        <<"],\"heading\":"<<heading<<",\"cell\":0.25,\"size\":256}\n";
    check(bool(meta),"synthetic field coordinates exported");
}
}
int main(int argc,char** argv) {
    const std::filesystem::path output=argc>1?argv[1]:"";
    using Wake=row::KelvinWake;
    auto foamMass=[](const row::Waves& water) {
        double total=0; for(float f:water.bowFoam) total+=f; return total;
    };
    row::Waves stopped;
    for(float speed:{0.f,-1.f,.3f,std::numeric_limits<float>::quiet_NaN()})
        row::BowWhitewater(speed).emit(stopped,0,0,0,1.f);
    check(foamMass(stopped)==0,"stopped/invalid/very slow boat emits no bow whitewater");
    double previousMass=0;
    for(float speed:{1.f,2.f,3.f}) {
        row::Waves water;
        const row::BowWhitewater bow(speed);
        for(int i=0;i<60;++i) bow.emit(water,0,0,0,1.f/60);
        const double mass=foamMass(water);
        check(mass>previousMass,"bow foam increases with boat speed"); previousMass=mass;
        check(water.bowFoam[128*256+138]>.1f,"whitewater reaches ahead of the 2.3 m hull tip");
        double stern=0,asymmetry=0;
        for(int y=1;y<256;++y) for(int x=0;x<256;++x) {
            if(x<128) stern+=water.bowFoam[y*256+x];
            asymmetry=std::max(asymmetry,double(std::abs(water.bowFoam[y*256+x]-water.bowFoam[(256-y)*256+x])));
        }
        check(stern==0 && asymmetry<1e-6,"bow emitter is bilateral and never emits at the stern");
    }
    row::Waves sixty,ninety,turned;
    const row::BowWhitewater cruise(2);
    for(int i=0;i<60;++i) cruise.emit(sixty,0,0,0,1.f/60);
    for(int i=0;i<90;++i) cruise.emit(ninety,0,0,0,1.f/90);
    check(std::abs(foamMass(sixty)-foamMass(ninety))/foamMass(sixty)<1e-5,"foam production is independent of render frame rate");
    cruise.emit(turned,0,0,Wake::Pi/2,1);
    check(turned.bowFoam[138*256+128]>.1f && turned.bowFoam[128*256+138]==0,"bow emission follows boat heading, not a fixed world axis");
    const float deposited=sixty.bowFoam[128*256+138];
    sixty.center(.25,0);
    check(sixty.bowFoam[128*256+137]==deposited,"existing bow foam stays at its deposited world position");
    const double beforeDecay=foamMass(sixty);
    for(int i=0;i<600;++i) { row::BowWhitewater(0).emit(sixty,0,0,0,row::Waves::Step); sixty.tick(); }
    check(foamMass(sixty)<beforeDecay*.01,"remaining bow foam fades after stopping");
    check(row::BowWhitewater(0).crest(2.4f,0,0)==0 && cruise.crest(2.4f,0,0)>0 && cruise.crest(2.4f,0,0)<=.022f,
        "small bow crest uses speed and never lifts the stopped surface");
    check(std::all_of(turned.height.begin(),turned.height.end(),[](float h){return h==0;}),"whitewater emission does not inject extra nondispersive hull waves");
    check(std::all_of(turned.foam.begin(),turned.foam.end(),[](float f){return f==0;}),"bow foam does not change the oar bubble layer");
    turned.center(1000,1000);
    check(foamMass(turned)==0,"teleport clears bow foam as well as the wave field");
    // Independent Fourier-mode oracle: two different wavelengths must oscillate
    // at sqrt(g*k), not the old constant-speed omega=c*k. Test actual solver.
    for(int mode:{4,16}) {
        Wake wave;
        const double k=2*Wake::Pi*mode/(Wake::N*Wake::Cell);
        for(int y=0;y<Wake::N;++y) for(int x=0;x<Wake::N;++x)
            wave.height[y*Wake::N+x]=.01f*float(std::cos(k*x*Wake::Cell));
        wave.tick(0,0,0,0);
        const double expected=.01*std::cos(std::sqrt(9.81*k)*Wake::Step)*std::exp(-.055*Wake::Step);
        check(std::abs(wave.height[128*Wake::N+128]-expected)<2e-7,"deep-water dispersion for independent wavelengths");
    }
    Wake idle;
    for(int i=0;i<30;++i) idle.tick(0,0,0,0);
    check(energy(idle)==0,"stationary/calibration boat generates no hull wake");
    idle.tick(0,0,0,std::numeric_limits<float>::quiet_NaN());
    check(energy(idle)==0,"invalid speed cannot contaminate water");

    const auto began=std::chrono::steady_clock::now();
    int ticks=0;
    for(float speed:{2.f,3.f}) {
        Wake wake;
        double x=0;
        for(int i=0;i<1200;++i) {
            x=speed*(i+.5)*Wake::Step;
            wake.center(x,0); wake.tick(x,0,0,speed); ++ticks;
        }
        double peak=0,mirror=0,sum=0;
        for(int y=48;y<208;++y) for(int i=48;i<208;++i) {
            const float h=wake.height[y*Wake::N+i];
            peak=std::max(peak,double(std::abs(h))); sum+=h;
            mirror=std::max(mirror,double(std::abs(h-wake.height[(256-y)*Wake::N+i])));
        }
        std::cout<<"WATER speed_mps="<<speed<<" peak_m="<<peak<<" mirror_error_m="<<mirror<<" mean_m="<<sum/(160*160)<<'\n';
        check(peak>.003 && peak<.15,"moving hull produces bounded centimetre gravity waves");
        check(mirror<.0005,"straight wake is symmetric about the travelled line");
        check(std::abs(sum/(160*160))<.001,"no sustained lift of reference water level");
        check(std::all_of(wake.height.begin(),wake.height.end(),[](float h){return std::isfinite(h)&&std::abs(h)<.15f;}),"long translating field remains finite and bounded");
        dump(wake,output,speed==2?"straight-2mps":"straight-3mps",x,0);
        // Sample translation at the same world position (no time advancement).
        const float before=wake.height[128*Wake::N+100];
        wake.center(x+Wake::Cell,0);
        check(wake.height[128*Wake::N+99]==before,"domain shift preserves wake world position exactly");
        const double movingEnergy=energy(wake);
        for(int i=0;i<900;++i) { wake.tick(x,0,0,0); ++ticks; }
        std::cout<<"WATER stopped_energy_ratio="<<energy(wake)/movingEnergy<<'\n';
        check(energy(wake)<movingEnergy*.1,"stopping removes pressure and old waves disperse/decay");
        wake.center(x+1000,1000);
        check(energy(wake)==0,"teleport clears the old wake");
        wake.tick(x+1000,1000,0,0);
        check(energy(wake)==0,"teleport also clears hidden surface potential");
    }
    Wake curve;
    double x=0,y=0;
    for(int i=0;i<1200;++i) {
        const double heading=i<600?0:(i-600)*Wake::Step*.065;
        x+=2.5*std::cos(heading)*Wake::Step; y+=2.5*std::sin(heading)*Wake::Step;
        curve.center(x,y); curve.tick(x,y,heading,2.5f); ++ticks;
    }
    dump(curve,output,"turn",x,y,599*Wake::Step*.065);
    check(std::all_of(curve.height.begin(),curve.height.end(),[](float h){return std::isfinite(h)&&std::abs(h)<.15f;}),"turning keeps a bounded world-space history");
    const auto old=curve.height;
    curve.center(x,y);
    check(curve.height==old,"centering alone never rotates or replaces an old wake");
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()/ticks;
    std::cout<<"WATER mean_step_ms="<<ms<<" ticks="<<ticks<<" (includes recentering and checks; no rendering)\n";
    std::cout<<"PASS "<<checks<<" water checks\n";
}
