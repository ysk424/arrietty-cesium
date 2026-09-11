#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "RowModule.h"
#include "RowDevices.h"
static void* OpenVRHandle=nullptr;
bool RowDeviceApiAvailable() { return OpenVRHandle!=nullptr; }
class FRowModule:public FDefaultGameModuleImpl {
public:
    virtual void StartupModule() override {
        const FString candidates[]{FString(FPlatformProcess::BaseDir())/TEXT("openvr_api.dll"),
            FPaths::ProjectDir()/TEXT("Binaries/Win64/openvr_api.dll"),
            FPaths::ProjectDir()/TEXT("../../ThirdParty/OpenVR/bin/win64/openvr_api.dll")};
        for(const auto& path:candidates) if(FPaths::FileExists(path)) {
            OpenVRHandle=FPlatformProcess::GetDllHandle(*FPaths::ConvertRelativePathToFull(path));
            if(OpenVRHandle) break;
        }
        UE_LOG(LogTemp,Display,TEXT("ROW_OPENVR_LIBRARY available=%d"),OpenVRHandle!=nullptr);
    }
    virtual void ShutdownModule() override {
        // Engine PreExit destroys OpenXR before module shutdown. Pawn EndPlay
        // has already joined pose/BLE workers; it must not destroy SteamVR first.
        if(OpenVRHandle) {
            row::Devices::shutdownDeferredVr();
            UE_LOG(LogTemp,Display,TEXT("ROW_OPENVR_SHUTDOWN after_hmd=1"));
            FPlatformProcess::FreeDllHandle(OpenVRHandle);
        }
        OpenVRHandle=nullptr;
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FRowModule, ArriettyRow, "ArriettyRow");
