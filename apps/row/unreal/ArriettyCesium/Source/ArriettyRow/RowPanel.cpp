#include "RowPanel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "RowCore.h"
int32 URowPanel::NativePaint(const FPaintArgs&,const FGeometry& g,const FSlateRect&,
    FSlateWindowElementList& e,int32 layer,const FWidgetStyle&,bool) const {
    const auto brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto box=[&](float x,float y,float w,float h,FLinearColor c) {
        FSlateDrawElement::MakeBox(e,layer,g.ToPaintGeometry(FVector2f(w,h),FSlateLayoutTransform(FVector2f(x,y))),brush,ESlateDrawEffect::None,c);
    };
    auto text=[&](float x,float y,FString s,int size,FLinearColor c) {
        FSlateDrawElement::MakeText(e,layer+1,g.ToPaintGeometry(FVector2f(1000,360),FSlateLayoutTransform(FVector2f(x,y))),s,
            FCoreStyle::GetDefaultFontStyle("Regular",size),ESlateDrawEffect::None,c);
    };
    const FLinearColor white(.91,.97,.94),muted(.42,.61,.57),mint(.33,1,.75);
    box(0,0,1000,360,FLinearColor(.008,.022,.024,1)); box(0,0,5,360,mint);
    text(25,14,TEXT("ARRIETTY / ROW"),22,mint); text(450,18,Status,18,white);
    const FString labels[]{TEXT("DISTANCE"),TEXT("TIME"),TEXT("SPEED  km/h"),TEXT("HEART  bpm")};
    const FString values[]{Distance,Time,Speed,Heart};
    for(int i=0;i<4;++i) { text(25+247*i,73,labels[i],18,muted); text(25+247*i,110,values[i],43,white); }
    box(25,195,950,1,FLinearColor(.07,.17,.16));
    text(25,215,Detail,18,white);
    const float margin=float(row::Model::StraightMargin*100),pixelsPerCm=6;
    const bool centered=FMath::Abs(LeanCm)<=margin;
    const FLinearColor amber(1.f,.70f,.26f);
    text(25,265,SteeringAvailable?(centered?TEXT("CENTER"):LeanCm<0?TEXT("LEFT"):TEXT("RIGHT")):TEXT("SETUP"),22,
        SteeringAvailable?(centered?mint:amber):muted);
    text(257,269,TEXT("LEFT"),14,muted); text(712,269,TEXT("RIGHT"),14,muted);
    box(320,275,360,12,FLinearColor(.06f,.13f,.14f));
    box(500-margin*pixelsPerCm,273,2*margin*pixelsPerCm,16,FLinearColor(.10f,.35f,.26f));
    box(499,269,2,24,muted);
    if(SteeringAvailable) {
        const float marker=500+FMath::Clamp(LeanCm,-30.f,30.f)*pixelsPerCm;
        box(marker-3,269,6,24,centered?mint:amber);
        text(795,265,FString::Printf(TEXT("%+.1f cm"),LeanCm),22,centered?mint:amber);
    }
    text(400,299,TEXT("STRAIGHT  +/- 8 cm"),14,muted);
    text(25,334,Guide,14,muted);
    return layer+2;
}
