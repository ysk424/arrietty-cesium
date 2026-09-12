#include "FlyAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UFlyAudioComponent::Initialize()
{
    if(Tracks.Num()) return;
    FParse::Value(FCommandLine::Get(),TEXT("FlyVolume="),MasterVolume);
    MasterVolume=FMath::IsFinite(MasterVolume)?FMath::Clamp(MasterVolume,0.f,1.f):.8f;
    const TCHAR* Names[]{TEXT("S_WindSoft"),TEXT("S_WindFast"),TEXT("S_PedalDrive"),TEXT("S_Propeller"),TEXT("S_GroundRoll"),TEXT("S_Touchdown")};
    int32 Loaded=0;
    for(int32 I=0;I<FFlyAudioMix::Count;++I)
    {
        auto Track=NewObject<UAudioComponent>(GetOwner()); Tracks.Add(Track);
        Track->bAutoActivate=false; Track->bAutoDestroy=false; Track->bCanPlayMultipleInstances=false;
        Track->bAllowSpatialization=false; Track->bReverb=false;
        Track->SetupAttachment(GetOwner()->GetRootComponent());
        Track->SetVolumeMultiplier(0); Track->RegisterComponent();
        const FString Path=FString::Printf(TEXT("/Game/Fly/Audio/%s.%s"),Names[I],Names[I]);
        auto Sound=LoadObject<USoundWave>(nullptr,*Path);
        if(!Sound) { UE_LOG(LogTemp,Warning,TEXT("FLY_AUDIO_MISSING asset=%s run=apps/fly/tools/prepare_audio.ps1"),Names[I]); continue; }
        Track->SetSound(Sound); ++Loaded;
        if(I<FFlyAudioMix::Touchdown) Track->Play();
    }
    UE_LOG(LogTemp,Display,TEXT("FLY_AUDIO_READY assets=%d master=%.2f spatialized=0"),Loaded,MasterVolume);
}

void UFlyAudioComponent::Update(const FFlyAudioInput& Input,float Dt)
{
    if(Tracks.Num()!=FFlyAudioMix::Count) return;
    Mix.Tick(Input,Dt);
    for(int32 I=0;I<FFlyAudioMix::Count;++I)
    {
        // Keep the one-shot's volume intact while its short stop fade runs.
        if(I==FFlyAudioMix::Touchdown && !Input.Active) continue;
        Tracks[I]->SetVolumeMultiplier(Mix.Gain[I]*MasterVolume);
        Tracks[I]->SetPitchMultiplier(Mix.Pitch[I]);
    }
    if(!Input.Active && Active) Tracks[FFlyAudioMix::Touchdown]->FadeOut(.08f,0);
    Active=Input.Active;
    if(Mix.TouchdownNow && Tracks[FFlyAudioMix::Touchdown]->Sound)
    {
        Tracks[FFlyAudioMix::Touchdown]->Play(); ++TouchdownCount;
        UE_LOG(LogTemp,Display,TEXT("FLY_AUDIO_TOUCHDOWN count=%d"),TouchdownCount);
    }
}

void UFlyAudioComponent::TickFixture(float Dt)
{
    // Called only by an explicit no-HMD fixture pawn on the empty Engine map.
    // Exercises game output, never microphone input, devices or geography.
    const double Now=FPlatformTime::Seconds();
    if(!FixtureBegan) FixtureBegan=Now;
    if(!Capturing && !Finished && Now-FixtureBegan>3)
    {
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,30);
        Capturing=true; CaptureBegan=Now;
    }
    FFlyAudioInput Input;
    const double Age=Capturing?Now-CaptureBegan:-1;
    if(Age>=2 && Age<26)
    {
        Input.Active=true; Input.Speed=18; Input.Cadence=70; Input.Power=200;
        if(Age>=7) { Input.Airborne=true; Input.Speed=24; }
        if(Age>=12) { Input.Cadence=0; Input.Power=0; }
        if(Age>=18) Input.Speed=55;
        if(Age>=23) { Input.Airborne=false; Input.Speed=18; Input.Touchdowns=1; }
    }
    if(Age>=26) Input.Touchdowns=1;
    Update(Input,Dt);
    if(Capturing && Age>=30)
    {
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,
            TEXT("fly-audio-fixture"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("AudioCapture")));
        Capturing=false; Finished=true;
        UE_LOG(LogTemp,Display,TEXT("FLY_AUDIO_CAPTURE_COMPLETE touchdowns=%d"),TouchdownCount);
    }
    if(Finished && Now-CaptureBegan>33) UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
}

void UFlyAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    for(auto Track:Tracks) if(Track) Track->Stop();
    Super::EndPlay(Reason);
}
