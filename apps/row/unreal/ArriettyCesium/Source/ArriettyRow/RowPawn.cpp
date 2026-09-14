#include "RowPawn.h"
#include "RowPanel.h"
#include "RowWater.h"
#include "RowGeography.h"
#include "CesiumCreditSystem.h"
#include "UObject/UnrealType.h"
#include "RowModule.h"
#include "RowAudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "ProceduralMeshComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Console.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

// Windows maps both main Enter and extended (keypad) Enter to EKeys::Enter.
// Receive session controls before widget focus can consume them. Restrict this
// to the active game window, leaving other apps, editor windows and console alone.
class FRowKeyInput final:public IInputProcessor {
public:
    explicit FRowKeyInput(ARowPawn* pawn):Pawn(pawn) {}
    void Tick(float,FSlateApplication&,TSharedRef<ICursor>) override {}
    bool HandleKeyDownEvent(FSlateApplication& app,const FKeyEvent& event) override {
        if(!Accepts(app,event)) return false;
        if(!event.IsRepeat()) {
            Pawn->PendingCommands.Add(event.GetKey()==EKeys::Enter);
            UE_LOG(LogTemp,Display,TEXT("ROW_KEY key=%s route=preprocessor"),*event.GetKey().ToString());
        }
        return true; // Also consume repeats; one press must cause one action.
    }
    bool HandleKeyUpEvent(FSlateApplication& app,const FKeyEvent& event) override { return Accepts(app,event); }
    const TCHAR* GetDebugName() const override { return TEXT("RowKeypad"); }
private:
    bool Accepts(FSlateApplication& app,const FKeyEvent& event) const {
        if(!Pawn.IsValid() || event.IsAltDown() || event.IsControlDown() || event.IsCommandDown() || event.IsShiftDown()) return false;
        const auto key=event.GetKey();
        if(key!=EKeys::Enter && key!=EKeys::NumPadZero && key!=EKeys::Insert && key!=EKeys::Escape) return false;
        const auto world=Pawn->GetWorld();
        const auto viewport=world?world->GetGameViewport():nullptr;
        if(!viewport || (viewport->ViewportConsole && viewport->ViewportConsole->ConsoleActive())) return false;
        const auto window=viewport->GetWindow();
        return window.IsValid() && app.GetActiveTopLevelWindow()==window;
    }
    TWeakObjectPtr<ARowPawn> Pawn;
};

