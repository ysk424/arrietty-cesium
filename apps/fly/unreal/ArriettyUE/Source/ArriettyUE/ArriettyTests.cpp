#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "ArriettyPawn.h"
#include "ArriettyWorld.h"
#include "CesiumGeoreference.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "XRTrackingSystemBase.h"

namespace
{
// Uses UE's real DefaultXRCamera and UCameraComponent with simulated poses.
// No OpenXR session, SteamVR, device worker or UDP bridge is started.
class FArriettyTestXR : public FXRTrackingSystemBase
{
public:
    FArriettyTestXR() : FXRTrackingSystemBase(nullptr) {}
    FQuat Orientation=FQuat::Identity;
    FVector Position=FVector(125,-80,160);
    bool Valid=true;
    bool AllowCamera=true;
    virtual FName GetSystemName() const override { return TEXT("ArriettyTestXR"); }
    virtual int32 GetXRSystemFlags() const override { return 0; }
    virtual bool EnumerateTrackedDevices(TArray<int32>& Devices, EXRTrackedDeviceType Type) override { Devices.Add(0); return true; }
    virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override
    { OutOrientation=Orientation; OutPosition=Position; return Valid; }
    virtual float GetWorldToMetersScale() const override { return 100; }
    virtual void ResetOrientationAndPosition(float Yaw) override {}
    virtual bool IsHeadTrackingAllowed() const override { return true; }
    virtual bool IsHeadTrackingAllowedForWorld(UWorld&) const override { return true; }
    virtual TSharedPtr<IXRCamera,ESPMode::ThreadSafe> GetXRCamera(int32 DeviceId=0) override
    { return AllowCamera?FXRTrackingSystemBase::GetXRCamera(DeviceId):nullptr; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArriettyAttitudeTest,"Arrietty.Coordinates.Attitude",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArriettyAttitudeTest::RunTest(const FString&)
{
    // Tests physical nose/wing directions, independent of Python conversion.
    TestTrue(TEXT("UE positive pitch raises nose"),FRotator(6,0,0).RotateVector(FVector::ForwardVector).Z>0);
    TestTrue(TEXT("Left bank converted to UE -roll lowers left wing"),FRotator(0,0,-10).RotateVector(FVector(0,-1,0)).Z<0);
    TestTrue(TEXT("Right bank converted to UE +roll lowers right wing"),FRotator(0,0,10).RotateVector(FVector(0,1,0)).Z<0);
    TestTrue(TEXT("North points +X"),FRotator(0,0,0).Vector().Equals(FVector(1,0,0),1.e-6));
    TestTrue(TEXT("East points +Y"),FRotator(0,90,0).Vector().Equals(FVector(0,1,0),1.e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArriettyHmdAlignmentTest,"Arrietty.Coordinates.HmdAlignment",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArriettyHmdAlignmentTest::RunTest(const FString&)
{
    const auto PreviousXR=GEngine->XRSystem;
    auto XR=MakeShared<FArriettyTestXR,ESPMode::ThreadSafe>();
    GEngine->XRSystem=XR;
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto Pawn=World->SpawnActor<AArriettyPawn>();
    // No BeginPlay: the test exercises the camera without opening a socket.
    Pawn->bOffline=false; Pawn->bPlaying=true;
    int32 Request=0;
    FMinimalViewInfo View;
    for(double BikeYaw : {0.,90.,223.075})
    {
        for(double HeadYaw : {0.,45.,135.,180.,-137.})
        {
            Pawn->SetActorLocationAndRotation(FVector::ZeroVector,FRotator(0,BikeYaw,0));
            Pawn->Tracking->SetRelativeTransform(FTransform::Identity);
            XR->Orientation=FRotator(-12,HeadYaw,5).Quaternion();
            Pawn->Camera->SetRelativeRotation(FRotator(0,180,0)); // stale camera
            FMinimalViewInfo Before;
            Pawn->Camera->GetCameraView(.01f,Before);
            Pawn->PendingAlignment=++Request;
            Pawn->CalcCamera(.01f,View);
            TestEqual(TEXT("Only a confirmed camera acknowledges alignment"),Pawn->Aligned,Request);
            const FVector RideForward=Pawn->GetActorForwardVector();
            TestTrue(TEXT("Button 1 keeps the direction the rider was looking"),Before.Rotation.Quaternion().AngularDistance(View.Rotation.Quaternion())<1.e-5);
            TestTrue(TEXT("HMD forward sets the course instead of the initial runway heading"),RideForward.Equals(Before.Rotation.Vector().GetSafeNormal2D(),1.e-5));
            TestTrue(TEXT("Confirmed bearing is sent to the physics bridge"),FMath::Abs(FRotator::NormalizeAxis(Pawn->AlignmentBearing-Before.Rotation.Yaw))<1.e-5);
            TestTrue(TEXT("Final camera faces along the bicycle"),View.Rotation.Vector().GetSafeNormal2D().Equals(RideForward,1.e-5));
            TestTrue(TEXT("Room offset removed, floor-relative eye height retained"),View.Location.Equals(FVector(0,0,160),1.e-5));
            const FVector InitialEye=View.Location;
            Pawn->SetActorLocation(RideForward*300);
            Pawn->CalcCamera(.01f,View);
            TestTrue(TEXT("Forward pedalling moves the actual view forward"),(View.Location-InitialEye).Equals(View.Rotation.Vector().GetSafeNormal2D()*300,1.e-5));
            XR->Orientation=FRotator(-12,HeadYaw+40,5).Quaternion();
            Pawn->CalcCamera(.01f,View);
            TestTrue(TEXT("Looking sideways remains independent of steering"),FMath::Abs(FRotator::NormalizeAxis(View.Rotation.Yaw-Pawn->AlignmentBearing)-40)<1.e-5);
            TestTrue(TEXT("Head turning does not change the latched bicycle forward"),Pawn->GetActorForwardVector().Equals(RideForward,1.e-5));
            TestFalse(TEXT("A reply from before alignment cannot restore the runway heading"),Pawn->CanApplyPose(Request-1));
            TestTrue(TEXT("The applied bearing allows subsequent simulation poses"),Pawn->CanApplyPose(Request));
        }
    }
    Pawn->SetActorRotation(FRotator(6,223.075,-12));
    XR->Orientation=FRotator(-12,-21.69,5).Quaternion();
    FMinimalViewInfo BankedBefore;
    Pawn->Camera->GetCameraView(.01f,BankedBefore);
    Pawn->PendingAlignment=++Request;
    Pawn->CalcCamera(.01f,View);
    TestEqual(TEXT("Recalibration also works with flight pitch and bank"),Pawn->Aligned,Request);
    TestTrue(TEXT("Flight recenter preserves the view orientation"),View.Rotation.Quaternion().AngularDistance(BankedBefore.Rotation.Quaternion())<1.e-5);
    TestTrue(TEXT("Flight forward uses the view's horizontal projection"),Pawn->GetActorForwardVector().GetSafeNormal2D().Equals(View.Rotation.Vector().GetSafeNormal2D(),1.e-5));
    Pawn->Aligned=0; Pawn->PendingAlignment=++Request;
    XR->Valid=false;
    Pawn->CalcCamera(.01f,View);
    TestEqual(TEXT("Invalid tracking cannot enable movement"),Pawn->Aligned,0);
    XR->Valid=true;
    XR->AllowCamera=false;
    Pawn->Camera->SetRelativeRotation(FRotator(0,180,0));
    XR->Orientation=FQuat::Identity;
    Pawn->CalcCamera(.01f,View);
    TestEqual(TEXT("Valid pose with a backwards rendered camera cannot enable movement"),Pawn->Aligned,0);
    XR->AllowCamera=true;
    Pawn->CalcCamera(.01f,View);
    TestEqual(TEXT("Camera availability retries the pending alignment"),Pawn->Aligned,Request);
    // Repeat the physical view/forward test on Cesium's east/south/up frame.
    auto Geography=World->SpawnActor<AArriettyWorld>();
    Geography->Geo=World->SpawnActor<ACesiumGeoreference>();
    Geography->OriginLLH=FVector(138.7,35.3,1500);
    Geography->Geo->SetOriginLongitudeLatitudeHeight(Geography->OriginLLH);
    Geography->OriginSet=true;Pawn->Geography=Geography;
    for(double HeadYaw : {0.,45.,135.,180.,-137.}) {
        const FVector Location=Geography->Position(0,0,100);
        Pawn->SetActorLocationAndRotation(Location,Geography->Rotation(FRotator::ZeroRotator,Location));
        Pawn->Tracking->SetRelativeTransform(FTransform::Identity);
        XR->Orientation=FRotator(-12,HeadYaw,5).Quaternion();
        FMinimalViewInfo Before;Pawn->Camera->GetCameraView(.01f,Before);
        Pawn->PendingAlignment=++Request;Pawn->CalcCamera(.01f,View);
        TestEqual(TEXT("Cesium camera acknowledges alignment"),Pawn->Aligned,Request);
        TestTrue(TEXT("Cesium alignment preserves the actual view"),Before.Rotation.Quaternion().AngularDistance(View.Rotation.Quaternion())<1.e-5);
        const FVector A=Geography->Position(0,0,100);
        const double Bearing=FMath::DegreesToRadians(Pawn->AlignmentBearing);
        const FVector B=Geography->Position(FMath::Sin(Bearing),FMath::Cos(Bearing),100);
        TestTrue(TEXT("Geographic motion follows the pre-button view"),(B-A).GetSafeNormal2D().Equals(Before.Rotation.Vector().GetSafeNormal2D(),1.e-5));
    }
    World->DestroyWorld(false);
    GEngine->XRSystem=PreviousXR;
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArriettyCesiumCoordinatesTest,"Arrietty.Coordinates.Cesium",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArriettyCesiumCoordinatesTest::RunTest(const FString&) {
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto Scene=World->SpawnActor<AArriettyWorld>();Scene->Geo=World->SpawnActor<ACesiumGeoreference>();
    Scene->OriginLLH=FVector(138.7,35.3,1500);Scene->Geo->SetOriginLongitudeLatitudeHeight(Scene->OriginLLH);Scene->OriginSet=true;
    for(double H : {-300.,0.,100.}) for(double E : {0.,10000.}) {
        const FVector Position=Scene->Position(E,0,H);
        const FVector LLH=Scene->Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Position);
        TestTrue(TEXT("Curved position preserves ellipsoid height including valleys"),FMath::Abs(LLH.Z-(1500+H))<.0001);
    }
    TestTrue(TEXT("Launch ground maps to zero"),Scene->Position(0,0,0).IsNearlyZero(.001));
    const auto P=Scene->Position(0,0,100);
    for(double Bearing : {0.,90.,180.,270.}) {
        const auto R=Scene->Rotation(FRotator(0,Bearing,0),P);
        const double Angle=FMath::DegreesToRadians(Bearing);
        const FVector Advance=Scene->Position(FMath::Sin(Angle),FMath::Cos(Angle),100)-P;
        TestTrue(TEXT("Bearing maps to the physical nose direction"),Advance.GetSafeNormal().Equals(R.Vector(),.00001));
    }
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArriettyTerrainSweepTest,"Arrietty.Terrain.Sweep",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArriettyTerrainSweepTest::RunTest(const FString&) {
    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto Scene=World->SpawnActor<AArriettyWorld>();
    auto Rock=World->SpawnActor<AActor>();
    auto Box=NewObject<UBoxComponent>(Rock);Rock->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10,100,100));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Rock->SetActorLocation(FVector(150,0,100));
    TestTrue(TEXT("Small obstacle between samples blocks motion"),Scene->PathBlocked(FVector::ZeroVector,FVector::ForwardVector,Scene));
    TestFalse(TEXT("Clear direction stays available"),Scene->PathBlocked(FVector::ZeroVector,-FVector::ForwardVector,Scene));
    TestFalse(TEXT("Flight above rock remains clear"),Scene->PathBlocked(FVector(0,0,1000),FVector::ForwardVector,Scene));
    Rock->SetActorLocation(FVector(600,0,100));
    TestFalse(TEXT("Distant rock outside original sweep"),Scene->PathBlocked(FVector::ZeroVector,FVector::ForwardVector,Scene));
    TestTrue(TEXT("Fast movement extends collision sweep"),Scene->PathBlocked(FVector::ZeroVector,FVector::ForwardVector,Scene,8.));
    Scene->Magnification=10;
    Scene->Track(0,0,FVector2D(100,0));
    TestTrue(TEXT("Magnified route queries terrain ahead"),Scene->QueryCenter().X>0);
    TestTrue(TEXT("Lookahead still covers current position and recovery"),Scene->QueryCenter().X<=60);
    World->DestroyWorld(false);return true;
}
#endif
