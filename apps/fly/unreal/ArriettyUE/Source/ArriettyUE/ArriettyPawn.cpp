#include "ArriettyPawn.h"
#include "FlyAudioComponent.h"
#include "ArriettyWorld.h"
#include "CesiumCreditSystem.h"
#include "ArriettyPanel.h"
#include "ArriettySetup.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "IXRCamera.h"
#include "Common/UdpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"

AArriettyPawn::AArriettyPawn()
{
    PrimaryActorTick.bCanEverTick=true;
    AutoPossessPlayer=EAutoReceiveInput::Player0;
    Origin=CreateDefaultSubobject<USceneComponent>(TEXT("Vehicle")); SetRootComponent(Origin);
    FlightAudio=CreateDefaultSubobject<UFlyAudioComponent>(TEXT("FlightAudio"));
    Tracking=CreateDefaultSubobject<USceneComponent>(TEXT("XROrigin")); Tracking->SetupAttachment(Origin);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("HMD")); Camera->SetupAttachment(Tracking);
    Camera->bLockToHmd=true; Camera->bUsePawnControlRotation=false; Camera->SetFieldOfView(95);
    PanelComponent=CreateDefaultSubobject<UWidgetComponent>(TEXT("Instruments"));
    PanelComponent->SetupAttachment(Origin);
    PanelComponent->SetWidgetSpace(EWidgetSpace::World);
    PanelComponent->SetWidgetClass(UArriettyPanel::StaticClass());
    PanelComponent->SetDrawSize(FVector2D(1500,540));
    PanelComponent->SetRelativeLocation(FVector(130,0,100));
    PanelComponent->SetRelativeRotation(FRotator(24.78,180,0));
    PanelComponent->SetRelativeScale3D(FVector(.085));
    PanelComponent->SetTwoSided(true);
    PanelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PanelComponent->SetRedrawTime(1.f/30);
    Attribution=CreateDefaultSubobject<UWidgetComponent>(TEXT("CesiumAttribution"));
    Attribution->SetupAttachment(Origin);Attribution->SetWidgetSpace(EWidgetSpace::World);
    Attribution->SetDrawSize(FVector2D(1400,300));Attribution->SetRelativeLocation(FVector(145,0,65));
    Attribution->SetRelativeRotation(FRotator(25,180,0));Attribution->SetRelativeScale3D(FVector(.07));
    Attribution->SetTwoSided(true);Attribution->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AArriettyPawn::BeginPlay()
{
    Super::BeginPlay();
    BeganAt=FPlatformTime::Seconds();
    bAudioFixture=FParse::Param(FCommandLine::Get(),TEXT("FlyAudioFixture")) && FParse::Param(FCommandLine::Get(),TEXT("nohmd"));
    if(bAudioFixture) { Camera->bLockToHmd=false; FlightAudio->Initialize(); return; }
    PanelComponent->InitWidget(); Panel=Cast<UArriettyPanel>(PanelComponent->GetWidget());
    if(auto Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Instruments.M_Instruments")))
        PanelComponent->SetMaterial(0,Material);
    FString Session=FPlatformMisc::GetEnvironmentVariable(TEXT("ARRIETTY_UE_SESSION"));
    FString Data;
    TSharedPtr<FJsonObject> Config;
    if(!FFileHelper::LoadFileToString(Data,*Session) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Config) || !Config.IsValid())
    {
        if(Panel) Panel->Status=TEXT("Launch with fly.ps1");
        UE_LOG(LogTemp,Error,TEXT("ARRIETTY_UE_SESSION_MISSING")); return;
    }
    Token=Config->GetStringField(TEXT("token"));
    bOffline=!Config->GetBoolField(TEXT("hardware"));
    FlightAudio->Initialize();
    bSmoke=FParse::Param(FCommandLine::Get(),TEXT("ArriettySmoke"));
    Remote=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    bool Valid; Remote->SetIp(TEXT("127.0.0.1"),Valid); Remote->SetPort(Config->GetIntegerField(TEXT("port")));
    Socket=FUdpSocketBuilder(TEXT("ArriettyLoopback")).AsNonBlocking().BoundToAddress(FIPv4Address::InternalLoopback).BoundToPort(0).WithReceiveBufferSize(65536);
    if(bOffline) { Camera->bLockToHmd=false; Camera->SetRelativeLocation(FVector(0,0,160)); Camera->SetRelativeRotation(FRotator(-15,0,0)); }
    else UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
    Geography=AArriettyWorld::Get(GetWorld());
    if(Panel) Panel->Status=TEXT("PREPARING | AUTOMATIC START");
    FString SolarData;
    TSharedPtr<FJsonObject> Solar;
    if(FFileHelper::LoadFileToString(SolarData,*FPlatformMisc::GetEnvironmentVariable(TEXT("ARRIETTY_UE_SOLAR"))) &&
       FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SolarData),Solar))
    { const FString Local=Solar->GetStringField(TEXT("local_time")); SetupDate=Local.Left(10); SetupTime=Local.Mid(11,5); }
    if(auto PC=Cast<APlayerController>(GetController()); PC && !bSmoke)
    {
        Setup=CreateWidget<UArriettySetup>(PC); Setup->Pawn=this;
        Setup->AddToViewport(); Setup->SetDesiredSizeInViewport(FVector2D(430,460)); Setup->SetPositionInViewport(FVector2D(30,30));
    }
    UE_LOG(LogTemp,Display,TEXT("ARRIETTY_UE_PAWN_READY offline=%d"),bOffline);
}

