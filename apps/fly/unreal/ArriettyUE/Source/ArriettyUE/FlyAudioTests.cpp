#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "FlyAudioComponent.h"
#include "ArriettyPawn.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlyAudioMixTest,"Arrietty.Audio.Mix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FFlyAudioMixTest::RunTest(const FString&)
{
    FFlyAudioMix Mix; FFlyAudioInput In;
    auto Settle=[&](int N=200) { for(int I=0;I<N;++I) Mix.Tick(In,.02f); };
    In.Active=true; In.Speed=24; In.Cadence=70; In.Power=200; Settle();
    TestTrue(TEXT("Wheels move on ground"),Mix.Gain[FFlyAudioMix::Ground]>.1f);
    TestTrue(TEXT("Pedalling drives propeller"),Mix.Gain[FFlyAudioMix::Propeller]>.1f);
    In.Airborne=true; Settle();
    TestTrue(TEXT("No wheel roll aloft"),Mix.Gain[FFlyAudioMix::Ground]<.0001f);
    In.Cadence=0; In.Power=0; Settle();
    TestTrue(TEXT("Wind continues in unpowered glide"),Mix.Gain[FFlyAudioMix::WindSoft]>.1f);
    TestTrue(TEXT("Pedal and propeller decay to silence"),Mix.Gain[FFlyAudioMix::Pedal]+Mix.Gain[FFlyAudioMix::Propeller]<.0001f);
    const float Slow=Mix.Gain[FFlyAudioMix::WindFast];
    In.Speed=55; Settle();
    TestTrue(TEXT("Fast wind rises with airspeed"),Mix.Gain[FFlyAudioMix::WindFast]>Slow+.1f);
    In.Airborne=false; In.Touchdowns=1; Mix.Tick(In,.02f);
    TestTrue(TEXT("Landing triggers one shot"),Mix.TouchdownNow);
    Mix.Tick(In,.02f); TestFalse(TEXT("Repeated packet cannot repeat touchdown"),Mix.TouchdownNow);
    In.Active=false; In.Touchdowns=2; Mix.Tick(In,.02f);
    TestFalse(TEXT("Blocked or paused input cannot play touchdown"),Mix.TouchdownNow);
    Settle(30);
    for(float Gain:Mix.Gain) TestTrue(TEXT("Stop and tracking loss fade all voices"),Gain<.0001f);
    In.Active=true; Mix.Tick(In,.02f);
    TestFalse(TEXT("Resume does not replay old touchdown"),Mix.TouchdownNow);
    In.Speed=1.e9f; In.Cadence=1.e9f; In.Power=1.e9f; Settle();
    float Sum=0;
    for(int I=0;I<FFlyAudioMix::Count;++I) {
        Sum+=Mix.Gain[I];
        TestTrue(TEXT("Pitch remains bounded"),Mix.Pitch[I]>=.69f && Mix.Pitch[I]<=1.41f);
    }
    TestTrue(TEXT("Six normalized voices retain peak headroom at volume 1"),Sum*.5f<.95f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlyAudioAssetsTest,"Arrietty.Audio.Assets",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FFlyAudioAssetsTest::RunTest(const FString&)
{
    auto World=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    auto Pawn=World?Cast<AArriettyPawn>(UGameplayStatics::GetPlayerPawn(World,0)):nullptr;
    if(!TestNotNull(TEXT("Fly fixture pawn"),Pawn)) return false;
    auto Audio=Pawn->FindComponentByClass<UFlyAudioComponent>();
    if(!TestNotNull(TEXT("Fly audio component"),Audio)) return false;
    if(!TestEqual(TEXT("Six voices including pedal propeller"),Audio->Tracks.Num(),int32(FFlyAudioMix::Count))) return false;
    for(int I=0;I<Audio->Tracks.Num();++I) {
        auto Track=Audio->Tracks[I]; auto Wave=Cast<USoundWave>(Track->Sound);
        if(!TestNotNull(FString::Printf(TEXT("Sound %d"),I),Wave)) continue;
        TestEqual(TEXT("Wind stereo, mechanical sounds mono"),Wave->NumChannels,I<2?2:1);
        TestEqual(TEXT("Only touchdown is a one-shot"),bool(Wave->bLooping),I!=FFlyAudioMix::Touchdown);
        TestTrue(TEXT("PCM import"),Wave->GetSoundAssetCompressionType()==ESoundAssetCompressionType::PCM);
        TestTrue(TEXT("No first-pedal streaming wait"),Wave->GetLoadingBehavior()==ESoundWaveLoadingBehavior::ForceInline);
        TestFalse(TEXT("Cockpit sound does not turn with head yaw"),bool(Track->bAllowSpatialization));
        TestFalse(TEXT("No overlapping copies"),bool(Track->bCanPlayMultipleInstances));
    }
    return true;
}
#endif
