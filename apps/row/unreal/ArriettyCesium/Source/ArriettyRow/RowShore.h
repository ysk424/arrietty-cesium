#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RowShore.generated.h"
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class ARowGeography;

// One shared, low-frequency terrain capture feeds the purchased surf material.
// No hardware, physics, height sampling requests or per-eye simulation.
UCLASS()
class ARRIETTYROW_API ARowShore:public AActor {
    GENERATED_BODY()
public:
    ARowShore();
    bool Initialize(ARowGeography* Geo,UMaterialInstanceDynamic* Water);
    void Update(FVector Boat,float Dt);
private:
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Depth;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Seeds;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Scratch;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Field;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SeedMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> JumpMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FieldMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;
    UPROPERTY() TObjectPtr<ARowGeography> Geography;
    float Age=1;
    bool DiagnosticDone=false;
};
