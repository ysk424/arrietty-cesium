#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RowPawn.h"
#include "RowAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRowAudioAssetsTest,"ArriettyRow.Audio.Assets",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRowAudioAssetsTest::RunTest(const FString&) {
    const auto world=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    auto pawn=world?Cast<ARowPawn>(UGameplayStatics::GetPlayerPawn(world,0)):nullptr;
    if(!TestNotNull(TEXT("Row pawn exists"),pawn)) return false;
    auto audio=pawn->FindComponentByClass<URowAudioComponent>();
    if(!TestNotNull(TEXT("Row audio component exists"),audio)) return false;
    if(!TestEqual(TEXT("Five audio voices"),audio->Tracks.Num(),int(row::AudioMix::Count))) return false;
    for(int i=0;i<audio->Tracks.Num();++i) {
        const auto track=audio->Tracks[i];
        auto wave=Cast<USoundWave>(track->Sound);
        if(!TestNotNull(FString::Printf(TEXT("Sound asset %d"),i),wave)) continue;
        TestEqual(TEXT("Two output ears"),wave->NumChannels,2);
        TestEqual(TEXT("Only catch is a one-shot"),bool(wave->bLooping),i!=row::AudioMix::Catch);
        TestTrue(TEXT("PCM avoids lossy stereo balance changes"),wave->GetSoundAssetCompressionType()==ESoundAssetCompressionType::PCM);
        TestTrue(TEXT("No first-stroke streaming delay"),wave->GetLoadingBehavior()==ESoundWaveLoadingBehavior::ForceInline);
        TestFalse(TEXT("Head yaw cannot pan the ear correction"),bool(track->bAllowSpatialization));
        TestFalse(TEXT("No unbounded overlapping catches"),bool(track->bCanPlayMultipleInstances));
    }
    return true;
}
#endif
