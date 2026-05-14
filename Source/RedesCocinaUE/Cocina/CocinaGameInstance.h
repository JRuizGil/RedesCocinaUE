#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CocinaGameInstance.generated.h"

UCLASS()
class REDESCOCINAUE_API UCocinaGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    /** Nombre que introduce el jugador en el menú. Persiste toda la sesión. */
    UPROPERTY(BlueprintReadWrite, Category = "Cocina")
    FString PlayerDisplayName = TEXT("Jugador");

    /** Validación simple: 1..16 caracteres, sin espacios al borde. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    bool SetPlayerName(const FString& Raw);
};