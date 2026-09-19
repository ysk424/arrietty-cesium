#include "RowPs4.h"
#include "RowCalibration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
static unsigned checks=0;
static void check(bool ok,const char* label) { ++checks; if(!ok) { std::cerr<<"FAIL "<<label<<'\n'; std::exit(1); } }
static row::Input input(row::ImuSample s,double now) {
    row::Input i; i.now=now;i.useImu=true;i.imu=s;i.head={{0,0,1},{1,0,0},true,now};return i;
}
int main(int argc,char** argv) {
    if(argc>1) {
        std::ifstream file(argv[1]);std::string line; std::getline(file,line);
        row::Ps4Fusion fusion;std::vector<row::ImuSample> samples;row::ImuVector gravity;unsigned quiet=0;
        std::ofstream trace; if(argc>2) trace.open(argv[2]);
        while(std::getline(file,line)) {
            std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
            double t,tick;row::Ps4Packet p;
            if(!(row>>t>>tick>>p.gyro.x>>p.gyro.y>>p.gyro.z>>p.acceleration.x>>p.acceleration.y>>p.acceleration.z)) continue;
            p.gyro=p.gyro*(1./16);p.acceleration=p.acceleration*(9.80665/8192);
            if(!fusion.tick(p,t)) continue;
            samples.push_back(fusion.sample);
            if(t>=1 && t<4) {gravity=gravity+fusion.sample.acceleration;++quiet;}
        }
        check(quiet>100,"private recording quiet samples");gravity=gravity*(1./quiet);
        std::vector<row::ImuSample> strokes;
        for(auto s:samples) if(s.received>=5 && s.received<30) strokes.push_back(s);
        unsigned cycles=0; auto fit=row::fitImu(strokes,gravity,cycles);
        std::cout<<"fit="<<fit.valid<<" cycles="<<cycles<<" gravity="<<gravity.x<<","<<gravity.y<<","<<gravity.z<<" axis="<<fit.axis.x<<","<<fit.axis.y<<'\n';
        row::Model model;row::SteeringFrame frame{{1,0,0},{0,0,1},true,true,fit};
        if(fit.valid) model.start(input(samples.front(),samples.front().received),frame);
        row::Calibration calibration;bool began=false,started=false;row::Model online;
        for(size_t j=1;j<samples.size();++j) {
            const auto s=samples[j];double dt=s.received-samples[j-1].received;
            const auto i=input(s,s.received);
            if(!began && s.received>=.5) began=calibration.begin(i);
            if(began && !started) {
                calibration.tick(i,dt);
                if(calibration.phase==row::CalibrationPhase::Complete) {started=online.start(i,calibration.frame);std::cout<<"online_start="<<s.received<<'\n';}
            } else if(started) online.tick(i,dt);
            auto n=model.strokes;model.tick(i,dt);if(n!=model.strokes)std::cout<<"pull="<<s.received<<'\n';
            if(trace)trace<<s.received<<","<<s.acceleration.x<<","<<s.acceleration.y<<","<<s.acceleration.z<<","<<model.barVelocity<<","<<model.strokes<<'\n';
        }
        std::cout<<"replay_strokes="<<model.strokes<<" state="<<int(model.state)<<" online="<<started<<" phase="<<int(calibration.phase)<<" online_strokes="<<online.strokes<<'\n';return 0;
    }
    row::Ps4Buttons buttons;row::Ps4Packet p;p.square=true;buttons.tick(p,0);
    check(buttons.state.starts==0,"held start on connection ignored");p.square=false;buttons.tick(p,.01);p.square=true;buttons.tick(p,.02);buttons.tick(p,.03);
    check(buttons.state.starts==1,"start once per press");p.triangle=true;buttons.tick(p,.04);
    check(buttons.state.stops==1,"stop button");p={};p.l1=true;buttons.tick(p,.05);check(buttons.state.steer==-.5,"normal left");p.l2=true;buttons.tick(p,.06);check(buttons.state.steer==-1,"strong left priority");p.r1=true;buttons.tick(p,.07);check(buttons.state.steer==0,"opposed buttons cancel");p={};p.r2=true;buttons.tick(p,.08);check(buttons.state.steer==1,"strong right");p={};buttons.tick(p,.09);check(buttons.state.steer==0,"release immediate");
    check(!buttons.state.fresh(.35),"stale input");buttons.disconnect();check(!buttons.state.valid,"disconnect");
    p={};p.triangle=true;buttons.tick(p,.36);check(buttons.state.stops==2,"held triangle stops on reconnect");
    auto report=row::ps4EnableReport(547);check(report.size()==547 && report[3]==0 && report[6]==0,"enable without effects");
    report[0]=0x11;report[1]=0x80;report[23]=0;report[24]=0x20;
    auto crc=row::ps4Crc(0xa1,report.data(),74);for(unsigned j=0;j<4;++j)report[74+j]=uint8_t(crc>>(8*j));
    check(row::parsePs4(report.data(),report.size(),p) && std::abs(p.acceleration.y-9.80665)<1e-8,"CRC packet SI acceleration");
    report[15]^=1;check(!row::parsePs4(report.data(),report.size(),p),"corrupted packet rejected");
    for(size_t j=0;j<78;++j)check(!row::parsePs4(report.data(),j,p),"truncation rejected");
    row::Ps4Fusion fusion;p={};p.acceleration={0,9.80665,0};
    check(fusion.tick(p,0),"tilted initialization");check(std::abs(fusion.sample.acceleration.z-9.80665)<1e-8,"initial gravity up");
    for(int j=1;j<=400;++j) {
        const double t=j*.005,angle=t*.3;
        p.acceleration={0,9.80665*std::cos(angle),-9.80665*std::sin(angle)};p.gyro={.3*180/row::Pi,0,0};fusion.tick(p,t);
    }
    check(std::hypot(fusion.sample.acceleration.x,fusion.sample.acceleration.y)<.01,"rotation removes gravity during gyro motion");
    check(!fusion.tick(p,2),"duplicate time not integrated");
    row::Model model;row::SteeringFrame frame{{1,0,0},{0,0,1},true,true,{{0,0,9.80665},{1,0,0},true}};
    row::ImuSample stationary;stationary.acceleration={0,0,9.80665};stationary.valid=true;stationary.compensated=true;stationary.received=0;stationary.sequence=1;
    auto i=input(stationary,0);i.usePs4=true;i.controls.valid=true;i.controls.received=0;
    check(model.start(i,frame),"PS4 starts with calibrated IMU and HMD");
    for(int j=1;j<=100;++j) {
        i.now=j*.01;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;
        i.head.position.y=.25;i.head.forward={0,1,0};model.speed=2;model.tick(i,.01);
    }
    check(model.heading==0 && model.steer==0,"head lean and gaze never steer PS4 mode");
    i.controls.steer=-.5;i.now+=.01;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;model.tick(i,.01);
    check(model.steer==-.5 && model.yawRate<0,"shoulder button turns left");
    i.controls.steer=0;i.now+=.01;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;model.tick(i,.01);
    check(model.yawRate==0,"button release zeroes yaw without lingering turn");
    const auto distance=model.distance,elapsed=model.elapsed,heading=model.heading;
    i.now+=.3;i.head.received=i.now;model.tick(i,.01);
    check(model.state==row::State::TrackingLost && model.reconnectPending && model.speed==0,"dropout pauses with auto-resume intent");
    i.now+=.01;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;
    model.tick(i,.01);
    check(model.state==row::State::Running && !model.reconnectPending,"fresh reconnect auto-resumes without calibration");
    check(model.distance==distance && model.elapsed==elapsed && model.heading==heading && model.imuMotion.velocity==0,"reconnect preserves ride and discards gap velocity");
    model.pause();i.now+=1;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;model.tick(i,.01);
    check(model.state==row::State::Paused,"manual pause survives reconnect");
    model.start(i,frame);i.now+=.3;i.head.received=i.now;model.tick(i,.01);model.pause();
    i.now+=.01;i.head.received=i.imu.received=i.controls.received=i.now;++i.imu.sequence;model.tick(i,.01);
    check(model.state==row::State::Paused && !model.reconnectPending,"manual pause during dropout cancels auto-resume");
    model.reset();model.tick(i,.01);check(model.state==row::State::Ready,"stop cannot auto-start");
    row::Ps4Fusion physical;row::Calibration setup;row::Model ride;bool begun=false,started=false;
    const double angle=40*row::Pi/180;row::ImuQuaternion mount{std::cos(angle/2),std::sin(angle/2),0,0};
    for(int j=0;j<=8000;++j) {
        const double t=j*.005;row::Ps4Packet q;
        const double acceleration=t>=5 && t<35?-.4*std::pow(2*row::Pi/3.,2)*std::cos((t-5)*2*row::Pi/3.):0;
        q.acceleration=mount.inverse({acceleration,0,9.80665});physical.tick(q,t);
        auto current=input(physical.sample,t);current.usePs4=true;current.controls.valid=true;current.controls.received=t;
        if(!begun && t>=.5)begun=setup.begin(current);
        if(begun && !started) {
            setup.tick(current,.005);if(setup.phase==row::CalibrationPhase::Complete)started=ride.start(current,setup.frame);
        } else if(started) ride.tick(current,.005);
    }
    check(started && ride.state==row::State::Running,"synthetic tilted PS4 completes calibration and rowing");
    std::cout<<"synthetic following strokes="<<ride.strokes<<" drive="<<ride.drive<<'\n';
    check(ride.strokes==8,"synthetic eight cycles after two-stroke calibration");
    check(ride.drive==0,"stationary end stops drive despite filter decay");
    std::cout<<checks<<" PS4 checks passed\n";
}
