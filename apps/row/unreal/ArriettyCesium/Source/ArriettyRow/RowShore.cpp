#include "RowShore.h"
#include "RowGeography.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Math/OrthoMatrix.h"

ARowShore::ARowShore() {
    PrimaryActorTick.bCanEverTick=false;
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ShoreTerrainCapture")); RootComponent=Capture;
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->ProjectionType=ECameraProjectionMode::Orthographic; Capture->OrthoWidth=51200;
    Capture->bAutoCalculateOrthoPlanes=false; Capture->bUpdateOrthoPlanes=false;
    // UE's default ortho far plane is shorter than this capture's altitude.
    // Explicit 0–20 km planes keep depth in centimetres relative to the 10 km camera.
    Capture->bUseCustomProjectionMatrix=true;
    Capture->CustomProjectionMatrix=FReversedZOrthoMatrix(25600.,25600.,1./2000000.,0.);
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneDepth;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetDynamicShadows(false); Capture->ShowFlags.SetLighting(false);
    Capture->ShowFlags.SetPostProcessing(false);
}
bool ARowShore::Initialize(ARowGeography* Geo,UMaterialInstanceDynamic* Water) {
    Geography=Geo; WaterMaterial=Water;
    auto load=[&](const TCHAR* name) {
        auto base=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Row/Waterline/%s.%s"),name,name));
        return base?UMaterialInstanceDynamic::Create(base,this):nullptr;
    };
    SeedMaterial=load(TEXT("M_RowShoreSeeds")); JumpMaterial=load(TEXT("M_RowShoreJump")); FieldMaterial=load(TEXT("M_RowShoreField"));
    if(!SeedMaterial || !JumpMaterial || !FieldMaterial) return false;
    auto rt=[&](ETextureRenderTargetFormat format) {
        return UKismetRenderingLibrary::CreateRenderTarget2D(this,512,512,format,FLinearColor::Black,false);
    };
    Depth=rt(RTF_RGBA32f); Seeds=rt(RTF_RGBA16f); Scratch=rt(RTF_RGBA16f); Field=rt(RTF_RGBA16f);
    if(!Depth || !Seeds || !Scratch || !Field) return false;
    Seeds->Filter=TF_Nearest; Scratch->Filter=TF_Nearest; Field->Filter=TF_Bilinear;
    Capture->TextureTarget=Depth;
    Geo->ConfigureWater(SeedMaterial);
    Water->SetTextureParameterValue(TEXT("Shore Data Texture"),Field);
    Water->SetScalarParameterValue(TEXT("Shore Texture Scale"),51200);
    Water->SetScalarParameterValue(TEXT("Shore Texture Angle"),0);
    Water->SetScalarParameterValue(TEXT("RowShoreEnabled"),1);
    // Warm these small passes before the geography readiness gate opens.
    Update(FVector::ZeroVector,1);
    UE_LOG(LogTemp,Display,TEXT("ROW_SHORE_CAPTURE_READY size=512 span_m=512 interval_s=1 visual_only=1"));
    return true;
}
void ARowShore::Update(FVector Boat,float Dt) {
    Age+=Dt;
    if(Age<1 || !Geography || !WaterMaterial) return;
    Age=0;
    // Snap to texels to avoid swimming while preserving a common XY/UV basis.
    FVector center(FMath::RoundToDouble(Boat.X/100)*100,FMath::RoundToDouble(Boat.Y/100)*100,1000000);
    SetActorLocationAndRotation(center,FRotator(-90,-90,0));
    Capture->ShowOnlyComponents.Empty(); Capture->ShowOnlyActorComponents(Geography->GetTerrainActor(),true);
    Capture->CaptureScene();
    auto draw=[&](UTextureRenderTarget2D* target,UMaterialInstanceDynamic* material,UTextureRenderTarget2D* input) {
        material->SetTextureParameterValue(TEXT("Source"),input);
        UKismetRenderingLibrary::DrawMaterialToRenderTarget(this,target,material);
    };
    SeedMaterial->SetVectorParameterValue(TEXT("Center"),FLinearColor(center.X,center.Y,0,0));
    draw(Seeds,SeedMaterial,Depth);
    auto from=Seeds.Get(),to=Scratch.Get();
    for(int jump=16;jump>=1;jump/=2) {
        JumpMaterial->SetScalarParameterValue(TEXT("Jump"),jump); draw(to,JumpMaterial,from); Swap(from,to);
    }
    draw(Field,FieldMaterial,from);
    WaterMaterial->SetVectorParameterValue(TEXT("Shore Texture Location"),FLinearColor(center.X,center.Y,0,0));
    // Explicit demo-only GPU readback: absent from normal VR operation.
    if(!DiagnosticDone && Geography->IsReady() && FParse::Param(FCommandLine::Get(),TEXT("RowDemo")) &&
       FParse::Param(FCommandLine::Get(),TEXT("RowShoreCheck"))) {
        DiagnosticDone=true;
        TArray<FLinearColor> pixels;
        UKismetRenderingLibrary::ReadRenderTargetRaw(this,Field,pixels,false);
        int active=0; float maximum=0;
        for(const auto& p:pixels) { if(p.R>0) ++active; maximum=FMath::Max(maximum,p.R); }
        UE_LOG(LogTemp,Display,TEXT("ROW_SHORE_CHECK pixels=%d active=%d max=%.4f"),pixels.Num(),active,maximum);
        UKismetRenderingLibrary::ReadRenderTargetRaw(this,Depth,pixels,false);
        float low=FLT_MAX,high=-FLT_MAX;
        for(const auto& p:pixels) { low=FMath::Min(low,p.R); high=FMath::Max(high,p.R); }
        UE_LOG(LogTemp,Display,TEXT("ROW_SHORE_DEPTH_CHECK min=%.2f max=%.2f components=%d"),low,high,Capture->ShowOnlyComponents.Num());
        UKismetRenderingLibrary::ExportRenderTarget(this,Field,FPaths::ProjectSavedDir(),TEXT("row-shore-field.hdr"));
    }
}