ARowPawn::ARowPawn() {
    PrimaryActorTick.bCanEverTick=true; AutoPossessPlayer=EAutoReceiveInput::Player0;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("BoatPosition"));
    RowAudio=CreateDefaultSubobject<URowAudioComponent>(TEXT("RowAudio"));
    Tracking=CreateDefaultSubobject<USceneComponent>(TEXT("XROrigin")); Tracking->SetupAttachment(RootComponent);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("HMD")); Camera->SetupAttachment(Tracking);
    Camera->bLockToHmd=true; Camera->bUsePawnControlRotation=false; Camera->SetFieldOfView(90);
    BoatRoot=CreateDefaultSubobject<USceneComponent>(TEXT("BoatVisuals")); BoatRoot->SetupAttachment(RootComponent);
    Hull=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Hull")); Hull->SetupAttachment(BoatRoot);
    Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instruments=CreateDefaultSubobject<UWidgetComponent>(TEXT("RowInstruments")); Instruments->SetupAttachment(Camera);
    Instruments->SetWidgetSpace(EWidgetSpace::World); Instruments->SetWidgetClass(URowPanel::StaticClass());
    Instruments->SetDrawSize(FVector2D(1000,360)); Instruments->SetRelativeLocation(FVector(115,0,-30));
    Instruments->SetRelativeRotation(FRotator(22,180,0)); Instruments->SetRelativeScale3D(FVector(.075));
    Instruments->SetTwoSided(true); Instruments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instruments->SetCastShadow(false); Instruments->SetWindowFocusable(false);
    Attribution=CreateDefaultSubobject<UWidgetComponent>(TEXT("MapAttribution")); Attribution->SetupAttachment(Camera);
    Attribution->SetWidgetSpace(EWidgetSpace::World); Attribution->SetDrawSize(FVector2D(1600,220));
    Attribution->SetRelativeLocation(FVector(150,0,-58)); Attribution->SetRelativeRotation(FRotator(20,180,0));
    Attribution->SetRelativeScale3D(FVector(.07)); Attribution->SetTwoSided(true);
    Attribution->SetCollisionEnabled(ECollisionEnabled::NoCollision); Attribution->SetCastShadow(false); Attribution->SetWindowFocusable(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> mesh(TEXT("/Engine/BasicShapes/Cube.Cube")); Cube=mesh.Object;
}
void ARowPawn::BeginPlay() {
    Super::BeginPlay(); Began=row::Devices::seconds();
    FString magnification;
    if(FParse::Value(FCommandLine::Get(),TEXT("RowMagnification="),magnification)) {
        MovementMagnification=FCString::Atod(*magnification);
        if(!magnification.IsNumeric() || !row::validMagnification(MovementMagnification)) {
            UE_LOG(LogTemp,Error,TEXT("ROW_OPTIONS_INVALID magnification_requires_1_to_10"));
            SetActorTickEnabled(false);
            UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
            return; // Reject before opening device workers.
        }
    }
    Geography=ARowGeography::Get(GetWorld());
    if(Geography) SetActorRotation(FRotator(0,Geography->GetSpawnYaw(),0));
    Home=GetActorTransform(); Home.SetLocation(FVector(GetActorLocation().X,GetActorLocation().Y,0)); SetActorTransform(Home);
    Model.heading=FMath::DegreesToRadians(GetActorRotation().Yaw);
    Offline=FParse::Param(FCommandLine::Get(),TEXT("RowOffline")); Demo=FParse::Param(FCommandLine::Get(),TEXT("RowDemo"));
    FParse::Value(FCommandLine::Get(),TEXT("RowQuitAfter="),QuitAfter);
    FParse::Value(FCommandLine::Get(),TEXT("RowScreenshotAt="),ScreenshotAt);
    FParse::Value(FCommandLine::Get(),TEXT("RowScreenshotPath="),ScreenshotPath);
    if(Demo) Offline=true;
    Chase=Offline && FParse::Param(FCommandLine::Get(),TEXT("RowChase"));
    if(Offline) { Camera->bLockToHmd=false; Camera->SetRelativeLocation(FVector(0,0,100)); Camera->SetRelativeRotation(FRotator(-7,0,0)); }
    else UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
    if(Chase) { Camera->SetRelativeLocation(FVector(-440,-440,310)); Camera->SetRelativeRotation(FRotator(-24,45,0)); Camera->SetFieldOfView(75); }
    if(Offline && FParse::Param(FCommandLine::Get(),TEXT("RowWaterView"))) {
        Chase=true; // Preview camera cannot become the calibrated travel heading.
        Camera->SetRelativeLocation(FVector(900,0,1100));
        Camera->SetRelativeRotation(FRotator(-35,180,0)); Camera->SetFieldOfView(75);
    }
    if(!Offline) {
        FString configPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../../../config/row.local.json"));
        FParse::Value(FCommandLine::Get(),TEXT("RowSettings="),configPath);
        FString json; TSharedPtr<FJsonObject> cfg; row::DeviceConfig dc; dc.deferVrShutdown=true;
        if(FFileHelper::LoadFileToString(json,*configPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(json),cfg)) {
            FString serial,address; cfg->TryGetStringField(TEXT("tracker_serial"),serial); dc.trackerSerial=TCHAR_TO_UTF8(*serial);
            auto readAddress=[&](const TCHAR* key) { FString a; cfg->TryGetStringField(key,a); a.ReplaceInline(TEXT(":"),TEXT("")); a.ReplaceInline(TEXT("-"),TEXT("")); return FCString::Strtoui64(*a,nullptr,16); };
            dc.rowerAddress=readAddress(TEXT("rower_address")); dc.heartAddress=readAddress(TEXT("heart_rate_address"));
            FString barInput; cfg->TryGetStringField(TEXT("bar_input"),barInput);
            UseImu=barInput==TEXT("wt9011dcl");
            if(UseImu) {
                dc.imuAddress=readAddress(TEXT("imu_address")); dc.trackerSerial.clear();
                FString type; cfg->TryGetStringField(TEXT("imu_address_type"),type);
                dc.imuAddressType=type==TEXT("random")?1:type==TEXT("public")?0:-1;
            }
            else if(!barInput.IsEmpty() && barInput!=TEXT("tracker")) {
                // An unknown selection must not silently choose another sensor.
                dc.trackerSerial.clear(); Notice=TEXT("Unknown bar_input in local settings");
            }
        } else Notice=TEXT("Local device settings missing");
        if(RowDeviceApiAvailable()) Devices=std::make_unique<row::Devices>(dc);
        else Notice=TEXT("OpenVR SDK missing / run bootstrap");
    }
    Instruments->InitWidget(); Panel=Cast<URowPanel>(Instruments->GetWidget());
    if(Panel) Panel->SetIsFocusable(false);
    if(auto mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Materials/M_Instruments.M_Instruments"))) Instruments->SetMaterial(0,mat);
    BuildBoat(); Water=GetWorld()->SpawnActor<ARowWater>();
    RowAudio->Initialize(Offline);
    if(auto pc=Cast<APlayerController>(GetController())) pc->SetInputMode(FInputModeGameOnly());
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
    if(FSlateApplication::IsInitialized()) {
        KeyInput=MakeShared<FRowKeyInput>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(KeyInput,0);
    }
    UE_LOG(LogTemp,Display,TEXT("ROW_READY offline=%d demo=%d reference_water_z_cm=0 movement_magnification=%g"),Offline,Demo,MovementMagnification);
}
void ARowPawn::SetupPlayerInputComponent(UInputComponent* input) {
    Super::SetupPlayerInputComponent(input);
    input->BindKey(EKeys::Enter,IE_Pressed,this,&ARowPawn::Toggle);
    input->BindKey(EKeys::NumPadZero,IE_Pressed,this,&ARowPawn::Stop);
    input->BindKey(EKeys::Insert,IE_Pressed,this,&ARowPawn::Stop);
    input->BindKey(EKeys::Escape,IE_Pressed,this,&ARowPawn::Stop);
}
row::Input ARowPawn::ReadInput() const {
    row::Input in; in.now=row::Devices::seconds(); in.bar=Snapshot.bar; in.head=Snapshot.head; in.telemetry=Snapshot.telemetry;
    in.useImu=UseImu && !Offline; in.imu=Snapshot.imu;
    if(Offline) {
        const bool calibrating=Model.state==row::State::Calibrating;
        const double phase=std::fmod(calibrating?CalibrationMotionTime:SimTime,2.8);
        const bool moving=(calibrating && Calibration.phase==row::CalibrationPhase::Axis) || (!calibrating && Demo);
        const double bar=moving?(phase<1?.38*std::cos(row::Pi*phase):-.38*std::cos(row::Pi*(phase-1)/1.8)):OfflineBarRest;
        in.bar={{bar,0,.65},{1,0,0},true,in.now};
        const bool straight=FParse::Param(FCommandLine::Get(),TEXT("RowDemoStraight"));
        const bool occlusionDemo=Demo && FParse::Param(FCommandLine::Get(),TEXT("RowDemoOcclusion"));
        in.head={{occlusionDemo && !calibrating?bar/1.3:0,Demo && !calibrating && !straight?.18*std::sin(SimTime*.07):0,1},{1,0,0},true,in.now};
        // Explicit offline fixture only. Never inject missing poses into VR.
        if(occlusionDemo && Model.state==row::State::Running && Model.elapsed>=4.) {
            const double cycle=std::fmod(Model.elapsed-4.,4.);
            if(cycle<.65) in.bar.valid=false;
        }
        if(Demo) {
            in.telemetry.power.set(95,in.now);
            in.telemetry.resistance.set(6,in.now);
        }
    } else {
        // OpenVR head provides a common physical room frame for lean/bar input;
        // also require the actual OpenXR rendering pose to be tracked.
        auto xr=GEngine?GEngine->XRSystem:nullptr;
        in.head.valid=in.head.valid && xr.IsValid() && xr->IsTracking(IXRTrackingSystem::HMDDeviceId);
    }
    return in;
}
void ARowPawn::Toggle() {
    if(Geography && !Geography->IsReady()) return;
    if(Model.state==row::State::Running) {
        Model.pause(); Notice.Empty(); Record(TEXT("pause"));
        UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=pause")); return;
    }
    if(Model.state==row::State::Calibrating) {
        // A second press must not cancel a start that is already in progress.
        // Some keypads also generate repeated complete down/up pairs when held.
        UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=calibration_continue")); return;
    }
    ShowSetupPanel();
    const auto in=ReadInput();
    if(!Calibration.begin(in)) {
        Notice=TEXT("Enter received / check HMD + bar");
        UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=start_blocked head_valid=%d bar_valid=%d imu=%d"),in.head.valid,row::Model::barTracked(in),UseImu);
        return;
    }
    Model.calibrate(); CalibrationMotionTime=0; Notice.Empty(); Record(TEXT("calibration_begin"));
    UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=calibration_begin"));
}
void ARowPawn::ShowSetupPanel() {
    // Before neutral calibration the physical room origin may be far from the
    // boat. Keep setup instructions in view, then return instruments to the boat.
    Instruments->AttachToComponent(Camera,FAttachmentTransformRules::KeepRelativeTransform);
    Instruments->SetRelativeLocation(FVector(115,0,-30));
    Instruments->SetRelativeRotation(FRotator(15,180,0));
}
void ARowPawn::FinishCalibration(const row::Input& in) {
    if(!Model.start(in,Calibration.frame)) {
        Model.state=row::State::TrackingLost; Notice=TEXT("SETUP LOST / NUM ENTER"); return;
    }
    if(Offline) { OfflineBarRest=in.bar.position.x; SimTime=CalibrationMotionTime; }
    // Convert the measured physical machine axis into the actual rendered world.
    // Subtract current physical head yaw so looking sideways cannot skew it.
    const double headYaw=FMath::RadiansToDegrees(std::atan2(in.head.forward.y,in.head.forward.x));
    const double axisYaw=FMath::RadiansToDegrees(std::atan2(Model.forward.y,Model.forward.x));
    Model.heading=FMath::DegreesToRadians(Chase?GetActorRotation().Yaw:
        Camera->GetComponentRotation().Yaw+FMath::FindDeltaAngleDegrees(headYaw,axisYaw));
    if(!Offline) {
        const FVector roomEye=Camera->GetRelativeLocation();
        const auto offset=in.head.position-Calibration.frame.center;
        const FRotator roomRotation(0,Camera->GetRelativeRotation().Yaw-headYaw,0);
        const FVector fromNeutral=roomRotation.RotateVector(FVector(offset.x,offset.y,offset.z)*100);
        Tracking->SetRelativeLocation(fromNeutral-roomEye+FVector(0,0,100));
    }
    const FRotator facing(0,FMath::RadiansToDegrees(Model.heading),0);
    Instruments->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepWorldTransform);
    BoatRoot->SetWorldRotation(facing); Instruments->SetWorldRotation(FRotator(22,facing.Yaw+180,0));
    Instruments->SetWorldLocation(GetActorLocation()+facing.RotateVector(FVector(115,0,53)));
    if(SessionFile.IsEmpty()) {
        const FString dir=FPaths::ProjectSavedDir()/TEXT("Sessions"); IFileManager::Get().MakeDirectory(*dir,true);
        SessionFile=dir/FString::Printf(TEXT("row-%s.csv"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S%ss")));
        FFileHelper::SaveStringToFile(TEXT("utc,event,active_s,distance_m,speed_kmh,heart_bpm,strokes,source,machine_distance_m,machine_elapsed_s,machine_speed_kmh,power_w,lean_cm,steer,yaw_deg_s,resistance_level,power_multiplier,game_power_w,bar_source,bar_gap_s,tracking_issue,movement_magnification,world_speed_kmh,resistance_raw,resistance_age_s\n"),*SessionFile);
    }
    Notice.Empty(); Record(TEXT("start"));
    UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=start"));
}
void ARowPawn::Stop() {
    if(!SessionFile.IsEmpty()) Record(TEXT("stop")); SessionFile.Empty();
    Model.reset(); Calibration={}; CalibrationMotionTime=0; SimTime=0; DemoStarted=true; SetActorTransform(Home);
    Model.heading=FMath::DegreesToRadians(Home.Rotator().Yaw);
    BoatRoot->SetRelativeRotation(FRotator::ZeroRotator);
    ShowSetupPanel();
    Notice=TEXT("Stopped / Home");
    UE_LOG(LogTemp,Display,TEXT("ROW_CONTROL action=stop_home"));
}
void ARowPawn::Tick(float dt) {
    Super::Tick(dt); if(Devices) Snapshot=Devices->snapshot();
    if(Geography && !Offline && !AttributionAttached) {
        // Cesium owns and updates this widget. Present it in the HMD as well as the scene.
        auto credits=ACesiumCreditSystem::GetDefaultCreditSystem(this);
        auto field=credits?FindFProperty<FObjectProperty>(credits->GetClass(),TEXT("CreditsWidget")):nullptr;
        auto widget=field?Cast<UUserWidget>(field->GetObjectPropertyValue_InContainer(credits)):nullptr;
        if(widget) {
            widget->RemoveFromParent(); Attribution->SetWidget(widget); AttributionAttached=true;
            UE_LOG(LogTemp,Display,TEXT("ROW_ATTRIBUTION_HMD attached=1"));
        }
    }
    if(Geography && !Geography->IsReady()) {
        PendingCommands.Empty();
        if(Panel) {
            Panel->Status=Geography->GetMessage(); Panel->Detail=Geography->PlaceLabel;
            Panel->Guide=TEXT("Preparing scenery. Rowing starts after the terrain is ready.");
        }
        const double loadingTime=row::Devices::seconds()-Began;
        if(ScreenshotAt>0 && loadingTime>ScreenshotAt && !ScreenshotPath.IsEmpty()) {
            FScreenshotRequest::RequestScreenshot(ScreenshotPath,false,false); ScreenshotAt=0;
        }
        if(QuitAfter>0 && loadingTime>QuitAfter) UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
        return;
    }
    // Run world/session mutations on the pawn tick, after refreshing devices.
    TArray<bool> commands; Swap(commands,PendingCommands);
    for(bool toggle:commands) { if(toggle) Toggle(); else Stop(); }
    if(Snapshot.bar.valid) ++ValidBarFrames;
    if(Demo && !DemoStarted && row::Devices::seconds()-Began>8) { Toggle(); DemoStarted=true; }
    const auto in=ReadInput(); const auto oldState=Model.state; const double oldHeading=Model.heading;
    const auto oldBarSource=Model.barTracking.source;
    if(Model.state==row::State::Calibrating) {
        const auto oldPhase=Calibration.phase;
        Calibration.tick(in,dt);
        if(Calibration.phase==row::CalibrationPhase::Axis) CalibrationMotionTime+=dt;
        if(Calibration.phase!=oldPhase) UE_LOG(LogTemp,Display,TEXT("ROW_CALIBRATION phase=%d"),int(Calibration.phase));
        if(Calibration.phase==row::CalibrationPhase::Complete) FinishCalibration(in);
        else if(Calibration.phase==row::CalibrationPhase::Failed) {
            Model.state=Calibration.issue==row::CalibrationIssue::Tracking?row::State::TrackingLost:row::State::Paused;
            Notice=Calibration.issue==row::CalibrationIssue::Tracking?TEXT("SETUP LOST / NUM ENTER"):TEXT("SETUP TIMEOUT / NUM ENTER");
            UE_LOG(LogTemp,Display,TEXT("ROW_CALIBRATION_FAILED issue=%d dt_s=%.4f bar_valid=%d head_valid=%d bar_age_s=%.4f head_age_s=%.4f"),
                int(Calibration.issue),dt,row::Model::barTracked(in),row::Model::headTracking(in),in.now-in.bar.received,in.now-in.head.received);
            Record(TEXT("calibration_failed"));
        }
    }
    // Pose derivatives use the real frame interval; a hitch invokes core watchdog.
    const double moved=oldState==row::State::Calibrating?0:Model.tick(in,dt,MovementMagnification);
    if(Model.state==row::State::Running) SimTime+=dt;
    if(oldState==row::State::Running && (oldBarSource!=Model.barTracking.source || Model.state==row::State::TrackingLost)) {
        Record(Model.state==row::State::TrackingLost?TEXT("tracking_lost"):TEXT("bar_source_changed"));
        UE_LOG(LogTemp,Display,TEXT("ROW_TRACKING source=%s gap_s=%.3f issue=%s dt_s=%.4f bar_valid=%d head_valid=%d"),
            UTF8_TO_TCHAR(row::barSourceName(Model.barTracking.source)),Model.barTracking.gapSeconds,
            UTF8_TO_TCHAR(row::trackingIssueName(Model.barTracking.issue)),dt,row::Model::barTracked(in),row::Model::headTracking(in));
    }
    if(moved>0) {
        const FVector delta(std::cos(Model.heading)*moved*100,std::sin(Model.heading)*moved*100,0);
        FHitResult hit;
        FCollisionQueryParams params(SCENE_QUERY_STAT(RowBank),false,this);
        const FVector from=GetActorLocation()+FVector(0,0,35);
        FVector next=GetActorLocation()+delta;
        const bool mappedWater=!Geography || Geography->CanNavigatePath(GetActorLocation(),next+delta.GetSafeNormal()*250);
        if(Geography) next.Z=Geography->SurfaceHeightCm(next.X,next.Y);
        // Keep each mesh sweep short at higher magnification and follow the
        // curved mean water. Retain the existing hidden-imagery tolerance.
        bool exposedTerrain=false;
        FVector sweepFrom=from;
        const int steps=FMath::Max(1,FMath::CeilToInt(delta.Size()/50.));
        for(int i=1;i<=steps;++i) {
            FVector sweepTo=GetActorLocation()+delta*(double(i)/steps);
            if(Geography) sweepTo.Z=Geography->SurfaceHeightCm(sweepTo.X,sweepTo.Y);
            sweepTo.Z+=35;
            const bool collision=GetWorld()->SweepSingleByChannel(hit,sweepFrom,sweepTo,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(30),params);
            if(collision && (!Geography || hit.ImpactPoint.Z>Geography->SurfaceHeightCm(hit.ImpactPoint.X,hit.ImpactPoint.Y)+3)) {
                exposedTerrain=true; break;
            }
            sweepFrom=sweepTo;
        }
        if(!mappedWater || exposedTerrain) {
            Model.distance-=moved; Model.pause(); Notice=TEXT("Shore / paused"); Record(TEXT("shore"));
        } else {
            SetActorLocation(next);
            AddActorWorldRotation(FRotator(0,FMath::RadiansToDegrees(Model.heading-oldHeading),0));
        }
    }
    // Wave following is visual only. The level HMD follows the curved mean water.
    const double time=row::Devices::seconds()-Began;
    BoatRoot->SetRelativeLocation(FVector(0,0,1.1*std::sin(time*1.4)));
    const auto br=BoatRoot->GetRelativeRotation();
    BoatRoot->SetRelativeRotation(FRotator(.25*std::sin(time*1.8),br.Yaw,.5*std::sin(time*1.3)));
    OarBlend=FMath::FInterpTo(OarBlend,float(Model.drive),dt,8);
    for(int i=0;i<Oars.Num();++i) {
        const int side=i==0?-1:1;
        Oars[i]->SetRelativeRotation(FRotator(0,side*(68+36*OarBlend),-side*(8-12*OarBlend)));
    }
    if(Water) Water->UpdateBoat(GetActorLocation(),Model.heading,Model.speed,Model.drive,dt);
    RowAudio->Update(Model,dt);
    if(Panel) {
        Panel->Distance=FString::Printf(TEXT("%.0f m"),Model.distance);
        const int seconds=int(Model.elapsed); Panel->Time=FString::Printf(TEXT("%02d:%02d"),seconds/60,seconds%60);
        Panel->MovementMagnification=MovementMagnification;
        Panel->Speed=FString::Printf(TEXT("%.1f"),Model.speed*3.6*MovementMagnification);
        const auto hr=Snapshot.heart;
        Panel->Heart=hr.fresh(in.now,5) && hr.value>0?FString::Printf(TEXT("%.0f"),hr.value):TEXT("--");
        const TCHAR* rowing=Model.barTracking.source==row::BarSource::Imu?TEXT("ROWING / WIT IMU"):
            Model.barTracking.source==row::BarSource::HmdAssist?TEXT("ROWING / HMD ASSIST"):
            Model.barTracking.source==row::BarSource::Coast?TEXT("BAR LOST / COASTING"):
            Model.barTracking.source==row::BarSource::Reacquiring?TEXT("BAR RETURNING / COASTING"):TEXT("ROWING");
        const TCHAR* state=Model.state==row::State::Running?rowing:Model.state==row::State::Paused?TEXT("PAUSED"):
            Model.state==row::State::TrackingLost?TEXT("TRACKING LOST / NUM ENTER"):TEXT("READY / NUM ENTER");
        Panel->Status=!Notice.IsEmpty()?Notice:Demo?FString::Printf(TEXT("DEMO / %s"),state):FString(state);
        Panel->Guide=TEXT("NUM ENTER Start / Pause     NUM 0 Stop / Home     Lean left / right to steer");
        Panel->SteeringAvailable=Calibration.frame.valid && row::Model::headTracking(in) && Model.state!=row::State::Calibrating;
        Panel->LeanCm=float((Model.state==row::State::Running?Model.lean:row::dot(in.head.position-Model.neutralHead,Model.right))*100);
        if(Model.state==row::State::Calibrating) {
            if(Calibration.phase==row::CalibrationPhase::Settle) {
                Panel->Status=FString::Printf(TEXT("1/3  GET READY  %.1f s"),Calibration.remaining);
                Panel->Guide=UseImu?TEXT("Face straight along the machine. Extend the bar and hold it still."):
                    TEXT("Release the keypad. Sit in your normal centered rowing posture.");
            } else if(Calibration.phase==row::CalibrationPhase::Center) {
                Panel->Status=FString::Printf(TEXT("2/3  HOLD STILL  %.1f s"),Calibration.remaining);
                Panel->Guide=UseImu?TEXT("Keep facing straight: this sets steering center. Hold head and bar still."):
                    TEXT("Face the machine and hold your normal posture for one quiet second.");
            } else {
                Panel->Status=FString::Printf(TEXT("3/3  ROW THE BAR  %u / 2"),Calibration.strokes);
                Panel->Guide=Calibration.issue==row::CalibrationIssue::LookForward?TEXT("Face along the machine. NUM 0 then NUM ENTER to retry."):
                    Calibration.issue==row::CalibrationIssue::KeepStraight?TEXT("Move the bar straight. NUM 0 then NUM ENTER to retry."):
                    UseImu?TEXT("Pull the extended bar toward you, then return. Twice. NUM 0 cancels."):
                    TEXT("ENTER received. Move the bar out and back twice. Starts automatically. NUM 0 cancels.");
            }
        }
        const bool estimateAvailable=Model.state==row::State::Running &&
            (Model.barTracking.source==row::BarSource::Tracker || Model.barTracking.source==row::BarSource::HmdAssist || Model.barTracking.source==row::BarSource::Imu);
        const auto output=row::samplePower(in.telemetry,in.now,estimateAvailable?Model.barVelocity:0.);
        const bool powerAvailable=output.usingBt || estimateAvailable;
        const FString watts=powerAvailable?FString::Printf(TEXT("%.0f"),output.usingBt?output.machineWatts:output.baseWatts):TEXT("--");
        const FString load=output.resistance>=1?FString::Printf(TEXT("%.0f"),output.resistance):TEXT("-- (x1)");
        const FString game=powerAvailable?FString::Printf(TEXT("%.0f"),output.gameWatts):TEXT("--");
        Panel->Detail=FString::Printf(TEXT("%s %s W  |  LOAD %s  |  GAME %s W  |  %u strokes"),
            output.usingBt?TEXT("BT"):UseImu?TEXT("IMU est"):Model.barTracking.source==row::BarSource::HmdAssist?TEXT("HMD est"):TEXT("Tracker est"),*watts,*load,*game,Model.strokes);
    }
    if(!SessionFile.IsEmpty() && in.now>=NextRecord) { Record(TEXT("sample")); NextRecord=in.now+1; }
    const bool captureAssist=Offline && Demo && FParse::Param(FCommandLine::Get(),TEXT("RowCaptureAssist")) &&
        Model.barTracking.source==row::BarSource::HmdAssist && Model.barTracking.gapSeconds>.1;
    if(ScreenshotAt>0 && (time>ScreenshotAt || captureAssist) && !ScreenshotPath.IsEmpty()) {
        FScreenshotRequest::RequestScreenshot(ScreenshotPath,false,false); ScreenshotAt=0;
        UE_LOG(LogTemp,Display,TEXT("ROW_PREVIEW_CAPTURE distance_m=%.2f state=%d"),Model.distance,int(Model.state));
    }
    if(QuitAfter>0 && time>QuitAfter) UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
}
void ARowPawn::Record(const TCHAR* event) {
    if(SessionFile.IsEmpty()) return;
    const auto in=ReadInput(); const double now=in.now; const auto& t=in.telemetry;
    const bool estimateAvailable=Model.state==row::State::Running &&
        (Model.barTracking.source==row::BarSource::Tracker || Model.barTracking.source==row::BarSource::HmdAssist || Model.barTracking.source==row::BarSource::Imu);
    const auto output=row::samplePower(t,now,estimateAvailable?Model.barVelocity:0.);
    auto value=[&](const row::Field& f,double scale=1.) { return f.fresh(now,5) && f.value>=0?FString::Printf(TEXT("%.3f"),f.value*scale):FString(); };
    const FString hr=Snapshot.heart.value>0?value(Snapshot.heart):FString();
    const FString speed=t.pace.fresh(now) && t.pace.value>=0?FString::Printf(TEXT("%.3f"),t.pace.value>0?1800./t.pace.value:0.):FString();
    const FString resistance=output.resistance>=1?FString::Printf(TEXT("%.0f"),output.resistance):FString();
    const FString game=output.usingBt || estimateAvailable?FString::Printf(TEXT("%.3f"),output.gameWatts):FString();
    const FString machinePower=output.usingBt?FString::Printf(TEXT("%.3f"),output.machineWatts):FString();
    // Preserve invalid/stale dial observations for diagnosis without using them
    // as power gain. No identity, raw BLE payload or device command is recorded.
    const bool haveResistance=t.resistance.received>=0 && FMath::IsFinite(t.resistance.value);
    const FString rawResistance=haveResistance?FString::Printf(TEXT("%.0f"),t.resistance.value):FString();
    const FString resistanceAge=haveResistance?FString::Printf(TEXT("%.3f"),now-t.resistance.received):FString();
    const FString line=FString::Printf(TEXT("%s,%s,%.3f,%.3f,%.3f,%s,%u,%s,%s,%s,%s,%s,%.2f,%.3f,%.3f,%s,%.0f,%s,%s,%.3f,%s,%g,%.3f,%s,%s\n"),
        *FDateTime::UtcNow().ToIso8601(),event,Model.elapsed,Model.distance,Model.speed*3.6,*hr,Model.strokes,
        Demo?TEXT("demo"):output.usingBt?TEXT("bt"):UseImu?TEXT("imu_estimate"):Model.barTracking.source==row::BarSource::HmdAssist?TEXT("hmd_estimate"):TEXT("tracker_estimate"),*value(t.distance),*value(t.elapsed),*speed,*machinePower,
        Model.lean*100,Model.steer,FMath::RadiansToDegrees(Model.yawRate),*resistance,output.multiplier,*game,
        UTF8_TO_TCHAR(row::barSourceName(Model.barTracking.source)),Model.barTracking.gapSeconds,
        UTF8_TO_TCHAR(row::trackingIssueName(Model.barTracking.issue)),MovementMagnification,Model.speed*3.6*MovementMagnification,*rawResistance,*resistanceAge);
    FFileHelper::SaveStringToFile(line,*SessionFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
}
void ARowPawn::EndPlay(const EEndPlayReason::Type reason) {
    if(KeyInput && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(KeyInput);
    KeyInput.Reset(); PendingCommands.Empty();
    Record(TEXT("exit"));
    if(Devices) UE_LOG(LogTemp,Display,TEXT("ROW_DEVICE_SUMMARY tracker_frames=%llu rower_packets=%u heart_packets=%u rejected=%u"),
        ValidBarFrames,Snapshot.telemetry.packets,Snapshot.heartPackets,Snapshot.telemetry.rejected);
    if(Devices && UseImu) UE_LOG(LogTemp,Display,TEXT("ROW_IMU_SUMMARY packets=%llu rejected=%u errors=%u"),
        Snapshot.imu.sequence,Snapshot.imuRejected,Snapshot.imuErrors);
    Devices.reset(); Super::EndPlay(reason);
}
void ARowPawn::BuildBoat() {
    auto base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Materials/M_Hull.M_Hull"));
    Hull->SetMaterial(0,base);
    // Open rowing shell: tapered keel and gunwales, with a second inner surface.
    TArray<FVector> v,n; TArray<FVector2D> uv; TArray<int32> idx;
    constexpr int rings=41,across=17;
    for(int inner=0;inner<2;++inner) for(int i=0;i<rings;++i) {
        const float along=float(i)/(rings-1),x=-230+460*along;
        const float width=FMath::Max(2.f,45.f*FMath::Pow(FMath::Max(0.f,FMath::Sin(PI*along)),.55f))-inner*1.3f;
        for(int j=0;j<across;++j) {
            const float a=float(j)/(across-1)*PI;
            v.Add(FVector(x,-width*FMath::Cos(a),22-38*FMath::Sin(a)+inner*2));
            n.Add(FVector(0,-FMath::Cos(a)*(inner?-1:1),-FMath::Sin(a)*(inner?-1:1)));
            uv.Add(FVector2D(along,float(j)/(across-1)));
        }
    }
    for(int side=0;side<2;++side) for(int i=0;i<rings-1;++i) for(int j=0;j<across-1;++j) {
        int k=side*rings*across+i*across+j;
        if(side) idx.Append({k,k+across,k+1,k+1,k+across,k+across+1});
        else idx.Append({k,k+1,k+across,k+1,k+across+1,k+across});
    }
    Hull->CreateMeshSection_LinearColor(0,v,idx,n,uv,{}, {},false);
    auto wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Materials/M_Wood.M_Wood"));
    auto add=[&](const TCHAR* name,FVector p,FVector scale,USceneComponent* parent) {
        auto m=NewObject<UStaticMeshComponent>(this,name); m->SetupAttachment(parent); m->SetStaticMesh(Cube);
        m->SetRelativeLocation(p); m->SetRelativeScale3D(scale); m->SetMaterial(0,wood);
        m->SetCollisionEnabled(ECollisionEnabled::NoCollision); m->RegisterComponent(); return m;
    };
    add(TEXT("Seat"),FVector(-20,0,18),FVector(.32,.75,.035),BoatRoot);
    add(TEXT("Footboard"),FVector(75,0,5),FVector(.18,.55,.035),BoatRoot);
    for(int side:{-1,1}) {
        auto pivot=NewObject<UStaticMeshComponent>(this); pivot->SetupAttachment(BoatRoot); pivot->SetRelativeLocation(FVector(20,side*48,26)); pivot->RegisterComponent();
        add(side<0?TEXT("LeftShaft"):TEXT("RightShaft"),FVector(75,0,0),FVector(2.6,.035,.035),pivot);
        add(side<0?TEXT("LeftBlade"):TEXT("RightBlade"),FVector(188,0,0),FVector(.52,.22,.018),pivot);
        Oars.Add(pivot);
    }
}
