#pragma once
#include "RowCore.h"
#include "RowImu.h"
#include "RowPs4.h"
#include <memory>
#include <string>

namespace row {
struct DeviceConfig {
    std::string trackerSerial;
    std::string ps4Path; // Explicit local HID selection, never logged.
    uint64_t rowerAddress=0,heartAddress=0,imuAddress=0;
    bool enableVr=true;
    int imuAddressType=-1; // 0 public, 1 random, -1 requires advertisement.
    // UE shares SteamVR with OpenXR. Release our runtime only after HMD teardown.
    bool deferVrShutdown=false;
};
struct DeviceSnapshot {
    Pose bar,head;
    ImuSample imu;
    Ps4Controls ps4;
    Telemetry telemetry;
    Field heart;
    bool vrReady=false,rowerConnected=false,heartConnected=false;
    unsigned rowerErrors=0,heartErrors=0;
    unsigned heartPackets=0;
    int32_t rowerLastError=0,heartLastError=0;
    int rowerStage=0,heartStage=0;
    bool imuConnected=false;
    unsigned imuErrors=0,imuRejected=0;
    int32_t imuLastError=0;
    int imuStage=0;
    int imuAddressType=-1,imuGattStatus=-1;
};
// OpenVR poses and Windows GATT run independently of the UE game/render thread.
// All callbacks own shared state; teardown revokes handlers before releasing it.
class Devices {
public:
    explicit Devices(DeviceConfig config);
    ~Devices();
    Devices(const Devices&)=delete;
    Devices& operator=(const Devices&)=delete;
    DeviceSnapshot snapshot() const;
    static double seconds();
    // Call after all Devices workers and the host's OpenXR session have ended.
    static void shutdownDeferredVr();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
