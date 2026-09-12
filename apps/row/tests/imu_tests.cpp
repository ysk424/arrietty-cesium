#include "RowCalibration.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <fstream>
#include <sstream>
static int checks=0;
static void check(bool ok,const char* name) { ++checks; if(!ok) { std::cerr<<"FAIL "<<name<<'\n'; std::exit(1); } }
static row::ImuSample sensor(double t,double acceleration=0) {
    row::ImuSample sample;
    sample.received=t; sample.sequence=uint64_t(std::round(t*10))+1; sample.valid=true;
    // Near-vertical mounting, with fore/aft on sensor X. Test compensation
    // against physical acceleration, not the transform's own implementation.
    constexpr double tilt=65*row::Pi/180.;
    sample.acceleration={acceleration,9.80665*std::sin(tilt),9.80665*std::cos(tilt)};
    sample.angles={65,0,0}; return sample;
}
static row::Input input(double t,const row::ImuSample& sample) {
    row::Input in; in.now=t; in.useImu=true; in.imu=sample;
    in.head={{0,0,1},{1,0,0},true,t}; in.telemetry.power.set(90,t); return in;
}
int main(int argc,char** argv) {
    if(argc>1) {
        // Optional private replay: t, acceleration SI xyz, gyro xyz, angles xyz.
        // Public tests below are synthetic; real exercise samples stay in logs/.
        std::ifstream file(argv[1]); std::string line; std::vector<row::ImuSample> samples;
        row::ImuVector gravity; unsigned quietSamples=0;
        while(std::getline(file,line)) {
            std::replace(line.begin(),line.end(),',',' '); std::istringstream values(line);
            row::ImuSample s;
            if(!(values>>s.received>>s.acceleration.x>>s.acceleration.y>>s.acceleration.z
                >>s.angularVelocity.x>>s.angularVelocity.y>>s.angularVelocity.z>>s.angles.x>>s.angles.y>>s.angles.z)) continue;
            if(s.received<1) continue; // Connection warmup is not a physical impulse.
            s.valid=true; s.sequence=samples.size()+1; samples.push_back(s);
            if(s.received<4) { gravity=gravity+row::imuEarthAcceleration(s); ++quietSamples; }
        }
        check(samples.size()>30 && quietSamples>=5,"replay has quiet baseline");
        unsigned cycles=0; const auto learned=row::fitImu(samples,gravity*(1./quietSamples),cycles);
        check(learned.valid,"recorded motion supplies a consistent calibration line");
        row::Model model; const auto first=samples.front();
        row::SteeringFrame frame{{1,0,0},{0,0,1},true,true,learned};
        check(model.start(input(first.received,first),frame),"private replay starts");
        size_t at=0; unsigned previousCount=0;
        for(double t=first.received+.01;t<samples.back().received;t+=.01) {
            while(at+1<samples.size() && samples[at+1].received<=t) ++at;
            model.tick(input(t,samples[at]),.01);
            if(model.strokes!=previousCount) { std::cout<<"pull_at_s="<<t<<'\n'; previousCount=model.strokes; }
        }
        std::cout<<"replay_pulls="<<model.strokes<<" final_drive="<<model.drive<<" state="<<int(model.state)<<'\n';
        check(model.state==row::State::Running && model.drive<.001,"replay stays fresh and ends quiet");
        if(argc>2) check(model.strokes==unsigned(std::stoul(argv[2])),"replay agrees with reported count");
        if(argc>3) {
            row::Calibration setup; row::Model online; at=0; bool began=false,started=false;
            for(double t=first.received+.01;t<samples.back().received;t+=.01) {
                while(at+1<samples.size() && samples[at+1].received<=t) ++at;
                const auto current=input(t,samples[at]);
                if(!began && t>=std::stod(argv[3])) { began=setup.begin(current); }
                if(!began) continue;
                if(!started) {
                    setup.tick(current,.01);
                    if(setup.phase==row::CalibrationPhase::Complete) {
                        started=online.start(current,setup.frame); std::cout<<"online_calibration_complete_s="<<t<<'\n';
                    }
                } else online.tick(current,.01);
            }
            std::cout<<"following_pulls="<<online.strokes<<" state="<<int(online.state)<<'\n';
            check(started && online.state==row::State::Running,"recorded setup completes and keeps running");
            if(argc>4) check(online.strokes==unsigned(std::stoul(argv[4])),"remaining recorded strokes after setup");
        }
        return 0;
    }
    std::array<uint8_t,20> bytes{0x55,0x61};
    auto put=[&](size_t at,int16_t value) { bytes[at]=uint8_t(value&255); bytes[at+1]=uint8_t(uint16_t(value)>>8); };
    put(2,-2048); put(4,1024); put(6,2048); put(8,-16384); put(10,8192); put(12,16384);
    put(14,-16384); put(16,8192); put(18,16384);
    row::ImuSample sample;
    check(row::parseImu(bytes.data(),bytes.size(),10,sample),"combined notification accepted");
    check(std::abs(sample.acceleration.x+9.80665)<1e-8 && std::abs(sample.acceleration.y-4.903325)<1e-8,
          "signed little-endian acceleration converted to SI");
    check(sample.angularVelocity.x==-1000 && sample.angularVelocity.y==500 && sample.angularVelocity.z==1000,"gyro scale");
    check(sample.angles.x==-90 && sample.angles.y==45 && sample.angles.z==90,"angle scale");
    check(sample.fresh(10.1) && !sample.fresh(10.25) && !sample.fresh(9.9),"sensor freshness gates");
    for(size_t size=0;size<20;++size) check(!row::parseImu(bytes.data(),size,11,sample),"truncation rejected");
    bytes[1]=0x71;
    check(!row::parseImu(bytes.data(),bytes.size(),11,sample) && sample.received==10 && sample.sequence==1,"register reply cannot revive motion");
    bytes[1]=0x61;
    check(!row::parseImu(bytes.data(),bytes.size(),std::numeric_limits<double>::quiet_NaN(),sample),"nonfinite time rejected");
    sample.acceleration={0,0,9.80665}; sample.angles={0,0,0};
    check(std::abs(row::imuEarthAcceleration(sample).z-9.80665)<1e-8,"flat stationary gravity");
    sample.acceleration={-9.80665,0,0}; sample.angles={0,90,35};
    const auto upright=row::imuEarthAcceleration(sample);
    check(std::hypot(upright.x,upright.y)<1e-8 && std::abs(upright.z-9.80665)<1e-8,"vertical mounting gravity rotation");
    sample.acceleration.x=std::numeric_limits<double>::quiet_NaN();
    check(!sample.fresh(10.1),"nonfinite acceleration rejected");
    row::ImuFit fit{{0,0,9.80665},{1,0,0},true};
    row::SteeringFrame frame{{1,0,0},{0,0,1},true,true,fit};
    auto in=input(0,sensor(0)); row::Model model;
    check(!in.bar.valid && model.start(in,frame),"IMU starts without Tracker when calibrated");
    for(int i=1;i<=1000;++i) { const double t=i*.01; in=input(t,sensor((i-i%10)*.01)); model.tick(in,.01); }
    check(model.state==row::State::Running && model.distance==0 && model.strokes==0 && model.drive==0,"stationary tilted IMU plus positive BT watts stays still");
    check(model.barTracking.source==row::BarSource::Imu,"estimated IMU velocity is explicitly labelled");
    row::ImuMotion motion; motion.start(fit,sensor(0));
    const auto impulse=sensor(.1,-2); motion.tick(impulse,.1); const double velocity=motion.velocity;
    for(int i=1;i<14;++i) motion.tick(impulse,.1+i*.01);
    check(motion.velocity==velocity,"render polling cannot integrate one notification twice");
    check(!motion.tick(impulse,.36),"notification dropout rejected");
    check(!motion.tick(sensor(.05),.1),"out-of-order sample rejected");
    std::vector<row::ImuSample> training;
    for(int i=0;i<120;++i) {
        const double t=i*.1;
        training.push_back(sensor(t,t<1?0:-.4*std::pow(2*row::Pi/3.,2)*std::cos((t-1)*2*row::Pi/3.)));
    }
    unsigned cycles=0; const auto learned=row::fitImu(training,{0,0,9.80665},cycles);
    check(learned.valid && cycles==2 && learned.axis.x>.99,"two measured cycles learn axis and first-pull sign");
    row::Calibration calibration; in=input(0,sensor(0)); calibration.begin(in);
    double axisBegan=-1;
    for(int i=1;i<2000 && calibration.phase!=row::CalibrationPhase::Complete;++i) {
        const double t=i*.01,received=(i-i%10)*.01;
        if(calibration.phase==row::CalibrationPhase::Axis && axisBegan<0) axisBegan=received;
        const double acc=axisBegan>=0?-.4*std::pow(2*row::Pi/3.,2)*std::cos((received-axisBegan)*2*row::Pi/3.):0;
        in=input(t,sensor(received,acc)); calibration.tick(in,.01);
    }
    check(calibration.phase==row::CalibrationPhase::Complete && calibration.strokes==2,"IMU completes existing settle, quiet second and two-cycle setup");
    check(calibration.frame.useImu && calibration.frame.imu.valid,"IMU calibration is explicit in steering frame");
    row::Model ride; check(ride.start(input(0,sensor(0)),frame),"replay ride starts");
    for(int i=1;i<=3700;++i) {
        const double t=i*.01,received=(i-i%10)*.01;
        const double acc=received>=3 && received<33?-.4*std::pow(2*row::Pi/3.,2)*std::cos((received-3)*2*row::Pi/3.):0;
        in=input(t,sensor(received,acc)); ride.tick(in,.01);
    }
    std::cout<<"synthetic_pulls="<<ride.strokes<<" distance="<<ride.distance<<" state="<<int(ride.state)<<'\n';
    check(ride.strokes==10 && ride.distance>1,"10 Hz physical stroke fixture drives and counts ten pulls at 100 Hz rendering");
    check(ride.drive<1e-6,"last quiet interval removes estimated drive");
    auto missing=input(37.3,in.imu); ride.tick(missing,.01);
    check(ride.state==row::State::TrackingLost && ride.speed==0 && ride.drive==0,"IMU dropout stops motion without HMD fallback");
    ride.tick(input(37.4,sensor(37.4)),.01);
    check(ride.state==row::State::TrackingLost,"reconnection cannot auto-start ride");
    row::Calibration still; still.begin(input(0,sensor(0)));
    for(int i=1;i<=4100;++i) still.tick(input(i*.01,sensor((i-i%10)*.01)),.01);
    check(still.phase==row::CalibrationPhase::Failed && still.issue==row::CalibrationIssue::Timeout,"stationary sensor cannot complete stroke calibration");
    row::Calibration lost; lost.begin(input(0,sensor(0))); auto bad=input(.01,sensor(0)); bad.imu.valid=false; lost.tick(bad,.01);
    check(lost.phase==row::CalibrationPhase::Failed,"IMU loss during setup requires retry");
    auto invalidFrame=frame; invalidFrame.imu.axis.x=std::numeric_limits<double>::quiet_NaN();
    check(!model.start(input(0,sensor(0)),invalidFrame),"nonfinite IMU calibration cannot start");
    row::ImuMotion tilt; tilt.start(fit,sensor(0)); std::vector<row::ImuSample> rotations;
    for(int i=1;i<=60;++i) {
        auto s=sensor(i*.1); const double angle=65+20*std::sin(i*.1);
        s.angles.x=angle; s.angularVelocity.x=20*std::cos(i*.1);
        s.acceleration={0,9.80665*std::sin(angle*row::Pi/180.),9.80665*std::cos(angle*row::Pi/180.)};
        tilt.tick(s,s.received); rotations.push_back(s);
    }
    check(std::abs(tilt.velocity)<1e-8,"pure wrist tilt with gravity does not produce drive");
    check(!row::fitImu(rotations,{0,0,9.80665},cycles).valid,"rotation alone cannot teach a rowing axis");
    row::ImuMotion bias; bias.start(fit,sensor(0));
    for(int i=1;i<=100;++i) bias.tick(sensor(i*.1,-.3),i*.1);
    check(bias.velocity==0,"constant residual acceleration cannot drive indefinitely");
    row::Model steering; steering.start(input(0,sensor(0)),frame);
    for(int i=1;i<=100;++i) {
        in=input(i*.01,sensor((i-i%10)*.01)); in.head.forward={0,1,0}; steering.speed=2; steering.tick(in,.01);
    }
    check(steering.heading==0 && steering.steer==0,"looking sideways cannot turn an IMU ride");
    for(int i=101;i<=200;++i) {
        in=input(i*.01,sensor((i-i%10)*.01)); in.head.position.y=.2; steering.speed=2; steering.tick(in,.01);
    }
    check(steering.steer>.8 && steering.heading>0,"HMD lateral lean still steers");
    steering.pause(); steering.tick(input(2.01,sensor(2)),.01);
    check(steering.state==row::State::Paused && steering.speed==0,"fresh IMU packets cannot undo Enter pause");
    steering.start(input(3,sensor(3)),frame); in=input(3.01,sensor(3)); in.head.valid=false; steering.tick(in,.01);
    check(steering.state==row::State::TrackingLost && steering.speed==0,"IMU ride still requires HMD tracking");
    std::cout<<checks<<" IMU protocol checks passed\n";
}
