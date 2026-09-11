#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RowPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRowSetupControlsTest,"ArriettyRow.Controls.SetupEnter",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRowSetupControlsTest::RunTest(const FString&) {
    const auto world=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    auto pawn=world?Cast<ARowPawn>(UGameplayStatics::GetPlayerPawn(world,0)):nullptr;
    if(!TestNotNull(TEXT("Row game pawn exists"),pawn)) return false;
    if(!TestTrue(TEXT("Test is restricted to an offline game"),pawn->Offline)) return false;
    pawn->Stop(); pawn->Toggle();
    TestTrue(TEXT("Enter begins setup"),pawn->Model.state==row::State::Calibrating);
    TestTrue(TEXT("Setup panel follows HMD before room neutral exists"),pawn->Instruments->GetAttachParent()==pawn->Camera);
    row::Input in=pawn->ReadInput(); const double began=in.now;
    for(int i=1;i<=220;++i) {
        in.now=began+i*.01; in.bar.received=in.head.received=in.now;
        pawn->Calibration.tick(in,.01);
    }
    const auto phase=pawn->Calibration.phase; const double remaining=pawn->Calibration.remaining;
    TestTrue(TEXT("Setup has progressed beyond the initial countdown"),phase==row::CalibrationPhase::Center);
    for(int i=0;i<5;++i) pawn->Toggle();
    TestTrue(TEXT("Repeated Enter cannot cancel setup"),pawn->Model.state==row::State::Calibrating);
    TestTrue(TEXT("Repeated Enter preserves phase and progress"),pawn->Calibration.phase==phase && pawn->Calibration.remaining==remaining);
    double motion=0;
    for(int i=221;i<=1200 && pawn->Calibration.phase!=row::CalibrationPhase::Complete;++i) {
        in.now=began+i*.01; in.bar.received=in.head.received=in.now;
        if(pawn->Calibration.phase==row::CalibrationPhase::Axis) {
            motion+=.01; in.bar.position.x=.38*std::cos(motion*2*row::Pi/2.8);
        }
        pawn->Calibration.tick(in,.01);
    }
    TestTrue(TEXT("Calibration still completes after repeated Enter"),pawn->Calibration.frame.valid);
    pawn->FinishCalibration(in);
    TestTrue(TEXT("Completed setup starts rowing"),pawn->Model.state==row::State::Running);
    TestTrue(TEXT("Running instruments return to boat"),pawn->Instruments->GetAttachParent()==pawn->RootComponent);
    pawn->Toggle();
    TestTrue(TEXT("Enter still pauses a running session"),pawn->Model.state==row::State::Paused);
    pawn->Toggle(); pawn->Stop();
    TestTrue(TEXT("Stop cancels setup and returns Ready"),pawn->Model.state==row::State::Ready && !pawn->Calibration.frame.valid);
    TestTrue(TEXT("Stop makes instructions visible again"),pawn->Instruments->GetAttachParent()==pawn->Camera);
    return true;
}
#endif
