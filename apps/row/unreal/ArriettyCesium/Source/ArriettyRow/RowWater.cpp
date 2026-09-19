#include "RowWater.h"
#include "RowGeography.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Math/Float16Color.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void ARowWater::StartWaterline() {
    if(FParse::Param(FCommandLine::Get(),TEXT("RowLegacyWater")) ||
       FParse::Param(FCommandLine::Get(),TEXT("RowTestFixture"))) return;
    if(!FPackageName::DoesPackageExist(TEXT("/Game/Row/Waterline/MI_RowWaterline"))) return;
    auto material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Waterline/MI_RowWaterline.MI_RowWaterline"));
    if(!material) return; // Optional locally purchased content.
    auto cls=LoadClass<AActor>(nullptr,TEXT("/Game/Waterline/3_Ocean_Sim/BP_Waterline_Ocean_Gen_4.BP_Waterline_Ocean_Gen_4_C"));
    if(!cls) { UE_LOG(LogTemp,Error,TEXT("ROW_WATERLINE_MISSING_CLASS")); return; }
    Waterline=GetWorld()->SpawnActorDeferred<AActor>(cls,FTransform::Identity,this);
    if(!Waterline) return;
    bool valid=true;
    auto boolean=[&](const TCHAR* name,bool value) {
        auto p=FindFProperty<FBoolProperty>(cls,name);
        if(p) p->SetPropertyValue_InContainer(Waterline,value); else valid=false;
    };
    auto number=[&](const TCHAR* name,double value) {
        auto p=FindFProperty<FNumericProperty>(cls,name);
        if(p && p->IsFloatingPoint()) p->SetFloatingPointPropertyValue(p->ContainerPtrToValuePtr<void>(Waterline),value);
        else valid=false;
    };
    // The vendor actor computes waves only. ROW owns movement, water datum,
    // hull exclusion, rowing wakes and camera. No vendor buoyancy or input.
    for(auto name:{TEXT("Use Buoyancy"),TEXT("Use Ocean Buoyancy"),TEXT("Use Shore Buoyancy"),
                   TEXT("Use Drag Functions"),TEXT("Enable Shallow Water Sim"),TEXT("Use Underwater VFX"),
                   TEXT("Underwater Surface Enable"),TEXT("Volumetric Fog Enable"),
                   TEXT("Run Cascade 2"),TEXT("Run Cascade 3"),TEXT("Run Cascade 4")}) boolean(name,false);
    number(TEXT("1 Ocean Wave Height"),1.0);
    number(TEXT("1 Ocean Wave Steepness "),0.0);
    number(TEXT("1 Micro FFT Displacement Height"),0.0);
    if(auto p=FindFProperty<FObjectPropertyBase>(cls,TEXT("1 Water Surface Material")))
        p->SetObjectPropertyValue_InContainer(Waterline,material);
    else valid=false;
    if(!valid) {
        UE_LOG(LogTemp,Error,TEXT("ROW_WATERLINE_INCOMPATIBLE_PROPERTIES using_original_water"));
        Waterline->Destroy(); Waterline=nullptr; return;
    }
    UGameplayStatics::FinishSpawningActor(Waterline,FTransform::Identity);
    SyncWaterline();
    if(!WaterlineMaterial) {
        UE_LOG(LogTemp,Error,TEXT("ROW_WATERLINE_MATERIAL_UNAVAILABLE using_original_water"));
        Waterline->Destroy(); Waterline=nullptr; return;
    }
    UE_LOG(LogTemp,Display,TEXT("ROW_WATERLINE_CREATED geographic_mesh=1 vendor_buoyancy=0 vendor_underwater=0"));
}
void ARowWater::SyncWaterline() {
    if(!Waterline) return;
    if(!WaterlineTickOrdered) {
        auto pc=GetWorld()->GetFirstPlayerController();
        if(pc && pc->GetPawn()) {
            pc->GetPawn()->AddTickPrerequisiteActor(Waterline);
            WaterlineTickOrdered=true;
        }
    }
    TInlineComponentArray<UMeshComponent*> meshes(Waterline);
    for(auto mesh:meshes) { mesh->SetVisibility(false); mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
    auto p=FindFProperty<FObjectPropertyBase>(Waterline->GetClass(),TEXT("Water Surface DYN"));
    auto source=p?Cast<UMaterialInstanceDynamic>(p->GetObjectPropertyValue_InContainer(Waterline)):nullptr;
    if(!source) return;
    if(!WaterlineMaterial) UE_LOG(LogTemp,Display,TEXT("ROW_WATERLINE_MATERIAL_BOUND material=%s"),*source->GetName());
    WaterlineMaterial=source;
    if(LocalMaterial && DistantMaterial) {
        for(auto m:{LocalMaterial,DistantMaterial}) {
            m->CopyInterpParameters(source);
            m->SetTextureParameterValue(TEXT("WakeField"),Field);
        }
    }
}
void ARowWater::EndPlay(const EEndPlayReason::Type Reason) {
    if(IsValid(Waterline)) Waterline->Destroy();
    Waterline=nullptr; WaterlineMaterial=nullptr;
    Super::EndPlay(Reason);
}

ARowWater::ARowWater() {
    Patch=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LocalWaveSurface")); RootComponent=Patch;
    Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision); Patch->SetCastShadow(false);
    Patch->SetBoundsScale(1.1f);
    FarSurface=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DistantWater"));
    FarSurface->SetupAttachment(RootComponent); FarSurface->SetAbsolute(true,true,true);
    FarSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision); FarSurface->SetCastShadow(false);
    FarSurface->SetBoundsScale(2.f);
}
void ARowWater::BeginPlay() {
    Super::BeginPlay();
    StartWaterline();
    auto base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Materials/M_RowWater.M_RowWater"));
    if(Waterline) base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Row/Waterline/MI_RowWaterline.MI_RowWaterline"));
    if(!base) { UE_LOG(LogTemp,Error,TEXT("ROW_WATER_MATERIAL_MISSING")); return; }
    Field=UTexture2D::CreateTransient(row::Waves::N,row::Waves::N,PF_FloatRGBA);
    Field->SRGB=false; Field->NeverStream=true; Field->AddressX=TA_Clamp; Field->AddressY=TA_Clamp;
    Field->Filter=TF_Bilinear; Field->UpdateResource();
    LocalMaterial=UMaterialInstanceDynamic::Create(base,this);
    DistantMaterial=UMaterialInstanceDynamic::Create(base,this);
    LocalMaterial->SetScalarParameterValue(TEXT("LocalPatch"),1);
    if(auto geo=ARowGeography::Get(GetWorld()); geo && !geo->HasFailed()) {
        geo->ConfigureWater(LocalMaterial); geo->ConfigureWater(DistantMaterial);
        constexpr int FarN=241;
        const double radius=geo->GetRenderRadiusCm(),step=2*radius/(FarN-1);
        TArray<FVector> fv,fn; TArray<FVector2D> fuv; TArray<int32> ft;
        for(int y=0;y<FarN;++y) for(int x=0;x<FarN;++x) {
            fv.Add(FVector(x*step-radius,y*step-radius,0)); fn.Add(FVector::UpVector); fuv.Add(FVector2D(double(x)/(FarN-1),double(y)/(FarN-1)));
        }
        for(int y=0;y<FarN-1;++y) for(int x=0;x<FarN-1;++x) { int k=y*FarN+x; ft.Append({k,k+1,k+FarN,k+1,k+FarN+1,k+FarN}); }
        FarSurface->CreateMeshSection_LinearColor(0,fv,ft,fn,fuv,{}, {},false);
        FarSurface->SetMaterial(0,DistantMaterial);
    }
    for(auto m:{LocalMaterial,DistantMaterial}) m->SetTextureParameterValue(TEXT("WakeField"),Field);
    int waterActors=0;
    for(TActorIterator<AStaticMeshActor> it(GetWorld());it;++it) {
        auto mesh=it->GetStaticMeshComponent();
        if(it->ActorHasTag(TEXT("RowWater"))) {
            for(int i=0;i<mesh->GetNumMaterials();++i) mesh->SetMaterial(i,DistantMaterial);
            mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); ++waterActors;
        }
    }
    constexpr int N=257; constexpr float Size=6400.f,Step=Size/(N-1);
    TArray<FVector> v,n; TArray<FVector2D> uv; TArray<int32> tri;
    for(int y=0;y<N;++y) for(int x=0;x<N;++x) { v.Add({x*Step-Size/2,y*Step-Size/2,0}); n.Add(FVector::UpVector); uv.Add({float(x)/(N-1),float(y)/(N-1)}); }
    for(int y=0;y<N-1;++y) for(int x=0;x<N-1;++x) { int k=y*N+x; tri.Append({k,k+1,k+N,k+1,k+N+1,k+N}); }
    Patch->CreateMeshSection_LinearColor(0,v,tri,n,uv,{}, {},false);
    Patch->SetMaterial(0,LocalMaterial);
    Upload();
    SyncWaterline();
    UE_LOG(LogTemp,Display,TEXT("ROW_WATER_READY far_actors=%d local_triangles=%d field=256 kelvin=deep_water_fft hz=30 bow_whitewater=speed"),waterActors,tri.Num()/3);
}
void ARowWater::UpdateBoat(FVector pos,float heading,float speed,float drive,float dt) {
    if(!LocalMaterial) return;
    SyncWaterline();
    const double x=pos.X*.01,y=pos.Y*.01;
    static_assert(row::Waves::N==row::KelvinWake::N && row::Waves::Cell==row::KelvinWake::Cell);
    Waves.center(x,y); Kelvin.center(x,y); SetActorLocation(FVector(pos.X,pos.Y,0));
    for(auto m:{LocalMaterial,DistantMaterial}) {
        m->SetVectorParameterValue(TEXT("Boat"),FLinearColor(pos.X,pos.Y,0,0));
        m->SetVectorParameterValue(TEXT("BoatDirection"),FLinearColor(std::cos(heading),std::sin(heading),0,0));
    }
    const double fx=std::cos(heading),fy=std::sin(heading),rx=-fy,ry=fx;
    // Hull gravity waves use deep-water dispersion; the short-range layer below
    // supplies oar ripples and bubbles only (no competing constant-speed bow V).
    WakeTime+=dt;
    if(WakeTime>=.08 && speed>.12) {
        WakeTime=0; const float strength=FMath::Clamp(speed/3.f,0.f,1.f);
        for(int side:{-1,1}) Waves.disturb(x-fx*1.8+rx*.45*side,y-fy*1.8+ry*.45*side,
            0,.40f,.025f*strength);
    }
    if(drive>.35 && !WasDriving) {
        for(int side:{-1,1}) Waves.disturb(x+rx*1.9*side,y+ry*1.9*side,.009f,.34f,.5f);
        WasDriving=true;
    } else if(drive<.08) WasDriving=false;
    Accumulator+=FMath::Min(dt,.1f);
    const row::BowWhitewater bow(speed);
    while(Accumulator>=row::Waves::Step) {
        bow.emit(Waves,x,y,heading,row::Waves::Step);
        Waves.tick(); Accumulator-=row::Waves::Step;
    }
    if(!HaveBoat || std::hypot(x-PreviousX,y-PreviousY)>8) {
        PreviousX=x; PreviousY=y; PreviousHeading=heading; PreviousSpeed=0;
        KelvinAccumulator=0; HaveBoat=true;
    }
    const double frame=FMath::Clamp(double(dt),0.,.1);
    WaterTime+=frame;
    if(Waterline && WaterTime>5 && WaterTime-frame<=5 && FParse::Param(FCommandLine::Get(),TEXT("RowDemo")) &&
       FParse::Param(FCommandLine::Get(),TEXT("RowWaterlineCheck"))) {
        auto p=FindFProperty<FObjectPropertyBase>(Waterline->GetClass(),TEXT("1 RT Height"));
        auto rt=p?Cast<UTextureRenderTarget2D>(p->GetObjectPropertyValue_InContainer(Waterline)):nullptr;
        TArray<FLinearColor> samples;
        float low=FLT_MAX,high=-FLT_MAX;
        if(rt && UKismetRenderingLibrary::ReadRenderTargetRaw(this,rt,samples,false))
            for(auto c:samples) { low=FMath::Min(low,c.B); high=FMath::Max(high,c.B); }
        UE_LOG(LogTemp,Display,TEXT("ROW_WATERLINE_GPU_CHECK samples=%d blue_min=%g blue_max=%g local_patch=%g source=%d"),
            samples.Num(),low,high,LocalMaterial->K2_GetScalarParameterValue(TEXT("LocalPatch")),WaterlineMaterial!=nullptr);
        UE_LOG(LogTemp,Display,TEXT("ROW_WATERLINE_BINDINGS ocean=%g mask=%s field=%s linear=%s curve=%s source_parameters=%d"),
            LocalMaterial->K2_GetScalarParameterValue(TEXT("Ocean")),
            *GetNameSafe(LocalMaterial->K2_GetTextureParameterValue(TEXT("WaterMask"))),
            *GetNameSafe(LocalMaterial->K2_GetTextureParameterValue(TEXT("WakeField"))),
            *LocalMaterial->K2_GetVectorParameterValue(TEXT("SurfaceLinear")).ToString(),
            *LocalMaterial->K2_GetVectorParameterValue(TEXT("SurfaceCurve")).ToString(),
            WaterlineMaterial?WaterlineMaterial->ScalarParameterValues.Num():0);
    }
    KelvinAccumulator+=frame;
    while(KelvinAccumulator>=row::KelvinWake::Step) {
        // Sample the travelled segment near the middle of each fixed step.
        const double blend=frame>0?std::clamp((frame-KelvinAccumulator+row::KelvinWake::Step*.5)/frame,0.,1.):1.;
        const double turn=std::atan2(std::sin(heading-PreviousHeading),std::cos(heading-PreviousHeading));
        Kelvin.tick(PreviousX+(x-PreviousX)*blend,PreviousY+(y-PreviousY)*blend,
            PreviousHeading+turn*blend,speed>0?float(PreviousSpeed+(speed-PreviousSpeed)*blend):0.f);
        KelvinAccumulator-=row::KelvinWake::Step;
    }
    PreviousX=x; PreviousY=y; PreviousHeading=heading; PreviousSpeed=speed;
    UploadTime+=dt; if(UploadTime>=1./30.) { UploadTime=0; Upload(); }
}
void ARowWater::Upload() {
    if(!Field) return;
    constexpr int N=row::Waves::N;
    // Keep the texture's world-space origin paired with this upload; changing
    // it between 30 Hz uploads makes an otherwise stationary wake judder.
    for(auto m:{LocalMaterial,DistantMaterial})
        m->SetVectorParameterValue(TEXT("FieldOrigin"),FLinearColor(Waves.originX*100,Waves.originY*100,6400,0));
    const row::BowWhitewater bow(PreviousSpeed);
    const double fx=std::cos(PreviousHeading),fy=std::sin(PreviousHeading);
    for(int y=0;y<N;++y) for(int x=0;x<N;++x) {
        const int k=y*N+x;
        const double dx=Waves.originX+x*row::Waves::Cell-PreviousX,dy=Waves.originY+y*row::Waves::Cell-PreviousY;
        Surface[k]=Waves.height[k]+Kelvin.height[k]+bow.crest(float(dx*fx+dy*fy),float(-dx*fy+dy*fx),WaterTime);
    }
    auto height=[&](int k) { return Surface[k]; };
    auto pixels=new FFloat16Color[N*N];
    for(int y=0;y<N;++y) for(int x=0;x<N;++x) {
        const int k=y*N+x;
        const float dx=(height(y*N+FMath::Min(x+1,N-1))-height(y*N+FMath::Max(x-1,0)))/(2*row::Waves::Cell);
        const float dy=(height(FMath::Min(y+1,N-1)*N+x)-height(FMath::Max(y-1,0)*N+x))/(2*row::Waves::Cell);
        const float foam=1-(1-Waves.foam[k])*(1-Waves.bowFoam[k]);
        pixels[k]=FFloat16Color(FLinearColor(height(k),dx,dy,foam));
    }
    auto region=new FUpdateTextureRegion2D(0,0,0,0,N,N);
    Field->UpdateTextureRegions(0,1,region,N*sizeof(FFloat16Color),sizeof(FFloat16Color),
        reinterpret_cast<uint8*>(pixels),[](uint8* data,const FUpdateTextureRegion2D* r) {
            delete[] reinterpret_cast<FFloat16Color*>(data); delete r;
        });
}
