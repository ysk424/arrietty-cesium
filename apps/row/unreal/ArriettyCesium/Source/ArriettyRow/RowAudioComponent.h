#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RowAudio.h"
#include "RowAudioComponent.generated.h"
class UAudioComponent;

UCLASS(Config=Game)
class ARRIETTYROW_API URowAudioComponent:public UActorComponent {
    GENERATED_BODY()
public:
    void Initialize(bool Offline);
    void Update(const row::Model& Model,float DeltaSeconds);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(Config,EditAnywhere,Category="Audio",meta=(ClampMin="0",ClampMax="1"))
    float MasterVolume=.8f;
private:
    friend class FRowAudioAssetsTest;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Tracks;
    row::AudioMix Mix;
    bool ExercisePlaying=false,CapturePending=false;
    double Time=0,CaptureEnd=0;
    unsigned CatchCount=0;
};
