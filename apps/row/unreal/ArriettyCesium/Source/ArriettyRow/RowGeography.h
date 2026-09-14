#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RowGeography.generated.h"

class ACesium3DTileset;
class ACesiumGeoreference;
class UTexture2D;
class UMaterialInstanceDynamic;
struct FCesiumSampleHeightResult;

struct FRowWaterPolygon { TArray<TArray<FVector2D>> Rings; };

UCLASS()
class ARRIETTYROW_API ARowGeography:public AActor {
    GENERATED_BODY()
public:
    ARowGeography();
    static ARowGeography* Get(UWorld* World);
    bool Initialize(const FString& ScenePath);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    bool IsReady() const { return Ready; }
    bool HasFailed() const { return Failed; }
    const FString& GetMessage() const { return Message; }
    float GetSpawnYaw() const { return SpawnYaw; }
    double SurfaceHeightCm(double Xcm,double Ycm) const;
    bool CanNavigate(FVector Position) const;
    bool CanNavigatePath(FVector From,FVector To) const;
    void ConfigureWater(UMaterialInstanceDynamic* Material) const;
    double GetRenderRadiusCm() const { return Ocean?3000000.:FMath::Max(FMath::Max(FMath::Abs(Bounds.X),FMath::Abs(Bounds.Y)),FMath::Max(FMath::Abs(Bounds.X+Bounds.Z),FMath::Abs(Bounds.Y+Bounds.W)))+5000.; }
    FString PlaceLabel;
private:
    friend class FRowMovementTest;
    void Fail(const TCHAR* Code,const TCHAR* Text);
    void HeightsSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>&,const TArray<FString>&);
    UPROPERTY() TObjectPtr<ACesium3DTileset> Terrain;
    UPROPERTY() TObjectPtr<ACesiumGeoreference> Georeference;
    UPROPERTY() TObjectPtr<UTexture2D> Mask;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TerrainMaterial;
    TArray<FRowWaterPolygon> Polygons;
    TArray<FVector> Probes;
    double Coeff[5]={0,0,0,0,0};
    FVector4 Bounds=FVector4(-300000,-300000,600000,600000);
    float SpawnYaw=0;
    double RadiusM=3000,Age=0,Settled=0;
    bool Ocean=false,Ready=false,Failed=false,HeightChecked=false;
    FString Message=TEXT("Loading terrain / please wait");
};
