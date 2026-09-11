#include "RowGameMode.h"
#include "RowPawn.h"
#include "RowGeography.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ARowGameMode::ARowGameMode() { DefaultPawnClass=ARowPawn::StaticClass(); }
void ARowGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) {
    Super::InitGame(MapName,Options,ErrorMessage);
    if(FParse::Param(FCommandLine::Get(),TEXT("RowTestFixture")) &&
       (FParse::Param(FCommandLine::Get(),TEXT("RowOffline")) || FParse::Param(FCommandLine::Get(),TEXT("RowDemo")))) return;
    FString scene;
    FParse::Value(FCommandLine::Get(),TEXT("RowPlace="),scene);
    auto geography=GetWorld()->SpawnActor<ARowGeography>();
    geography->Initialize(scene);
}
