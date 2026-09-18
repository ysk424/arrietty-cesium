#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlyAudioMix.h"
#include "ArriettyPawn.generated.h"
class UCameraComponent;
class UWidgetComponent;
class UArriettyPanel;
class FSocket;
class FInternetAddr;

UCLASS()
class ARRIETTYUE_API AArriettyPawn : public APawn
{
    GENERATED_BODY()
public:
    AArriettyPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FString PlaceName=TEXT("ARRIETTY / FLY"), TimezoneLabel=TEXT("Local date / time");
    bool WorldReady=false;
    FString SetupDate, SetupTime, SetupMessage=TEXT("Local time stays fixed during play.");
    bool bSetupDirty=false;
    int32 ApplyId=0;
    void StartSimulation();
    bool CanResumePreparation() const { return !bAutoStartPending && !bPlaying && WorldReady && !bSetupDirty; }
private:
    friend class FArriettyHmdAlignmentTest;
    friend class FArriettyAutomaticStartTest;
    UPROPERTY() TObjectPtr<USceneComponent> Origin;
    UPROPERTY() TObjectPtr<class UFlyAudioComponent> FlightAudio;
    FFlyAudioInput AudioInput;
    bool bAudioFixture=false;
    UPROPERTY() TObjectPtr<USceneComponent> Tracking;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UWidgetComponent> PanelComponent;
    UPROPERTY() TObjectPtr<UArriettyPanel> Panel;
    UPROPERTY() TObjectPtr<class UArriettySetup> Setup;
    UPROPERTY() TObjectPtr<class AArriettyWorld> Geography;
    UPROPERTY() TObjectPtr<UWidgetComponent> Attribution;
    bool AttributionAttached=false, SpawnPlaced=false;
    double SmokeBegan=0;
    double SmokeMaxDistance=0,SmokeMaxAgl=0;
    FSocket* Socket = nullptr;
    TSharedPtr<FInternetAddr> Remote;
    FString Token;
    bool bPlaying=false, bOffline=true, bSmoke=false, bCaptured=false, bSmokeMoved=false, bSmokeAirborne=false;
    bool bAutoStartPending=true;
    int32 Sequence=0, Aligned=0, PendingAlignment=0, RecenterId=0, ReceivedSequence=-1;
    float OfflineSpeed=0;
    double LastPacket=0, BeganAt=0, LastViewLog=0;
    double AlignmentBearing=0;
    FVector PathVelocity=FVector::ZeroVector;
    int32 AppliedId=-1;
    bool bInputWasPlaying=true;
    void Send(bool Quit=false);
    void StopSimulation();
    void UpdateAutomaticStart(double Now,bool PrepareOnly);
    bool CanApplyPose(int32 AppliedAlignment) const { return bOffline || Aligned==0 || AppliedAlignment==Aligned; }
};
