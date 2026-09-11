#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "RowDevices.h"
#include <openvr.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace row {
using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
using namespace std::chrono_literals;
double Devices::seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Shared { mutable std::mutex mutex; DeviceSnapshot data; std::atomic<bool> stop=false; };
static guid uuid(uint32_t shortId) { return {shortId,0,0x1000,{0x80,0,0,0x80,0x5f,0x9b,0x34,0xfb}}; }
// WinRT calls have bounded waits and cooperative cancellation, including connect.
template<class Async> auto waitFor(Async op,const std::shared_ptr<Shared>& s) {
    const double deadline=Devices::seconds()+12;
    while(op.Status()==Windows::Foundation::AsyncStatus::Started) {
        if(s->stop || Devices::seconds()>deadline) { op.Cancel(); throw hresult_canceled(); }
        std::this_thread::sleep_for(20ms);
    }
    return op.GetResults();
}
static void bleLoop(std::shared_ptr<Shared> s,uint64_t address,bool heart) {
    if(!address) return; // Selection is explicit in ignored local settings.
    init_apartment(apartment_type::multi_threaded);
    while(!s->stop) {
        BluetoothLEDevice device{nullptr}; GattDeviceService service{nullptr};
        GattSession session{nullptr};
        GattCharacteristic characteristic{nullptr}; event_token token{}; bool registered=false;
        try {
            auto stage=[&](int v) { std::lock_guard lock(s->mutex); (heart?s->data.heartStage:s->data.rowerStage)=v; };
            stage(1);
            // A random BLE address needs the advertised address type; assuming
            // Public can yield an unreachable GATT device on Windows.
            using namespace Windows::Devices::Bluetooth::Advertisement;
            BluetoothLEAdvertisementWatcher watcher;
            watcher.ScanningMode(BluetoothLEScanningMode::Active);
            struct AdvertisementState { std::atomic<int> type{-1}; };
            auto advertised=std::make_shared<AdvertisementState>();
            auto advertisementToken=watcher.Received([advertised,address](const auto&,const BluetoothLEAdvertisementReceivedEventArgs& e) {
                if(e.BluetoothAddress()==address) advertised->type=int(e.BluetoothAddressType());
            });
            watcher.Start(); const double scanUntil=Devices::seconds()+5;
            while(!s->stop && advertised->type<0 && Devices::seconds()<scanUntil) std::this_thread::sleep_for(50ms);
            watcher.Stop(); watcher.Received(advertisementToken);
            const auto addressType=advertised->type<0?BluetoothAddressType::Public:BluetoothAddressType(advertised->type.load());
            device=waitFor(BluetoothLEDevice::FromBluetoothAddressAsync(address,addressType),s);
            if(!device) throw hresult_error(hresult{int32_t(0x80004005u)});
            session=waitFor(GattSession::FromDeviceIdAsync(device.BluetoothDeviceId()),s);
            if(session && session.CanMaintainConnection()) session.MaintainConnection(true);
            stage(2);
            auto services=waitFor(device.GetGattServicesForUuidAsync(uuid(heart?0x180d:0x1826),BluetoothCacheMode::Uncached),s);
            if(services.Status()!=GattCommunicationStatus::Success || services.Services().Size()==0) throw hresult_error(hresult{int32_t(0x80004005u)});
            service=services.Services().GetAt(0);
            stage(3);
            auto chars=waitFor(service.GetCharacteristicsForUuidAsync(uuid(heart?0x2a37:0x2ad1),BluetoothCacheMode::Uncached),s);
            if(chars.Status()!=GattCommunicationStatus::Success || chars.Characteristics().Size()==0) throw hresult_error(hresult{int32_t(0x80004005u)});
            characteristic=chars.Characteristics().GetAt(0);
            token=characteristic.ValueChanged([s,heart](const auto&,const GattValueChangedEventArgs& args) {
                try {
                    auto reader=DataReader::FromBuffer(args.CharacteristicValue());
                    std::vector<uint8_t> bytes(reader.UnconsumedBufferLength()); reader.ReadBytes(bytes);
                    const double now=Devices::seconds(); std::lock_guard lock(s->mutex);
                    if(heart) {
                        ++s->data.heartPackets;
                        auto bpm=parseHeart(bytes.data(),bytes.size());
                        s->data.heart.set(bpm?double(*bpm):-1,now);
                    } else parseRower(bytes.data(),bytes.size(),now,s->data.telemetry);
                } catch(...) { /* malformed payload is isolated from render thread */ }
            }); registered=true;
            stage(4);
            const auto status=waitFor(characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::Notify),s);
            if(status!=GattCommunicationStatus::Success) throw hresult_error(hresult{int32_t(0x80004005u)});
            { std::lock_guard lock(s->mutex); (heart?s->data.heartConnected:s->data.rowerConnected)=true; (heart?s->data.heartStage:s->data.rowerStage)=5; }
            const double connectedAt=Devices::seconds();
            while(!s->stop) {
                // Recover from silent notification loss as well as disconnects.
                double latest; { std::lock_guard lock(s->mutex); latest=heart?s->data.heart.received:
                    std::max(s->data.telemetry.strokeRate.received,s->data.telemetry.power.received); }
                if(Devices::seconds()-std::max(latest,connectedAt)>15) break;
                if(Devices::seconds()-connectedAt>5 && device.ConnectionStatus()==BluetoothConnectionStatus::Disconnected) break;
                std::this_thread::sleep_for(100ms);
            }
        } catch(const hresult_error& e) { std::lock_guard lock(s->mutex); ++(heart?s->data.heartErrors:s->data.rowerErrors);
            (heart?s->data.heartLastError:s->data.rowerLastError)=e.code().value; }
        catch(...) { std::lock_guard lock(s->mutex); ++(heart?s->data.heartErrors:s->data.rowerErrors); }
        if(registered) try { characteristic.ValueChanged(token); } catch(...) {}
        // Closing the GATT service releases this client's subscription.
        if(service) try { service.Close(); } catch(...) {}
        if(session) try { session.MaintainConnection(false); session.Close(); } catch(...) {}
        if(device) try { device.Close(); } catch(...) {}
        { std::lock_guard lock(s->mutex); (heart?s->data.heartConnected:s->data.rowerConnected)=false; }
        for(int i=0;i<30 && !s->stop;++i) std::this_thread::sleep_for(100ms);
    }
    uninit_apartment();
}
static std::atomic<bool> deferredVrInitialized=false;
void Devices::shutdownDeferredVr() {
    if(deferredVrInitialized.exchange(false)) vr::VR_Shutdown();
}
static void vrLoop(std::shared_ptr<Shared> s,std::string serial,bool deferShutdown) {
    while(!s->stop) {
        vr::EVRInitError error=vr::VRInitError_None;
        auto system=deferShutdown && deferredVrInitialized?vr::VRSystem():vr::VR_Init(&error,vr::VRApplication_Background);
        if(error!=vr::VRInitError_None || !system) {
            for(int i=0;i<30 && !s->stop;++i) std::this_thread::sleep_for(100ms);
            continue;
        }
        if(deferShutdown) deferredVrInitialized=true;
        int tracker=-1; double nextSearch=0; bool quit=false;
        while(!s->stop && !quit) {
            const double now=Devices::seconds();
            vr::VREvent_t event{};
            while(system->PollNextEvent(&event,sizeof(event))) if(event.eventType==vr::VREvent_Quit) quit=true;
            vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
            system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding,0,poses,vr::k_unMaxTrackedDeviceCount);
            if(now>=nextSearch) {
                tracker=-1; nextSearch=now+1;
                for(unsigned i=0;i<vr::k_unMaxTrackedDeviceCount;++i) {
                    if(!poses[i].bDeviceIsConnected) continue;
                    char name[256]{}; system->GetStringTrackedDeviceProperty(i,vr::Prop_SerialNumber_String,name,sizeof(name));
                    if(!serial.empty() && serial==name) { tracker=int(i); break; }
                }
            }
            auto convert=[&](int index)->Pose {
                Pose result; result.received=now;
                if(index<0) return result;
                auto& p=poses[index]; result.valid=p.bDeviceIsConnected && p.bPoseIsValid
                    && p.eTrackingResult==vr::TrackingResult_Running_OK;
                if(result.valid) {
                    auto& m=p.mDeviceToAbsoluteTracking;
                    // OpenVR RH Y-up -> UE tracking LH Z-up, meters (forward,right,up).
                    result.position={-m.m[2][3],m.m[0][3],m.m[1][3]};
                    result.forward={m.m[2][2],-m.m[0][2],-m.m[1][2]};
                }
                return result;
            };
            { std::lock_guard lock(s->mutex); s->data.vrReady=true;
                s->data.bar=convert(tracker); s->data.head=convert(vr::k_unTrackedDeviceIndex_Hmd); }
            std::this_thread::sleep_for(5ms);
        }
        if(!deferShutdown) vr::VR_Shutdown();
        { std::lock_guard lock(s->mutex); s->data.vrReady=false; s->data.bar.valid=s->data.head.valid=false; }
        // A runtime quit invalidates tracking. Do not tear down the shared
        // SteamVR client beneath a live OpenXR HMD; the host must restart it.
        if(deferShutdown) break;
    }
}
struct Devices::Impl {
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();
    std::thread vr,ble,hr;
    explicit Impl(DeviceConfig c):vr(vrLoop,shared,c.trackerSerial,c.deferVrShutdown),ble(bleLoop,shared,c.rowerAddress,false),hr(bleLoop,shared,c.heartAddress,true) {}
    ~Impl() { shared->stop=true; vr.join(); ble.join(); hr.join(); }
};
Devices::Devices(DeviceConfig c):impl(std::make_unique<Impl>(c)) {}
Devices::~Devices()=default;
DeviceSnapshot Devices::snapshot() const { std::lock_guard lock(impl->shared->mutex); return impl->shared->data; }
}
