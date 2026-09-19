#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RowDevices.h"
#include "RowCalibration.h"
#include "RowPawn.generated.h"
class UCameraComponent;
class UWidgetComponent;
class URowPanel;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class ARowWater;
class FRowKeyInput;
class URowAudioComponent;
class ARowGeography;
UCLASS()
class ARRIETTYROW_API ARowPawn:public APawn {
    GENERATED_BODY()
public:
    ARowPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
private:
    friend class FRowKeyInput;
    friend class FRowSetupControlsTest;
    friend class FRowMovementTest;
    friend class FRowPs4ControlsTest;
    void HandlePs4Controls(double Now,bool Ready);
    void Toggle();
    void ShowSetupPanel();
    void FinishCalibration(const row::Input& Input);
    void Stop();
    void BuildBoat();
    void Record(const TCHAR* Event);
    row::Input ReadInput() const;
    UPROPERTY() TObjectPtr<USceneComponent> Tracking;
    UPROPERTY() TObjectPtr<USceneComponent> BoatRoot;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UWidgetComponent> Instruments;
    UPROPERTY() TObjectPtr<UWidgetComponent> Attribution;
    bool AttributionAttached=false;
    UPROPERTY() TObjectPtr<URowPanel> Panel;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Hull;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Oars;
    UPROPERTY() TObjectPtr<ARowWater> Water;
    UPROPERTY() TObjectPtr<ARowGeography> Geography;
    UPROPERTY() TObjectPtr<URowAudioComponent> RowAudio;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    std::unique_ptr<row::Devices> Devices;
    row::DeviceSnapshot Snapshot;
    row::Model Model;
    row::Calibration Calibration;
    FTransform Home;
    FString SessionFile,Notice;
    bool Offline=false,Demo=false,DemoStarted=false;
    bool UseImu=false;
    bool UsePs4=false;
    uint64 Ps4Starts=0,Ps4Stops=0;
    bool Chase=false;
    double Began=0,NextRecord=0,SimTime=0,QuitAfter=0;
    double CalibrationMotionTime=0;
    double OfflineBarRest=.38;
    double MovementMagnification=1;
    float OarBlend=0;
    double ScreenshotAt=0;
    FString ScreenshotPath;
    uint64 ValidBarFrames=0;
    TSharedPtr<FRowKeyInput> KeyInput;
    TArray<bool> PendingCommands;
};
