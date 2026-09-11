#pragma once
#include "RowCore.h"
#include <memory>
#include <string>

namespace row {
struct DeviceConfig {
    std::string trackerSerial;
    uint64_t rowerAddress=0,heartAddress=0;
    // UE shares SteamVR with OpenXR. Release our runtime only after HMD teardown.
    bool deferVrShutdown=false;
};
struct DeviceSnapshot {
    Pose bar,head;
    Telemetry telemetry;
    Field heart;
    bool vrReady=false,rowerConnected=false,heartConnected=false;
    unsigned rowerErrors=0,heartErrors=0;
    unsigned heartPackets=0;
    int32_t rowerLastError=0,heartLastError=0;
    int rowerStage=0,heartStage=0;
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
