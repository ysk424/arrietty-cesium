#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RowPanel.generated.h"
UCLASS()
class ARRIETTYROW_API URowPanel:public UUserWidget {
    GENERATED_BODY()
public:
    FString Distance=TEXT("0 m"),Time=TEXT("00:00"),Speed=TEXT("0.0"),Heart=TEXT("--");
    FString Status=TEXT("NUM ENTER  Start"),Detail=TEXT("Lake Bled / Slovenia");
    FString Guide;
    float LeanCm=0;
    bool SteeringAvailable=false;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,
        int32,const FWidgetStyle&,bool) const override;
};
