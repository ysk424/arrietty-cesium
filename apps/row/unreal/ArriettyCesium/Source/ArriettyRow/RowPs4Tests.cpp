#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RowPawn.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRowPs4ControlsTest,"ArriettyRow.Controls.Ps4",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRowPs4ControlsTest::RunTest(const FString&) {
    const auto world=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    auto pawn=world?Cast<ARowPawn>(UGameplayStatics::GetPlayerPawn(world,0)):nullptr;
    if(!TestNotNull(TEXT("Pawn"),pawn) || !TestTrue(TEXT("Offline fixture only"),pawn->Offline))return false;
    pawn->Stop();pawn->UsePs4=true;pawn->Snapshot.ps4={};
    auto& c=pawn->Snapshot.ps4;c.received=row::Devices::seconds();c.valid=true;c.starts=1;
    pawn->Tick(.01f);
    TestTrue(TEXT("Square routes through normal setup"),pawn->Model.state==row::State::Calibrating);
    const double remaining=pawn->Calibration.remaining;pawn->Tick(.01f);
    TestTrue(TEXT("Held square does not restart countdown"),pawn->Calibration.remaining<remaining);
    ++c.starts;pawn->Tick(.01f);
    TestTrue(TEXT("Square during calibration keeps progress"),pawn->Calibration.remaining<remaining);
    ++c.stops;pawn->Tick(.01f);
    TestTrue(TEXT("Triangle aborts and returns home"),pawn->Model.state==row::State::Ready && !pawn->Calibration.frame.valid);
    ++c.starts;pawn->HandlePs4Controls(c.received,false);pawn->Tick(.01f);
    TestTrue(TEXT("Press during geography loading is consumed"),pawn->Model.state==row::State::Ready);
    ++c.starts;c.received-=1;pawn->Tick(.01f);c.received=row::Devices::seconds();pawn->Tick(.01f);
    TestTrue(TEXT("Stale queued start cannot replay"),pawn->Model.state==row::State::Ready);
    ++c.starts;++c.stops;pawn->Tick(.01f);
    TestTrue(TEXT("Triangle wins simultaneous square"),pawn->Model.state==row::State::Ready);
    pawn->UsePs4=false;pawn->Stop();return true;
}
#endif
