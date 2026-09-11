#include "RowAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "AudioMixerBlueprintLibrary.h"

void URowAudioComponent::Initialize(bool offline) {
    FParse::Value(FCommandLine::Get(),TEXT("RowVolume="),MasterVolume);
    MasterVolume=FMath::IsFinite(MasterVolume)?FMath::Clamp(MasterVolume,0.f,1.f):.8f;
    const TCHAR* names[]{TEXT("S_Catch"),TEXT("S_Pull"),TEXT("S_Boat"),TEXT("S_Water"),TEXT("S_Wind")};
    int loaded=0;
    for(int i=0;i<row::AudioMix::Count;++i) {
        auto audio=NewObject<UAudioComponent>(GetOwner());
        Tracks.Add(audio);
        audio->bAutoActivate=false; audio->bAutoDestroy=false; audio->bCanPlayMultipleInstances=false;
        audio->bAllowSpatialization=false; audio->bReverb=false;
        audio->SetupAttachment(GetOwner()->GetRootComponent());
        audio->SetVolumeMultiplier(0); audio->RegisterComponent();
        const FString path=FString::Printf(TEXT("/Game/Row/Audio/%s.%s"),names[i],names[i]);
        auto sound=LoadObject<USoundWave>(nullptr,*path);
        if(!sound) {
            UE_LOG(LogTemp,Warning,TEXT("ROW_AUDIO_MISSING asset=%s run=tools/prepare_audio.ps1"),names[i]);
            continue;
        }
        audio->SetSound(sound); ++loaded;
        if(i==row::AudioMix::Water || i==row::AudioMix::Wind) audio->Play();
    }
    // Explicit offline-only capture of the game's output; never microphone or hardware data.
    CapturePending=offline && FParse::Param(FCommandLine::Get(),TEXT("RowAudioCapture"));
    UE_LOG(LogTemp,Display,TEXT("ROW_AUDIO_READY assets=%d left_db=6 master=%.2f spatialized=0"),loaded,MasterVolume);
}

void URowAudioComponent::Update(const row::Model& model,float dt) {
    if(Tracks.Num()!=row::AudioMix::Count) return;
    Mix.tick(model,dt); Time+=FMath::Clamp(dt,0.f,.1f);
    if(CapturePending && Time>=1) {
        // Wait for rendering startup; record wall-clock audio duration even
        // when the engine clamps simulation time during a long frame.
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,30);
        CaptureEnd=FPlatformTime::Seconds()+30; CapturePending=false;
    }
    const bool running=model.state==row::State::Running;
    if(running && !ExercisePlaying) {
        Tracks[row::AudioMix::Pull]->Play(); Tracks[row::AudioMix::Boat]->Play();
    }
    if(!running && ExercisePlaying) {
        for(int i=0;i<=row::AudioMix::Boat;++i) Tracks[i]->FadeOut(.08f,0);
    }
    ExercisePlaying=running;
    for(int i=0;i<Tracks.Num();++i) {
        // Leave FadeOut's volume alone until the exercise voice has stopped.
        if(i<=row::AudioMix::Boat && !running) continue;
        Tracks[i]->SetVolumeMultiplier(float(Mix.gain[i])*MasterVolume);
    }
    Tracks[row::AudioMix::Boat]->SetPitchMultiplier(float(Mix.boatPitch));
    if(Mix.catchNow && Tracks[row::AudioMix::Catch]->Sound) {
        Tracks[row::AudioMix::Catch]->Play(); ++CatchCount;
    }
    if(CaptureEnd>0 && FPlatformTime::Seconds()>=CaptureEnd) {
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,
            TEXT("row-audio-demo"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("AudioCapture")));
        CaptureEnd=0;
        UE_LOG(LogTemp,Display,TEXT("ROW_AUDIO_CAPTURE_COMPLETE catches=%u"),CatchCount);
    }
}

void URowAudioComponent::EndPlay(const EEndPlayReason::Type reason) {
    for(auto audio:Tracks) if(audio) audio->Stop();
    Super::EndPlay(reason);
}
