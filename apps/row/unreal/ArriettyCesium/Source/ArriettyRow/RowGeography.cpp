#include "RowGeography.h"
#include "RowTerrain.h"
#include "ArriettyCesium.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumIonRasterOverlay.h"
#include "CesiumSampleHeightResult.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonSerializer.h"

ARowGeography::ARowGeography() { PrimaryActorTick.bCanEverTick=true; }
AActor* ARowGeography::GetTerrainActor() const { return Terrain; }
ARowGeography* ARowGeography::Get(UWorld* World) {
    if(World) for(TActorIterator<ARowGeography> it(World);it;++it) return *it;
    return nullptr;
}
void ARowGeography::Fail(const TCHAR* Code,const TCHAR* Text) {
    Failed=true; Ready=false; Message=Text;
    UE_LOG(LogTemp,Error,TEXT("ROW_GEOGRAPHY_FAILED code=%s"),Code);
}
bool ARowGeography::Initialize(const FString& ScenePath) {
    FString raw; TSharedPtr<FJsonObject> data;
    if(!FFileHelper::LoadFileToString(raw,*ScenePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(raw),data) || !data.IsValid()) {
        Fail(TEXT("scene_missing"),TEXT("Place data missing / launch with row.ps1")); return false;
    }
    auto number=[&](const TCHAR* key,double low,double high,double& out) {
        return data->TryGetNumberField(key,out) && FMath::IsFinite(out) && out>=low && out<=high;
    };
    double schema,lon,lat,height,yaw,maskSize;
    FString type,maskPath,country;
    const TArray<TSharedPtr<FJsonValue>> *coeff=nullptr,*bounds=nullptr,*polys=nullptr,*probes=nullptr;
    if(!number(TEXT("schema_version"),1,1,schema) || !number(TEXT("origin_longitude"),-180,180,lon) ||
       !number(TEXT("origin_latitude"),-85,85,lat) || !number(TEXT("water_height_ellipsoid_m"),-600,6700,height) ||
       !number(TEXT("spawn_yaw_deg"),-180,180,yaw) || !number(TEXT("navigation_radius_m"),1000,10000,RadiusM) ||
       !number(TEXT("mask_size"),2048,2048,maskSize) || !data->TryGetStringField(TEXT("water_type"),type) ||
       (type!=TEXT("sea") && type!=TEXT("lake")) || !data->TryGetStringField(TEXT("water_mask_path"),maskPath) ||
       !data->TryGetArrayField(TEXT("surface_coefficients"),coeff) || coeff->Num()!=5 ||
       !data->TryGetArrayField(TEXT("mask_bounds_m"),bounds) || bounds->Num()!=4 ||
       !data->TryGetArrayField(TEXT("water_polygons_m"),polys) || polys->IsEmpty() ||
       !data->TryGetArrayField(TEXT("height_probes"),probes) || probes->Num()!=5) {
        Fail(TEXT("scene_invalid"),TEXT("Invalid place data / run launcher again")); return false;
    }
    Ocean=type==TEXT("sea"); SpawnYaw=float(yaw);
    data->TryGetStringField(TEXT("name"),PlaceLabel); data->TryGetStringField(TEXT("country"),country);
    PlaceLabel+=TEXT(" / ")+country;
    for(int i=0;i<5;++i) {
        if(!(*coeff)[i]->TryGetNumber(Coeff[i]) || !FMath::IsFinite(Coeff[i]) || FMath::Abs(Coeff[i])>(i<2?.1:.001)) {
            Fail(TEXT("surface_invalid"),TEXT("Invalid water height data")); return false;
        }
    }
    double b[4];
    for(int i=0;i<4;++i) if(!(*bounds)[i]->TryGetNumber(b[i]) || !FMath::IsFinite(b[i]) || FMath::Abs(b[i])>40000) {
        Fail(TEXT("bounds_invalid"),TEXT("Invalid water bounds")); return false;
    }
    if(b[2]<=b[0] || b[3]<=b[1]) { Fail(TEXT("bounds_empty"),TEXT("Empty water bounds")); return false; }
    Bounds=FVector4(b[0]*100,b[1]*100,(b[2]-b[0])*100,(b[3]-b[1])*100);
    for(const auto& poly:*polys) {
        const TArray<TSharedPtr<FJsonValue>>* rings=nullptr;
        if(!poly->TryGetArray(rings) || rings->IsEmpty()) { Fail(TEXT("polygon_invalid"),TEXT("Invalid shoreline")); return false; }
        FRowWaterPolygon p;
        for(const auto& ring:*rings) {
            const TArray<TSharedPtr<FJsonValue>>* points=nullptr;
            if(!ring->TryGetArray(points) || points->Num()<4 || points->Num()>100000) { Fail(TEXT("ring_invalid"),TEXT("Invalid shoreline")); return false; }
            TArray<FVector2D> r;
            for(const auto& point:*points) {
                const TArray<TSharedPtr<FJsonValue>>* xy=nullptr; double x,y;
                if(!point->TryGetArray(xy) || xy->Num()!=2 || !(*xy)[0]->TryGetNumber(x) || !(*xy)[1]->TryGetNumber(y) ||
                   !FMath::IsFinite(x) || !FMath::IsFinite(y) || FMath::Abs(x)>40000 || FMath::Abs(y)>40000) {
                    Fail(TEXT("point_invalid"),TEXT("Invalid shoreline")); return false;
                }
                r.Add(FVector2D(x,y));
            }
            p.Rings.Add(MoveTemp(r));
        }
        Polygons.Add(MoveTemp(p));
    }
    if(!CanNavigate(FVector::ZeroVector)) { Fail(TEXT("spawn_outside_water"),TEXT("Launch point is outside mapped water")); return false; }
    for(const auto& probe:*probes) {
        const TArray<TSharedPtr<FJsonValue>>* xyz=nullptr; double a[3];
        if(!probe->TryGetArray(xyz) || xyz->Num()!=3) { Fail(TEXT("probe_invalid"),TEXT("Invalid terrain check")); return false; }
        for(int i=0;i<3;++i) if(!(*xyz)[i]->TryGetNumber(a[i]) || !FMath::IsFinite(a[i])) { Fail(TEXT("probe_invalid"),TEXT("Invalid terrain check")); return false; }
        if(FMath::Abs(a[0])>180 || FMath::Abs(a[1])>85 || a[2]<-600 || a[2]>6700) { Fail(TEXT("probe_range"),TEXT("Invalid terrain check")); return false; }
        Probes.Add(FVector(a[0],a[1],a[2]));
    }
    TArray<uint8> pixels;
    if(!FFileHelper::LoadFileToArray(pixels,*maskPath) || pixels.Num()!=int(maskSize*maskSize)) {
        Fail(TEXT("mask_missing"),TEXT("Water mask missing / run launcher again")); return false;
    }
    Mask=UTexture2D::CreateTransient(int(maskSize),int(maskSize),PF_G8);
    Mask->SRGB=false; Mask->NeverStream=true; Mask->Filter=TF_Bilinear; Mask->AddressX=TA_Clamp; Mask->AddressY=TA_Clamp;
    auto& mip=Mask->GetPlatformData()->Mips[0];
    void* bytes=mip.BulkData.Lock(LOCK_READ_WRITE); FMemory::Memcpy(bytes,pixels.GetData(),pixels.Num()); mip.BulkData.Unlock(); Mask->UpdateResource();
    FString configPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../../../config/cesium.local.json"));
    FParse::Value(FCommandLine::Get(),TEXT("RowCesiumConfig="),configPath);
    FString token=FPlatformMisc::GetEnvironmentVariable(TEXT("CESIUM_ION_TOKEN"));
    TSharedPtr<FJsonObject> cfg; double terrainId=1,imageryId=2;
    if(FFileHelper::LoadFileToString(raw,*configPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(raw),cfg) && cfg.IsValid()) {
        if(token.IsEmpty()) cfg->TryGetStringField(TEXT("ion_access_token"),token);
        cfg->TryGetNumberField(TEXT("terrain_asset_id"),terrainId); cfg->TryGetNumberField(TEXT("imagery_asset_id"),imageryId);
    }
    if(token.IsEmpty() || !FMath::IsFinite(terrainId) || !FMath::IsFinite(imageryId) || terrainId<1 || imageryId<1) {
        Fail(TEXT("token_missing"),TEXT("Cesium settings missing / run launcher again")); return false;
    }
    Georeference=GetWorld()->SpawnActor<ACesiumGeoreference>();
    Georeference->SetOriginLongitudeLatitudeHeight(FVector(lon,lat,height));
    Terrain=ArriettyCesium::CreateTerrain(GetWorld(),Georeference.Get(),int64(terrainId),token);
    Terrain->SetEnableWaterMask(false);
    auto terrainBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Materials/MI_RowTerrain.MI_RowTerrain"));
    if(!terrainBase) { Fail(TEXT("terrain_material_missing"),TEXT("Terrain material missing / run prepare.ps1")); return false; }
    TerrainMaterial=UMaterialInstanceDynamic::Create(terrainBase,this); ConfigureWater(TerrainMaterial);
    Terrain->SetMaterial(TerrainMaterial);
    Terrain->ShowCreditsOnScreen=true;
    ArriettyCesium::FinishTerrain(Terrain,int64(imageryId),token);
    UE_LOG(LogTemp,Display,TEXT("ROW_GEOGRAPHY_CREATED type=%s msl_to_ellipsoid=EGM96 origin_height_m=%.3f radius_m=%.0f"),*type,height,RadiusM);
    return true;
}
void ARowGeography::BeginPlay() {
    Super::BeginPlay();
    if(Terrain && !Failed) Terrain->SampleHeightMostDetailed(Probes,FCesiumSampleHeightMostDetailedCallback::CreateUObject(this,&ARowGeography::HeightsSampled));
}
void ARowGeography::HeightsSampled(ACesium3DTileset*,const TArray<FCesiumSampleHeightResult>& Results,const TArray<FString>&) {
    if(Failed) return;
    if(Results.Num()!=Probes.Num()) { Fail(TEXT("height_count"),TEXT("Terrain unavailable / restart when online")); return; }
    std::array<double,5> deltas{};
    for(int i=0;i<Results.Num();++i) {
        if(!Results[i].SampleSuccess) { Fail(TEXT("height_unavailable"),TEXT("Terrain unavailable / restart when online")); return; }
        const double delta=Results[i].LongitudeLatitudeHeight.Z-Probes[i].Z;
        if(!FMath::IsFinite(delta)) { Fail(TEXT("height_invalid"),TEXT("Terrain height unavailable")); return; }
        deltas[i]=delta;
    }
    const auto alignment=row::alignLakeTerrain(deltas,Ocean);
    UE_LOG(LogTemp,Display,TEXT("ROW_TERRAIN_WATER_CHECK min_delta_m=%.3f max_delta_m=%.3f"),alignment.minDeltaM,alignment.maxDeltaM);
    if(alignment.status==row::WaterTerrainStatus::AlignLake) {
        // Move this Row tileset's rendering AND physics together, not the known
        // lake height, georeference, water, HMD or any flight coordinates.
        Terrain->AddActorWorldOffset(FVector(0,0,alignment.offsetM*100));
        UE_LOG(LogTemp,Display,TEXT("ROW_LAKE_TERRAIN_ALIGNMENT offset_m=%.3f spread_m=%.3f water_datum_unchanged=1"),
            alignment.offsetM,alignment.maxDeltaM-alignment.minDeltaM);
    } else if(alignment.status!=row::WaterTerrainStatus::Ready) {
        Fail(TEXT("water_below_terrain"),TEXT("Water/terrain mismatch / check place data")); return;
    }
    HeightChecked=true;
}
void ARowGeography::Tick(float dt) {
    Super::Tick(dt);
    if(Ready || Failed) return;
    Age+=dt;
    if(HeightChecked && Terrain && Terrain->GetLoadProgress()>=99.f) Settled+=dt; else Settled=0;
    if(Settled>=1) {
        Ready=true; Message.Empty();
        UE_LOG(LogTemp,Display,TEXT("ROW_GEOGRAPHY_READY height_verified=1 terrain_loaded=1"));
    } else if(Age>120) Fail(TEXT("load_timeout"),TEXT("Terrain load timed out / check connection and restart"));
}
double ARowGeography::SurfaceHeightCm(double Xcm,double Ycm) const {
    const double x=Xcm*.01,y=Ycm*.01;
    return 100*(Coeff[0]*x+Coeff[1]*y+Coeff[2]*x*x+Coeff[3]*x*y+Coeff[4]*y*y);
}
static bool InsideRing(const TArray<FVector2D>& Ring,FVector2D P) {
    bool inside=false;
    for(int i=0,j=Ring.Num()-1;i<Ring.Num();j=i++) {
        const auto& a=Ring[i]; const auto& b=Ring[j];
        if((a.Y>P.Y)!=(b.Y>P.Y) && P.X<(b.X-a.X)*(P.Y-a.Y)/(b.Y-a.Y)+a.X) inside=!inside;
    }
    return inside;
}
bool ARowGeography::CanNavigate(FVector Position) const {
    if(Position.ContainsNaN()) return false;
    const FVector2D p(Position.X*.01,Position.Y*.01);
    if(p.Size()>RadiusM) return false;
    for(const auto& poly:Polygons) {
        if(!InsideRing(poly.Rings[0],p)) continue;
        bool hole=false;
        for(int i=1;i<poly.Rings.Num();++i) if(InsideRing(poly.Rings[i],p)) { hole=true; break; }
        if(!hole) return true;
    }
    return false;
}
bool ARowGeography::CanNavigatePath(FVector From,FVector To) const {
    if(!CanNavigate(From) || !CanNavigate(To)) return false;
    // The radius is convex, so endpoint checks suffice for it. Polygon holes
    // and narrow banks need segment intersections, even with clear endpoints.
    const FVector2D a(From.X*.01,From.Y*.01),b(To.X*.01,To.Y*.01);
    const auto cross=[](FVector2D u,FVector2D v) { return u.X*v.Y-u.Y*v.X; };
    for(const auto& poly:Polygons) for(const auto& ring:poly.Rings) {
        for(int i=0,j=ring.Num()-1;i<ring.Num();j=i++) {
            const auto& c=ring[j]; const auto& d=ring[i];
            if(FMath::Max(a.X,b.X)<FMath::Min(c.X,d.X) || FMath::Max(c.X,d.X)<FMath::Min(a.X,b.X) ||
               FMath::Max(a.Y,b.Y)<FMath::Min(c.Y,d.Y) || FMath::Max(c.Y,d.Y)<FMath::Min(a.Y,b.Y)) continue;
            const double abC=cross(b-a,c-a),abD=cross(b-a,d-a),cdA=cross(d-c,a-c),cdB=cross(d-c,b-c);
            if(abC*abD<=0 && cdA*cdB<=0) return false;
        }
    }
    return true;
}
void ARowGeography::ConfigureWater(UMaterialInstanceDynamic* M) const {
    M->SetTextureParameterValue(TEXT("WaterMask"),Mask);
    M->SetVectorParameterValue(TEXT("WaterBounds"),FLinearColor(Bounds.X,Bounds.Y,Bounds.Z,Bounds.W));
    M->SetVectorParameterValue(TEXT("WaterSize"),FLinearColor(Bounds.Z,Bounds.W,0,0));
    M->SetVectorParameterValue(TEXT("SurfaceLinear"),FLinearColor(Coeff[0],Coeff[1],0,0));
    M->SetVectorParameterValue(TEXT("SurfaceCurve"),FLinearColor(Coeff[2],Coeff[3],Coeff[4],0));
    M->SetScalarParameterValue(TEXT("Ocean"),Ocean?1.f:0.f);
    if(Ocean) M->SetVectorParameterValue(TEXT("DeepWater"),FLinearColor(.009f,.095f,.13f,1));
}
