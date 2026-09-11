#include "ArriettyWorld.h"
#include "ArriettyCesium.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

namespace {
constexpr double FlightGridStep=10.;
const FVector2D Checks[]={{0,0},{50,0},{-50,0},{0,50},{0,-50},{25,25},{25,-25},{-25,25},{-25,-25}};
TArray<TSharedPtr<FJsonValue>> Values(const FVector& P) {
    return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};
}
bool ReadObject(const FString& Path,TSharedPtr<FJsonObject>& Result) {
    FString Text;
    return FFileHelper::LoadFileToString(Text,*Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result) && Result.IsValid();
}
}
AArriettyWorld::AArriettyWorld() { PrimaryActorTick.bCanEverTick=true; }
AArriettyWorld* AArriettyWorld::Get(UWorld* World) {
    if(World) for(TActorIterator<AArriettyWorld> It(World);It;++It) return *It;
    return nullptr;
}
void AArriettyWorld::Fail(const TCHAR* Code,const TCHAR* Text) {
    Failed=true;Ready=false;Message=Text;
    UE_LOG(LogTemp,Error,TEXT("FLY_GEOGRAPHY_FAILED code=%s"),Code);
}
void AArriettyWorld::BeginPlay() {
    Super::BeginPlay();
    TSharedPtr<FJsonObject> Scene,Config;
    if(!ReadObject(FPlatformMisc::GetEnvironmentVariable(TEXT("ARRIETTY_UE_SOLAR")),Scene) ||
       !ReadObject(FPlatformMisc::GetEnvironmentVariable(TEXT("ARRIETTY_CESIUM_CONFIG")),Config)) {
        Fail(TEXT("config"),TEXT("Launch with fly.ps1"));return;
    }
    double Schema=0,Lon=0,Lat=0;
    if(!Scene->TryGetNumberField(TEXT("schema_version"),Schema) || Schema!=2 ||
       !Scene->TryGetNumberField(TEXT("origin_longitude"),Lon) || !Scene->TryGetNumberField(TEXT("origin_latitude"),Lat) ||
       !FMath::IsFinite(Lon) || !FMath::IsFinite(Lat) || FMath::Abs(Lon)>180 || FMath::Abs(Lat)>85) {
        Fail(TEXT("scene"),TEXT("Invalid flight scene"));return;
    }
    PlaceName=Scene->GetStringField(TEXT("name"));Timezone=Scene->GetStringField(TEXT("timezone"));
    AirStart=Scene->GetStringField(TEXT("start_mode"))==TEXT("air");
    StartAgl=Scene->GetNumberField(TEXT("start_agl_m"));
    if(!FMath::IsFinite(StartAgl) || StartAgl<10 || StartAgl>3000) { Fail(TEXT("agl"),TEXT("Invalid start altitude"));return; }
    Scene->TryGetNumberField(TEXT("movement_magnification"),Magnification);
    if(!FMath::IsFinite(Magnification) || Magnification<1 || Magnification>10) { Fail(TEXT("magnification"),TEXT("Invalid movement magnification"));return; }
    GridCount=Magnification>1?25:13;
    FString Token=FPlatformMisc::GetEnvironmentVariable(TEXT("CESIUM_ION_TOKEN"));
    if(Token.IsEmpty()) Config->TryGetStringField(TEXT("ion_access_token"),Token);
    double TerrainId=1,ImageryId=2;
    Config->TryGetNumberField(TEXT("terrain_asset_id"),TerrainId);Config->TryGetNumberField(TEXT("imagery_asset_id"),ImageryId);
    if(Token.IsEmpty() || !FMath::IsFinite(TerrainId) || !FMath::IsFinite(ImageryId) || TerrainId<1 || ImageryId<1) {
        Fail(TEXT("cesium"),TEXT("Cesium settings unavailable"));return;
    }
    Geo=GetWorld()->SpawnActor<ACesiumGeoreference>();Geo->SetOriginLongitudeLatitudeHeight(FVector(Lon,Lat,0));
    Terrain=ArriettyCesium::CreateTerrain(GetWorld(),Geo,TerrainId,Token);
    Terrain->EnableFrustumCulling=false;
    ArriettyCesium::FinishTerrain(Terrain,ImageryId,Token);
    ApplySun(Scene->GetNumberField(TEXT("sun_azimuth")),Scene->GetNumberField(TEXT("sun_elevation")));
    if(AirStart) Candidates.Add(FVector2D::ZeroVector);
    else {
        const TArray<TSharedPtr<FJsonValue>>* List=nullptr;
        if(!Scene->TryGetArrayField(TEXT("launch_candidates"),List) || List->Num()>100) { Fail(TEXT("candidates"),TEXT("Invalid launch points"));return; }
        for(const auto& Entry:*List) {
            const TArray<TSharedPtr<FJsonValue>>* Pair=nullptr;
            if(!Entry->TryGetArray(Pair) || Pair->Num()!=2) continue;
            double E=0,N=0;
            if((*Pair)[0]->TryGetNumber(E) && (*Pair)[1]->TryGetNumber(N) && FMath::IsFinite(E) && FMath::IsFinite(N) && FVector2D(E,N).Size()<=1100)
                Candidates.Add(FVector2D(E,N));
        }
    }
    if(Candidates.IsEmpty()) { Fail(TEXT("no_land"),TEXT("No land launch point; use -StartMode Air"));return; }
    for(const auto& Candidate:Candidates) for(const auto& Check:Checks) Probes.Add(LonLat(Candidate.X+Check.X,Candidate.Y+Check.Y));
    Terrain->SampleHeightMostDetailed(Probes,FCesiumSampleHeightMostDetailedCallback::CreateUObject(this,&AArriettyWorld::StartsSampled));
    UE_LOG(LogTemp,Display,TEXT("FLY_GEOGRAPHY_CREATED start=%s candidates=%d"),AirStart?TEXT("air"):TEXT("ground"),Candidates.Num());
}
FVector AArriettyWorld::LonLat(double E,double N) const {
    return Geo->TransformUnrealPositionToLongitudeLatitudeHeight(FVector(E*100,-N*100,0));
}
FVector AArriettyWorld::Position(double E,double N,double H) const {
    if(!Geo || !OriginSet) return FVector(N*100,E*100,H*100);
    auto P=LonLat(E,N);P.Z=OriginLLH.Z+H;
    return Geo->TransformLongitudeLatitudeHeightPositionToUnreal(P);
}
FRotator AArriettyWorld::Rotation(const FRotator& R,const FVector& Location) const {
    if(!Geo) return R;
    return Geo->TransformEastSouthUpRotatorToUnreal(FRotator(R.Pitch,ArriettyCesium::BearingToEsuYaw(R.Yaw),R.Roll),Location);
}
double AArriettyWorld::Bearing(const FRotator& R,const FVector& Location) const {
    return Geo?ArriettyCesium::EsuYawToBearing(Geo->TransformUnrealRotatorToEastSouthUp(R,Location).Yaw):R.Yaw;
}
void AArriettyWorld::StartsSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>& Results,const TArray<FString>&) {
    if(Failed) return;
    if(Results.Num()!=Probes.Num()) { Fail(TEXT("heights"),TEXT("Launch terrain unavailable"));return; }
    int Selected=INDEX_NONE;
    for(int C=0;C<Candidates.Num();++C) {
        const int Base=C*UE_ARRAY_COUNT(Checks);bool Good=true;
        if(!Results[Base].SampleSuccess || !FMath::IsFinite(Results[Base].LongitudeLatitudeHeight.Z)) continue;
        const double H=Results[Base].LongitudeLatitudeHeight.Z;
        for(int J=0;J<UE_ARRAY_COUNT(Checks);++J) {
            const auto& Sample=Results[Base+J];Good&=Sample.SampleSuccess && FMath::IsFinite(Sample.LongitudeLatitudeHeight.Z);
            if(!AirStart && J>0) Good&=FMath::Abs(Sample.LongitudeLatitudeHeight.Z-H)<=Checks[J].Size()*.06;
        }
        if(Good) { Selected=Base;break; }
    }
    if(Selected==INDEX_NONE) { Fail(TEXT("no_flat_launch"),TEXT("No verified flat launch nearby; use -StartMode Air"));return; }
    OriginLLH=Results[Selected].LongitudeLatitudeHeight;
    Geo->SetOriginLongitudeLatitudeHeight(OriginLLH);OriginSet=true;StartAltitude=AirStart?StartAgl:0;
    UE_LOG(LogTemp,Display,TEXT("FLY_ORIGIN lon=%.8f lat=%.8f ellipsoid_m=%.3f start_agl_m=%.1f"),OriginLLH.X,OriginLLH.Y,OriginLLH.Z,StartAltitude);
    SampleGrid();
}
void AArriettyWorld::Track(double E,double N,const FVector2D& Velocity) {
    if(FMath::IsFinite(E) && FMath::IsFinite(N)) Current=FVector2D(E,N);
    if(!Velocity.ContainsNaN()) CurrentVelocity=Velocity;
}
FVector2D AArriettyWorld::QueryCenter() const {
    // Keep current and recovering poses covered while looking into the route.
    const double Lead=Magnification>1?FMath::Min(CurrentVelocity.Size()*.5,50.):0.;
    const FVector2D Target=Current+CurrentVelocity.GetSafeNormal()*Lead;
    return FVector2D(FMath::RoundToDouble(Target.X/20)*20,FMath::RoundToDouble(Target.Y/20)*20);
}
bool AArriettyWorld::PathBlocked(const FVector& Location,const FVector& Direction,const AActor* Ignored,double ReachMeters) const {
    // Complement the geographic height field with loaded mesh collision so a
    // narrow rock/ridge between height samples cannot pass through the rider.
    const FVector Up=Rotation(FRotator::ZeroRotator,Location).RotateVector(FVector::UpVector);
    const FVector Start=Location+Up*100.;
    const FVector End=Start+Direction.GetSafeNormal()*FMath::Clamp(ReachMeters,2.,100.)*100.;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ArriettyFlightPath),true,Ignored);
    FHitResult Hit;
    return GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(30.f),Params);
}
void AArriettyWorld::SampleGrid() {
    if(Pending || Failed || !OriginSet) return;
    Pending=true;LastQuery=Age;
    PendingCenter=QueryCenter();
    TArray<FVector> Points;
    for(int Y=0;Y<GridCount;++Y) for(int X=0;X<GridCount;++X)
        Points.Add(LonLat(PendingCenter.X+(X-GridCount/2)*FlightGridStep,PendingCenter.Y+(Y-GridCount/2)*FlightGridStep));
    Terrain->SampleHeightMostDetailed(Points,FCesiumSampleHeightMostDetailedCallback::CreateUObject(this,&AArriettyWorld::GridSampled));
}
void AArriettyWorld::GridSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>& Results,const TArray<FString>&) {
    Pending=false;if(Failed) return;
    bool Good=Results.Num()==GridCount*GridCount;
    for(const auto& R:Results) Good&=R.SampleSuccess && FMath::IsFinite(R.LongitudeLatitudeHeight.Z);
    if(!Good) { Message=TEXT("Loading terrain ahead");return; }
    GridCenter=PendingCenter;Grid=MakeShared<FJsonObject>();
    Grid->SetNumberField(TEXT("east"),GridCenter.X-(GridCount/2)*FlightGridStep);Grid->SetNumberField(TEXT("north"),GridCenter.Y-(GridCount/2)*FlightGridStep);
    Grid->SetNumberField(TEXT("step"),FlightGridStep);Grid->SetNumberField(TEXT("count"),GridCount);
    TArray<TSharedPtr<FJsonValue>> Heights;
    for(const auto& R:Results) Heights.Add(MakeShared<FJsonValueNumber>(R.LongitudeLatitudeHeight.Z-OriginLLH.Z));
    Grid->SetArrayField(TEXT("heights"),Heights);
    UE_LOG(LogTemp,Display,TEXT("FLY_TERRAIN_PATCH east=%.0f north=%.0f samples=%d"),GridCenter.X,GridCenter.Y,Results.Num());
}
void AArriettyWorld::Tick(float Delta) {
    Super::Tick(Delta);Age+=Delta;if(Failed) return;
    if(!Ready) {
        if(Grid.IsValid() && Terrain->GetLoadProgress()>=99) Settled+=Delta;else Settled=0;
        if(Settled>1) { Ready=true;Message.Empty();UE_LOG(LogTemp,Display,TEXT("FLY_GEOGRAPHY_READY height_verified=1 terrain_loaded=1")); }
        if(Age>180) Fail(TEXT("timeout"),TEXT("Terrain loading timed out; restart when online"));
    }
    if(OriginSet && !Pending && Age-LastQuery>(Magnification>1?.2:1.) && (!Grid || (QueryCenter()-GridCenter).GetAbsMax()>=20)) SampleGrid();
}
TSharedRef<FJsonObject> AArriettyWorld::Packet() const {
    auto P=MakeShared<FJsonObject>();P->SetBoolField(TEXT("ready"),IsReady());P->SetStringField(TEXT("message"),Message);
    if(IsReady()) { P->SetArrayField(TEXT("origin"),Values(OriginLLH));P->SetObjectField(TEXT("patch"),Grid); }
    return P;
}
void AArriettyWorld::ApplySun(double Azimuth,double Elevation) {
    for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It) {
        It->SetActorRotation(Rotation(FRotator(-Elevation,Azimuth+180,0),FVector::ZeroVector));
        auto Light=Cast<UDirectionalLightComponent>(It->GetLightComponent());
        Light->SetAtmosphereSunLight(true);Light->SetLightSourceAngle(.533);Light->SetIntensity(50000*FMath::Clamp((Elevation+.3)/8.,0.,1.));
    }
    // Preserve the former evening exposure, while protecting satellite imagery
    // from clipping under the much brighter midday sun at arbitrary locations.
    for(TActorIterator<APostProcessVolume> It(GetWorld());It;++It)
        It->Settings.DepthOfFieldFstop=FMath::Lerp(4.f,11.f,float(FMath::Clamp((Elevation-3.)/20.,0.,1.)));
}
