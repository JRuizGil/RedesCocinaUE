#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CocinaGameMode.generated.h"

UCLASS()
class REDESCOCINAUE_API ACocinaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACocinaGameMode();

    virtual void Tick(float DeltaTime) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
    int32 NextStartIndex = 0;
};