void AArriettyPawn::Send(bool Quit)
{
    if(!Socket) return;
    TSharedRef<FJsonObject> P=MakeShared<FJsonObject>();
    if(Geography) P->SetObjectField(TEXT("terrain"),Geography->Packet());
    if(Geography && bPlaying && WorldReady) P->SetBoolField(TEXT("obstacle_ahead"),Geography->PathBlocked(GetActorLocation(),PathVelocity.IsNearlyZero()?GetActorForwardVector():PathVelocity,this,FMath::Max(2.,PathVelocity.Size()*.001)));
    P->SetNumberField(TEXT("protocol"),1); P->SetStringField(TEXT("token"),Token);
    P->SetNumberField(TEXT("seq"),++Sequence); P->SetBoolField(TEXT("play"),bPlaying);
    P->SetBoolField(TEXT("quit"),Quit); P->SetNumberField(TEXT("aligned"),Aligned);
    P->SetNumberField(TEXT("recenter_id"),RecenterId);
    P->SetNumberField(TEXT("alignment_bearing"),AlignmentBearing);
    const bool Tracked=bOffline || (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId));
    P->SetBoolField(TEXT("hmd_valid"),Tracked);
    P->SetNumberField(TEXT("apply_id"),ApplyId);
    P->SetStringField(TEXT("local_date"),SetupDate); P->SetStringField(TEXT("local_time"),SetupTime);
    if(bOffline)
    {
        auto PC=Cast<APlayerController>(GetController());
        int32 Buttons=0;
        if(PC) for(int32 I=0;I<8;++I)
        {
            const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};
            if(PC->IsInputKeyDown(Keys[I])) Buttons|=1<<I;
        }
        const double Age=SmokeBegan>0?FPlatformTime::Seconds()-SmokeBegan:0;
        if(bSmoke) { Buttons=(Age>2 && Age<2.3)?1:((Age>3 && Age<3.3 && Geography && Geography->StartAltitude==0)?2:((Age>4 && Age<4.3)?12:0)); OfflineSpeed=27; }
        P->SetNumberField(TEXT("buttons"),Buttons);
        P->SetNumberField(TEXT("speed"),OfflineSpeed);
        P->SetNumberField(TEXT("power"),OfflineSpeed>0?250:0);
        const double Steer=PC?(PC->IsInputKeyDown(EKeys::Right)?-8:(PC->IsInputKeyDown(EKeys::Left)?8:0)):0;
        P->SetNumberField(TEXT("steer"),Steer);
    }
    FString Json; FJsonSerializer::Serialize(P,TJsonWriterFactory<>::Create(&Json));
    FTCHARToUTF8 Bytes(*Json); int32 Sent; Socket->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length(),Sent,*Remote);
}

