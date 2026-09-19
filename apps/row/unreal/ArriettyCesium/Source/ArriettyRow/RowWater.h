#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RowWaves.h"
#include "RowKelvin.h"
#include "RowBowWhitewater.h"
#include "RowWater.generated.h"
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class ARowShore;
UCLASS()
class ARRIETTYROW_API ARowWater:public AActor {
    GENERATED_BODY()
public:
    ARowWater();
    void UpdateBoat(FVector Position,float Heading,float Speed,float Drive,float DeltaSeconds);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Upload();
    void StartWaterline();
    void SyncWaterline();
    UPROPERTY() TObjectPtr<AActor> Waterline;
    UPROPERTY() TObjectPtr<ARowShore> ShoreManager;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WaterlineMaterial;
    bool WaterlineTickOrdered=false;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Patch;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> FarSurface;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> ShoreSurface;
    UPROPERTY() TObjectPtr<UTexture2D> Field;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LocalMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DistantMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShoreMaterial;
    row::Waves Waves;
    row::KelvinWake Kelvin;
    std::vector<float> Surface=std::vector<float>(row::Waves::N*row::Waves::N);
    double WaterTime=0;
    double Accumulator=0,KelvinAccumulator=0,UploadTime=0,WakeTime=0;
    double PreviousX=0,PreviousY=0,PreviousHeading=0;
    float PreviousSpeed=0;
    bool WasDriving=false,HaveBoat=false;
};
