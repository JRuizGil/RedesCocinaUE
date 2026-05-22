#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Cocina/Ingrediente.h"
#include "RecetaAsset.generated.h"

/**
 * Receta de cocina: lista de tipos de ingrediente que deben estar TODOS en estado
 * Procesado y combinarse en la zona de emplatado. El orden no importa (se compara
 * como multiconjunto).
 */
UCLASS(BlueprintType)
class REDESCOCINAUE_API URecetaAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    FText DisplayName;

    /** Ingredientes necesarios. Deben estar todos en estado Procesado al emplatar. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    TArray<ETipoIngrediente> Ingredientes;

    /** Puntos otorgados al completar la receta. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    int32 PuntosCompletar = 50;
};
