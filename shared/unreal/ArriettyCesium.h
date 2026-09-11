#pragma once
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumIonRasterOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Shared setup only: water rules, flight physics and hardware stay in each app.
namespace ArriettyCesium {
inline ACesium3DTileset* CreateTerrain(UWorld* World, ACesiumGeoreference* Geo,
    int64 TerrainId, const FString& Token) {
    auto Terrain=World->SpawnActorDeferred<ACesium3DTileset>(ACesium3DTileset::StaticClass(),FTransform::Identity);
    Terrain->SetGeoreference(Geo); Terrain->SetIonAssetID(TerrainId); Terrain->SetIonAccessToken(Token);
    Terrain->SetMaximumScreenSpaceError(8); Terrain->SetCreatePhysicsMeshes(true);
    Terrain->ShowCreditsOnScreen=true;
    return Terrain;
}
inline void FinishTerrain(ACesium3DTileset* Terrain, int64 ImageryId, const FString& Token) {
    UGameplayStatics::FinishSpawningActor(Terrain,FTransform::Identity);
    auto Overlay=NewObject<UCesiumIonRasterOverlay>(Terrain,TEXT("SatelliteImagery"));
    Overlay->IonAssetID=ImageryId; Overlay->IonAccessToken=Token;
    Terrain->AddInstanceComponent(Overlay); Overlay->RegisterComponent(); Overlay->Activate(true);
}
// Flight's transport frame is north/east in centimetres, with geographic bearing.
inline double BearingToEsuYaw(double Bearing) { return FRotator::NormalizeAxis(Bearing-90.); }
inline double EsuYawToBearing(double Yaw) { return FRotator::ClampAxis(Yaw+90.); }
}
