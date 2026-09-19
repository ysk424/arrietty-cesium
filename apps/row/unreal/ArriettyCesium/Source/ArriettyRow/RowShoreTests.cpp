#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRowShoreTest,"ArriettyRow.Water.ShoreField",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRowShoreTest::RunTest(const FString&) {
    if(!TestTrue(TEXT("Offline fixture only"),FParse::Param(FCommandLine::Get(),TEXT("RowTestFixture")))) return false;
    auto world=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
    if(!TestNotNull(TEXT("World"),world)) return false;
    auto material=[&](const TCHAR* name) {
        auto base=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Row/Waterline/%s.%s"),name,name));
        return base?UMaterialInstanceDynamic::Create(base,world):nullptr;
    };
    auto seed=material(TEXT("M_RowShoreSeeds")),jump=material(TEXT("M_RowShoreJump")),field=material(TEXT("M_RowShoreField"));
    auto probe=material(TEXT("MI_RowShoreProbe"));
    if(!TestTrue(TEXT("Generate local shoreline materials before this optional GPU test"),seed && jump && field && probe)) return false;
#if WITH_EDITOR
    // The synchronous test cannot let the editor finish newly loaded texture
    // compilation on subsequent frames. Wait before sampling vendor textures.
    FAssetCompilingManager::Get().FinishAllCompilation();
#endif
    AddInfo(FString::Printf(TEXT("Shore noise texture=%s"),*GetNameSafe(probe->K2_GetTextureParameterValue(TEXT("Shore Noise Texture")))));
    auto a=UKismetRenderingLibrary::CreateRenderTarget2D(world,512,512,RTF_RGBA16f,FLinearColor::Black,false);
    auto b=UKismetRenderingLibrary::CreateRenderTarget2D(world,512,512,RTF_RGBA16f,FLinearColor::Black,false);
    auto result=UKismetRenderingLibrary::CreateRenderTarget2D(world,512,512,RTF_RGBA16f,FLinearColor::Black,false);
    auto depth=UTexture2D::CreateTransient(512,512,PF_A32B32G32R32F);
    depth->SRGB=false; depth->Filter=TF_Nearest; depth->NeverStream=true;
    for(int pattern:{0,1,2}) {
        TArray<FLinearColor> values; values.SetNumUninitialized(512*512);
        for(int y=0;y<512;++y) for(int x=0;x<512;++x)
            values[y*512+x]=FLinearColor((pattern==1 && x>=256) || (pattern==2 && x<256)?999000.f:2000000.f,0,0,1);
        auto& mip=depth->GetPlatformData()->Mips[0];
        void* dst=mip.BulkData.Lock(LOCK_READ_WRITE); FMemory::Memcpy(dst,values.GetData(),values.Num()*sizeof(FLinearColor)); mip.BulkData.Unlock(); depth->UpdateResource();
        seed->SetTextureParameterValue(TEXT("Source"),depth);
        UKismetRenderingLibrary::DrawMaterialToRenderTarget(world,a,seed);
        auto from=a,to=b;
        for(int step=16;step>=1;step/=2) {
            jump->SetTextureParameterValue(TEXT("Source"),from); jump->SetScalarParameterValue(TEXT("Jump"),step);
            UKismetRenderingLibrary::DrawMaterialToRenderTarget(world,to,jump); Swap(from,to);
        }
        field->SetTextureParameterValue(TEXT("Source"),from);
        UKismetRenderingLibrary::DrawMaterialToRenderTarget(world,result,field);
        TArray<FLinearColor> pixels;
        TestTrue(TEXT("Read actual GPU result"),UKismetRenderingLibrary::ReadRenderTargetRaw(world,result,pixels,false));
        if(pixels.Num()!=512*512) return false;
        int active=0; bool finite=true;
        for(const auto& p:pixels) { if(p.R>0) ++active; finite&=FMath::IsFinite(p.R)&&FMath::IsFinite(p.G)&&FMath::IsFinite(p.B); }
        TestTrue(TEXT("Field is finite"),finite);
        if(!pattern) TestEqual(TEXT("Empty sea has no artificial texture-edge shore"),active,0);
        else {
            const auto near=pixels[256*512+(pattern==1?246:265)];
            TestTrue(TEXT("Ten metres offshore has expected influence"),FMath::Abs(near.R-(1.f-10.f/24.f))<.06f);
            TestTrue(TEXT("Signed direction points toward the land"),near.G*(pattern==1?1.f:-1.f)>.98f && FMath::Abs(near.B)<.02f);
            TestEqual(TEXT("Far water has no surf"),pixels[256*512+(pattern==1?100:400)].R,0.f);
            TestEqual(TEXT("Land is not water"),pixels[256*512+(pattern==1?300:100)].R,0.f);
            TestTrue(TEXT("Only a bounded coastal strip is active"),active>9000 && active<14000);
            if(pattern==1) {
                probe->SetTextureParameterValue(TEXT("Shore Data Texture"),result);
                probe->SetScalarParameterValue(TEXT("Shore Texture Scale"),51200);
                TArray<FLinearColor> previous;
                for(float time:{0.f,2.f}) {
                    probe->SetScalarParameterValue(TEXT("ProbeTime"),time);
                    UKismetRenderingLibrary::DrawMaterialToRenderTarget(world,a,probe);
                    UKismetRenderingLibrary::ReadRenderTargetRaw(world,a,pixels,false);
                    float peak=0,change=0,wave=0,influence=0;
                    for(int i=0;i<pixels.Num();++i) {
                        peak=FMath::Max(peak,pixels[i].R);
                        wave=FMath::Max(wave,pixels[i].G); influence=FMath::Max(influence,pixels[i].B);
                        if(previous.Num()==pixels.Num()) change+=FMath::Abs(pixels[i].R-previous[i].R);
                    }
                    AddInfo(FString::Printf(TEXT("Vendor foam time=%.1f peak=%.4f change=%.4f wave=%.4f influence=%.4f"),time,peak,change,wave,influence));
                    TestTrue(TEXT("Purchased shore function produces foam"),peak>.01f);
                    TestTrue(TEXT("Purchased shore function produces bounded vertical waves"),wave>.01f && wave<=15.f);
                    if(!previous.IsEmpty()) TestTrue(TEXT("Purchased foam changes over time"),change>1);
                    previous=pixels;
                }
            }
        }
    }
    return true;
}
#endif