void AArriettyPawn::StartSimulation()
{
    if(!bSetupDirty && WorldReady)
    {
        bPlaying=true;
        bAutoStartPending=false;
    }
}

void AArriettyPawn::StopSimulation()
{
    bPlaying=false;
    bAutoStartPending=false; // Esc/watchdog must stay stopped on later ticks.
    Aligned=0;
    PendingAlignment=0;
}

void AArriettyPawn::UpdateAutomaticStart(double Now,bool PrepareOnly)
{
    if(bAutoStartPending && !bSmoke && !PrepareOnly && LastPacket>0 &&
       Now-LastPacket<=1 && AppliedId==ApplyId)
    {
        StartSimulation();
        if(bPlaying) UE_LOG(LogTemp,Display,TEXT("FLY_AUTOMATIC_PREPARATION_READY wait_for_button1=1"));
    }
}

void AArriettyPawn::Tick(float Delta)
{
    Super::Tick(Delta);
    if(bAudioFixture) { FlightAudio->TickFixture(Delta); return; }
    if(!Geography) Geography=AArriettyWorld::Get(GetWorld());
    WorldReady=Geography && Geography->IsReady();
    if(Geography) {
        PlaceName=Geography->PlaceName;TimezoneLabel=Geography->Timezone+TEXT(" / local date and time");
        if(!WorldReady) SetupMessage=Geography->Message;
        if(Geography->HasOrigin() && !SpawnPlaced) {
            const FVector Spawn=Geography->Position(0,0,Geography->StartAltitude);
            SetActorLocationAndRotation(Spawn,Geography->Rotation(FRotator::ZeroRotator,Spawn));
            SpawnPlaced=true;
        }
        if(!bOffline && !AttributionAttached) {
            auto Credits=ACesiumCreditSystem::GetDefaultCreditSystem(this);
            auto Field=Credits?FindFProperty<FObjectProperty>(Credits->GetClass(),TEXT("CreditsWidget")):nullptr;
            auto Widget=Field?Cast<UUserWidget>(Field->GetObjectPropertyValue_InContainer(Credits)):nullptr;
            if(Widget) { Widget->RemoveFromParent();Attribution->SetWidget(Widget);AttributionAttached=true; }
        }
    }
    auto PC=Cast<APlayerController>(GetController());
    if(PC)
    {
        if(PC->WasInputKeyJustPressed(EKeys::Escape)) StopSimulation();
        if(bPlaying && PC->WasInputKeyJustPressed(EKeys::R))
        {
            ++RecenterId; Aligned=0; PendingAlignment=0;
        }
        if(bOffline) OfflineSpeed=FMath::Clamp(OfflineSpeed+(PC->IsInputKeyDown(EKeys::Up)?8.f:0.f)*Delta-(PC->IsInputKeyDown(EKeys::Down)?8.f:0.f)*Delta,0.f,60.f);
    }
    const double Now=FPlatformTime::Seconds();
    const bool PrepareOnly=FParse::Param(FCommandLine::Get(),TEXT("FlyPrepareOnly"));
    UpdateAutomaticStart(Now,PrepareOnly);
    if(Setup)
    {
        Setup->SetVisibility(bPlaying?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
        if(PC)
        {
            PC->bShowMouseCursor=!bPlaying;
            if(bPlaying!=bInputWasPlaying)
            {
                bInputWasPlaying=bPlaying;
                if(bPlaying) PC->SetInputMode(FInputModeGameOnly());
                else PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
            }
        }
    }
    if(PrepareOnly && WorldReady) UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
    if((bSmoke || PrepareOnly) && (Now-BeganAt>210 || (Geography && Geography->HasFailed()))) {
        UE_LOG(LogTemp,Error,TEXT("FLY_SMOKE_FAILED geography_not_ready"));
        UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
    }
    if(bSmoke && bOffline && WorldReady) {
        if(SmokeBegan==0) SmokeBegan=Now;
        const double Age=Now-SmokeBegan;
        bPlaying=Age>1 && Age<43;
        if(Age>40 && !bCaptured) {
            bCaptured=true;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/fly-offline.png"),false,false);
            UE_LOG(LogTemp,Display,TEXT("FLY_SMOKE_POSE %s"),*GetActorLocation().ToString());
        }
        if(Age>45) {
            if(ReceivedSequence>20 && bSmokeMoved && bSmokeAirborne && SmokeMaxDistance>100 && SmokeMaxAgl>3) {
                UE_LOG(LogTemp,Display,TEXT("ARRIETTY_UE_SMOKE_DONE packets=%d moved=1 airborne=1 distance_m=%.2f agl_m=%.2f"),ReceivedSequence,SmokeMaxDistance,SmokeMaxAgl); }
            else { UE_LOG(LogTemp,Error,TEXT("ARRIETTY_UE_SMOKE_FAILED packets=%d moved=%d airborne=%d distance_m=%.2f agl_m=%.2f"),ReceivedSequence,bSmokeMoved,bSmokeAirborne,SmokeMaxDistance,SmokeMaxAgl); }
            UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
        }
    }
    Send();
    if(!Socket) return;
    uint32 Available; int32 Iterations=0;
    while(Socket->HasPendingData(Available) && ++Iterations<=64)
    {
        TArray<uint8> Bytes; Bytes.SetNumUninitialized(FMath::Min(Available,65535u)+1);
        auto Sender=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        int32 Read=0;
        if(!Socket->RecvFrom(Bytes.GetData(),Bytes.Num()-1,Read,*Sender)) break;
        if(Sender->ToString(true)!=Remote->ToString(true)) continue;
        Bytes[Read]=0;
        TSharedPtr<FJsonObject> P;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(reinterpret_cast<char*>(Bytes.GetData()))),P) || !P.IsValid()) continue;
        FString ResponseToken; double ResponseSeq=0;
        if(!P->TryGetStringField(TEXT("token"),ResponseToken) || ResponseToken!=Token || !P->TryGetNumberField(TEXT("seq"),ResponseSeq) || ResponseSeq<=ReceivedSequence) continue;
        ReceivedSequence=ResponseSeq; LastPacket=Now;
        bool Playing=false; P->TryGetBoolField(TEXT("playing"),Playing);
        const TSharedPtr<FJsonObject>* Audio=nullptr;
        AudioInput.Active=false;
        if(Playing && P->TryGetObjectField(TEXT("audio"),Audio))
        {
            (*Audio)->TryGetBoolField(TEXT("active"),AudioInput.Active);
            (*Audio)->TryGetBoolField(TEXT("airborne"),AudioInput.Airborne);
            AudioInput.Speed=(*Audio)->GetNumberField(TEXT("speed"));
            AudioInput.Cadence=(*Audio)->GetNumberField(TEXT("cadence"));
            AudioInput.Power=(*Audio)->GetNumberField(TEXT("power"));
            AudioInput.Touchdowns=(*Audio)->GetIntegerField(TEXT("touchdowns"));
        }
        FString TerrainStatus;P->TryGetStringField(TEXT("terrain_status"),TerrainStatus);
        if(Panel) { Panel->Telemetry=P; Panel->bPlaying=Playing; Panel->Status=bOffline?TEXT("OFFLINE | R: ALIGN | ESC: SETUP"):TEXT("LIVE | R: ALIGN | ESC: SETUP"); }
        bool Ride=false; P->TryGetBoolField(TEXT("ride"),Ride);
        if(Panel && Playing && !Ride) Panel->Status=TEXT("READY | BUTTON 1: ALIGN AND START");
        if(Panel && !TerrainStatus.IsEmpty()) Panel->Status=TerrainStatus;
        if(!Playing)
        {
            double Ack=0;
            if(P->TryGetNumberField(TEXT("apply_id"),Ack) && int32(Ack)!=AppliedId)
            {
                AppliedId=Ack;
                const FString Error=P->GetStringField(TEXT("apply_error"));
                if(Error.IsEmpty())
                {
                    const FString AppliedTime=P->GetStringField(TEXT("local_time"));
                    // Initial ACK or an ACK for older edits must not start
                    // preparation while the rider is still editing the time.
                    if(AppliedId>0 && AppliedId==ApplyId && SetupDate==AppliedTime.Left(10) && SetupTime==AppliedTime.Mid(11,5)) bSetupDirty=false;
                    SetupMessage=TEXT("Applied: ")+AppliedTime;
                    const double Azimuth=P->GetNumberField(TEXT("sun_azimuth")),Elevation=P->GetNumberField(TEXT("sun_elevation"));
                    if(Geography) Geography->ApplySun(Azimuth,Elevation);
                    else for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
                    {
                        It->SetActorRotation(FRotator(-Elevation,Azimuth+180,0));
                        Cast<UDirectionalLightComponent>(It->GetLightComponent())->SetIntensity(50000*FMath::Clamp((Elevation+.3)/8.,0.,1.));
                    }
                }
                else SetupMessage=Error;
            }
        }
        const TArray<TSharedPtr<FJsonValue>>* Pose;
        if(bPlaying && Playing && P->TryGetArrayField(TEXT("pose"),Pose) && Pose->Num()==6)
        {
            FVector Position((*Pose)[0]->AsNumber(),(*Pose)[1]->AsNumber(),(*Pose)[2]->AsNumber());
            SmokeMaxDistance=FMath::Max(SmokeMaxDistance,Position.Size2D()*.01);
            double Agl=0;if(P->TryGetNumberField(TEXT("altitude_agl"),Agl)) SmokeMaxAgl=FMath::Max(SmokeMaxAgl,Agl);
            bSmokeMoved|=Position.Size2D()>100;
            bool Airborne=false; P->TryGetBoolField(TEXT("airborne"),Airborne); bSmokeAirborne|=Airborne;
            FRotator Rotation((*Pose)[3]->AsNumber(),(*Pose)[4]->AsNumber(),(*Pose)[5]->AsNumber());
            const double Bearing=Rotation.Yaw;
            if(Geography) {
                const FVector Local=Position*.01;
                FVector Velocity=FVector::ZeroVector;
                const TArray<TSharedPtr<FJsonValue>>* Motion=nullptr;
                if(P->TryGetArrayField(TEXT("world_velocity"),Motion) && Motion->Num()==3)
                    Velocity=FVector((*Motion)[0]->AsNumber(),(*Motion)[1]->AsNumber(),(*Motion)[2]->AsNumber());
                if(Velocity.ContainsNaN()) Velocity=FVector::ZeroVector;
                Geography->Track(Local.Y,Local.X,FVector2D(Velocity.Y,Velocity.X));
                Position=Geography->Position(Local.Y,Local.X,Local.Z);
                const FVector Next=Local+Velocity*.1;
                PathVelocity=(Geography->Position(Next.Y,Next.X,Next.Z)-Position)*10.;
                Rotation=Geography->Rotation(Rotation,Position);
            }
            double AppliedAlignment=0; P->TryGetNumberField(TEXT("alignment_applied"),AppliedAlignment);
            // An in-flight reply from before calibration must not rotate the
            // camera back to the old runway heading while its ack is pending.
            if(CanApplyPose(int32(AppliedAlignment)) && !Position.ContainsNaN() && !Rotation.ContainsNaN())
                SetActorLocationAndRotation(Position,Rotation);
            const int32 Request=P->GetIntegerField(TEXT("align_request"));
            double RecenterAck=0; P->TryGetNumberField(TEXT("recenter_id"),RecenterAck);
            if(Request>0 && Request!=Aligned && int32(RecenterAck)==RecenterId)
            {
                if(bOffline) { AlignmentBearing=Bearing; Aligned=Request; }
                else PendingAlignment=Request;
                if(Panel && !bOffline) Panel->Status=TEXT("ALIGNING | FACE BICYCLE FORWARD");
            }
        }
    }
    if(LastPacket>0 && Now-LastPacket>1)
    {
        StopSimulation();
        if(Panel) { Panel->Status=TEXT("CONNECTION LOST | RETURN TO SETUP"); Panel->bPlaying=false; }
    }
    auto CurrentAudio=AudioInput;
    const bool Tracked=bOffline || (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId));
    CurrentAudio.Active &= bPlaying && WorldReady && Tracked && LastPacket>0 && Now-LastPacket<=1;
    FlightAudio->Update(CurrentAudio,Delta);
}

void AArriettyPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    // Capture the view the rider is already looking at. Its XY projection is
    // the bicycle's new forward, not a correction back to the runway bearing.
    FQuat HmdOrientation=FQuat::Identity;
    FVector HmdPosition=FVector::ZeroVector;
    auto XR=GEngine?GEngine->XRSystem:nullptr;
    const bool Tracked=!bOffline && XR.IsValid() && XR->GetXRCamera().IsValid() && XR->IsHeadTrackingAllowedForWorld(*GetWorld()) &&
        XR->IsTracking(IXRTrackingSystem::HMDDeviceId) &&
        XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId,HmdOrientation,HmdPosition) &&
        !HmdOrientation.ContainsNaN() && HmdOrientation.SizeSquared()>UE_SMALL_NUMBER && !HmdPosition.ContainsNaN();
    bool Applied=false;
    double RawYaw=0;
    Camera->GetCameraView(DeltaTime,OutResult);
    if(Tracked)
    {
        HmdOrientation.Normalize();
        const FVector Forward=HmdOrientation.GetForwardVector();
        RawYaw=FMath::RadiansToDegrees(FMath::Atan2(Forward.Y,Forward.X));
        const FVector ViewForward=OutResult.Rotation.Vector();
        if(bPlaying && PendingAlignment>0 && ViewForward.SizeSquared2D()>1.e-4)
        {
            const double WorldYaw=FMath::RadiansToDegrees(FMath::Atan2(ViewForward.Y,ViewForward.X));
            AlignmentBearing=Geography?Geography->Bearing(OutResult.Rotation,GetActorLocation()):WorldYaw;
            const FQuat TrackingWorldRotation=Tracking->GetComponentQuat();
            FRotator BikeRotation=Geography?Geography->Rotation(FRotator(0,AlignmentBearing,0),GetActorLocation()):GetActorRotation(); if(!Geography) BikeRotation.Yaw=AlignmentBearing;
            SetActorRotation(BikeRotation);
            // Re-express the same tracking-to-world rotation under the new
            // vehicle yaw. This preserves the view, including head pitch/roll.
            const FQuat TrackingRotation=GetActorQuat().Inverse()*TrackingWorldRotation;
            Tracking->SetRelativeRotation(TrackingRotation);
            const FVector Offset=TrackingRotation.RotateVector(HmdPosition);
            Tracking->SetRelativeLocation(FVector(-Offset.X,-Offset.Y,0));
            Camera->GetCameraView(DeltaTime,OutResult);
            Applied=true;
        }
    }
    const double Residual=FRotator::NormalizeAxis(OutResult.Rotation.Yaw-GetActorRotation().Yaw);
    if(Applied && FMath::Abs(Residual)<1.0)
    {
        Aligned=PendingAlignment; PendingAlignment=0;
        UE_LOG(LogTemp,Display,TEXT("ARRIETTY_UE_HMD_ALIGNED id=%d raw_yaw=%.2f origin_yaw=%.2f bike_yaw=%.2f view_yaw=%.2f residual=%.3f"),
            Aligned,RawYaw,Tracking->GetRelativeRotation().Yaw,GetActorRotation().Yaw,OutResult.Rotation.Yaw,Residual);
    }
    const double Now=FPlatformTime::Seconds();
    if(bPlaying && Now-LastViewLog>=1)
    {
        LastViewLog=Now;
        UE_LOG(LogTemp,Display,TEXT("ARRIETTY_UE_VIEW aligned=%d pending=%d tracked=%d bike_yaw=%.2f view_yaw=%.2f raw_yaw=%.2f origin_yaw=%.2f relative_yaw=%.2f north_cm=%.1f east_cm=%.1f"),
            Aligned,PendingAlignment,Tracked,GetActorRotation().Yaw,OutResult.Rotation.Yaw,RawYaw,Tracking->GetRelativeRotation().Yaw,Residual,GetActorLocation().X,GetActorLocation().Y);
    }
}

void AArriettyPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    bPlaying=false; for(int I=0;I<3;++I) Send(true);
    if(Socket) { Socket->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); Socket=nullptr; }
    Super::EndPlay(Reason);
}
