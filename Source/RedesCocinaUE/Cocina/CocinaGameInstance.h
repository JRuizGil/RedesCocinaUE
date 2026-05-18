#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CocinaGameInstance.generated.h"

UCLASS()
class REDESCOCINAUE_API UCocinaGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    /** Nombre que introduce el jugador en el men�. Persiste toda la sesi�n. */
    UPROPERTY(BlueprintReadWrite, Category = "Cocina")
    FString PlayerDisplayName = TEXT("Jugador");

    /** Validaci�n simple: 1..16 caracteres, sin espacios al borde. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    bool SetPlayerName(const FString& Raw);
};