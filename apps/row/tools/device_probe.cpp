#include "RowDevices.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
// The launcher writes a private settings file. No device IDs enter CLI/logs.
int main(int argc,char** argv) {
    if(argc<2) return 2;
    std::ifstream f(argv[1]); row::DeviceConfig c; std::string line;
    const bool ps4Only=argc>3 && std::string(argv[3])=="--ps4-only";
    if(ps4Only) { std::getline(f,c.ps4Path);c.enableVr=false; }
    else {
    std::getline(f,c.trackerSerial); std::getline(f,line); if(!line.empty()) c.rowerAddress=std::stoull(line,nullptr,16);
    std::getline(f,line); if(!line.empty()) c.heartAddress=std::stoull(line,nullptr,16);
    line.clear(); std::getline(f,line); if(!line.empty()) c.imuAddress=std::stoull(line,nullptr,16);
    line.clear(); std::getline(f,line); if(line=="random") c.imuAddressType=1; else if(line=="public") c.imuAddressType=0;
    }
    if(!f && !f.eof()) return 2;
    const bool imuOnly=argc>3 && std::string(argv[3])=="--imu-only";
    if(imuOnly) { c.enableVr=false; c.rowerAddress=c.heartAddress=0; }
    row::Devices d(c);
    unsigned valid=0; const auto start=row::Devices::seconds();
    const double duration=argc>2?std::stod(argv[2]):20;
    double nextReport=0;
    std::cout<<"Native C++ device test listening\n"<<std::flush;
    while(row::Devices::seconds()-start<duration) {
        auto s=d.snapshot(); valid+=s.bar.valid?1:0;
        if(row::Devices::seconds()-start>=nextReport) {
            std::cout<<"rower_packets="<<s.telemetry.packets<<" heart_packets="<<s.heartPackets<<" tracker="<<s.bar.valid
                <<" imu_packets="<<s.imu.sequence<<" imu_connected="<<s.imuConnected<<'\n'<<std::flush;
            nextReport+=10;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto s=d.snapshot(); const auto now=row::Devices::seconds();
    std::cout<<"native_tracker_samples="<<valid<<" rower_packets="<<s.telemetry.packets<<" rejected="<<s.telemetry.rejected
        <<" heart_fresh="<<s.heart.fresh(now)<<" rower_errors="<<s.rowerErrors<<" heart_errors="<<s.heartErrors
        <<" rower_stage="<<s.rowerStage<<" heart_stage="<<s.heartStage<<" HRESULT="<<std::hex<<s.rowerLastError<<","<<s.heartLastError<<'\n';
    std::cout<<std::dec<<"imu_packets="<<s.imu.sequence<<" imu_fresh="<<s.imu.fresh(now)<<" imu_rejected="<<s.imuRejected
        <<" imu_errors="<<s.imuErrors<<" imu_stage="<<s.imuStage<<" address_type="<<s.imuAddressType
        <<" gatt_status="<<s.imuGattStatus<<" imu_hresult="<<std::hex<<s.imuLastError<<'\n';
    return (ps4Only?s.imu.fresh(now) && s.ps4.fresh(now) && s.imu.sequence>10:imuOnly?s.imu.fresh(now) && s.imu.sequence>10:valid && s.telemetry.packets)?0:1;
}
