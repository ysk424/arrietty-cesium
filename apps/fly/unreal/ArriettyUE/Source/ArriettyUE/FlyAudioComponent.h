#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlyAudioMix.h"
#include "FlyAudioComponent.generated.h"
class UAudioComponent;
UCLASS()
class ARRIETTYUE_API UFlyAudioComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void Initialize();
    void Update(const FFlyAudioInput& Input,float Dt);
    void TickFixture(float Dt);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Tracks;
    FFlyAudioMix Mix;
    int32 TouchdownCount=0;
private:
    float MasterVolume=.8f;
    bool Active=false, Capturing=false, Finished=false;
    double FixtureBegan=0, CaptureBegan=0;
};
