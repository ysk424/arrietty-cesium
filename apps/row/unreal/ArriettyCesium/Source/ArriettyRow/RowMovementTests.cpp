#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RowPawn.h"
#include "RowGeography.h"
#include "RowPanel.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRowMovementTest,"ArriettyRow.Movement.Magnification",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRowMovementTest::RunTest(const FString&) {
    const auto world=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    auto pawn=world?Cast<ARowPawn>(UGameplayStatics::GetPlayerPawn(world,0)):nullptr;
    if(!TestNotNull(TEXT("Row game pawn exists"),pawn)) return false;
    if(!TestTrue(TEXT("Only the offline fixture may run this test"),pawn->Offline && !pawn->Geography)) return false;
    const double configured=pawn->MovementMagnification;
    const auto start=[&](double mag,double speed) {
        pawn->Stop(); pawn->MovementMagnification=mag;
        const auto in=pawn->ReadInput();
        pawn->Model.start(in,{{1,0,0},in.head.position,true});
        pawn->Model.heading=0; pawn->Model.speed=speed;
    };
    double baseDistance=0,baseSpeed=0;
    for(double mag:{1.,2.5,10.}) {
        start(mag,2); const FVector origin=pawn->GetActorLocation();
        pawn->Tick(.05f);
        if(mag==1) { baseDistance=pawn->Model.distance; baseSpeed=pawn->Model.speed; }
        TestTrue(TEXT("Actual pawn displacement and distance scale together"),
            FMath::IsNearlyEqual((pawn->GetActorLocation()-origin).Size()*.01,baseDistance*mag,1e-6) &&
            FMath::IsNearlyEqual(pawn->Model.distance,baseDistance*mag,1e-6));
        TestEqual(TEXT("Physical speed is unchanged"),pawn->Model.speed,baseSpeed);
        TestEqual(TEXT("Panel shows magnified world speed"),pawn->Panel->Speed,FString::Printf(TEXT("%.1f"),baseSpeed*3.6*mag));
        pawn->Toggle(); const FVector paused=pawn->GetActorLocation(); pawn->Tick(.05f);
        TestEqual(TEXT("Paused pawn stays still"),pawn->GetActorLocation(),paused);
        pawn->Stop();
        TestEqual(TEXT("Stop returns home without clearing the launch multiplier"),pawn->GetActorLocation(),pawn->Home.GetLocation());
        TestEqual(TEXT("Multiplier survives stop"),pawn->MovementMagnification,mag);
    }
    auto geo=world->SpawnActor<ARowGeography>();
    geo->Ready=true; geo->RadiusM=100;
    FRowWaterPolygon water;
    water.Rings.Add({FVector2D(-50,-50),FVector2D(50,-50),FVector2D(50,50),FVector2D(-50,50)});
    // A 10 cm island is narrower than a sampled path; exact crossings must stop.
    water.Rings.Add({FVector2D(2,-1),FVector2D(2.1,-1),FVector2D(2.1,1),FVector2D(2,1)});
    geo->Polygons.Add(water);
    TestTrue(TEXT("Path endpoints can both be mapped water"),geo->CanNavigate(FVector::ZeroVector) && geo->CanNavigate(FVector(550,0,0)));
    TestFalse(TEXT("Whole path catches a narrow island between endpoints"),geo->CanNavigatePath(FVector::ZeroVector,FVector(550,0,0)));
    TestTrue(TEXT("Clear water path remains navigable"),geo->CanNavigatePath(FVector::ZeroVector,FVector(0,550,0)));
    TestFalse(TEXT("Bank crossing is blocked"),geo->CanNavigatePath(FVector::ZeroVector,FVector(6000,0,0)));
    geo->RadiusM=3;
    TestFalse(TEXT("Radius remains unscaled"),geo->CanNavigatePath(FVector::ZeroVector,FVector(0,550,0)));
    geo->RadiusM=100;
    pawn->Geography=geo; start(10,5.5); pawn->Tick(.09f);
    TestTrue(TEXT("Blocked magnified step pauses without distance or position credit"),
        pawn->Model.state==row::State::Paused && pawn->Model.distance==0 && pawn->GetActorLocation().Equals(pawn->Home.GetLocation()));
    geo->Polygons[0].Rings.SetNum(1); geo->Coeff[2]=-.001;
    start(10,2); pawn->Tick(.05f);
    const auto location=pawn->GetActorLocation();
    TestTrue(TEXT("Travel follows curved mean water without magnifying its height"),
        location.X>0 && FMath::IsNearlyEqual(location.Z,geo->SurfaceHeightCm(location.X,location.Y),1e-8));
    // Exercise the real CSV formatter with rejected telemetry, retaining raw
    // observations while applied gain stays at unity. This is local test output.
    pawn->Offline=false; pawn->Snapshot.telemetry.resistance.set(17,row::Devices::seconds());
    pawn->SessionFile=FPaths::ProjectSavedDir()/TEXT("row-magnification-test.csv");
    FFileHelper::SaveStringToFile(TEXT(""),*pawn->SessionFile);
    pawn->Record(TEXT("fixture")); FString record;
    FFileHelper::LoadFileToString(record,*pawn->SessionFile);
    TArray<FString> columns; record.TrimStartAndEnd().ParseIntoArray(columns,TEXT(","),false);
    TestEqual(TEXT("CSV appends four fields without changing existing positions"),columns.Num(),25);
    if(columns.Num()==25) {
        TestTrue(TEXT("Invalid load stays unknown while raw observation is recorded"),columns[15].IsEmpty() && columns[16]==TEXT("1") && columns[23]==TEXT("17"));
        TestEqual(TEXT("CSV records launch multiplier"),columns[21],FString(TEXT("10")));
        TestTrue(TEXT("CSV world speed matches magnified physical speed"),FMath::IsNearlyEqual(FCString::Atod(*columns[22]),pawn->Model.speed*36,.001));
    }
    IFileManager::Get().Delete(*pawn->SessionFile); pawn->SessionFile.Empty(); pawn->Offline=true;
    pawn->Geography=nullptr; geo->Destroy(); pawn->Stop(); pawn->MovementMagnification=configured;
    return true;
}
#endif
