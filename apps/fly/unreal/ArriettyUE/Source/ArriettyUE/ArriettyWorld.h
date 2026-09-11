#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "CesiumSampleHeightResult.h"
#include "ArriettyWorld.generated.h"

UCLASS()
class ARRIETTYUE_API AArriettyWorld : public AActor {
    GENERATED_BODY()
public:
    AArriettyWorld();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    static AArriettyWorld* Get(UWorld* World);
    bool IsReady() const { return Ready && !Failed; }
    bool HasOrigin() const { return OriginSet; }
    bool HasFailed() const { return Failed; }
    FString Message=TEXT("Loading Cesium terrain"), PlaceName, Timezone;
    double StartAltitude=0;
    FVector OriginLLH=FVector::ZeroVector;
    FVector Position(double East,double North,double Altitude) const;
    FRotator Rotation(const FRotator& BearingRotation,const FVector& Location) const;
    double Bearing(const FRotator& WorldRotation,const FVector& Location) const;
    void Track(double East,double North,const FVector2D& Velocity=FVector2D::ZeroVector);
    bool PathBlocked(const FVector& Location,const FVector& Direction,const AActor* Ignored,double ReachMeters=2.) const;
    TSharedRef<FJsonObject> Packet() const;
    void ApplySun(double Azimuth,double Elevation);
private:
    friend class FArriettyCesiumCoordinatesTest;
    friend class FArriettyHmdAlignmentTest;
    friend class FArriettyTerrainSweepTest;
    UPROPERTY() TObjectPtr<class ACesiumGeoreference> Geo;
    UPROPERTY() TObjectPtr<class ACesium3DTileset> Terrain;
    bool Ready=false,Failed=false,OriginSet=false,Pending=false,AirStart=false;
    double Age=0,Settled=0,LastQuery=0,StartAgl=100;
    double Magnification=1;
    int GridCount=13;
    FVector2D CurrentVelocity=FVector2D::ZeroVector;
    FVector2D Current=FVector2D::ZeroVector,GridCenter=FVector2D(1.e8,1.e8),PendingCenter;
    TArray<FVector2D> Candidates;
    TArray<FVector> Probes;
    TSharedPtr<FJsonObject> Grid;
    FVector LonLat(double East,double North) const;
    void Fail(const TCHAR* Code,const TCHAR* Text);
    void SampleGrid();
    FVector2D QueryCenter() const;
    void StartsSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>&,const TArray<FString>&);
    void GridSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>&,const TArray<FString>&);
};
