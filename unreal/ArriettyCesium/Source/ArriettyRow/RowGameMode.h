#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RowGameMode.generated.h"
UCLASS()
class ARRIETTYROW_API ARowGameMode:public AGameModeBase {
    GENERATED_BODY()
public:
    ARowGameMode();
    virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
};
