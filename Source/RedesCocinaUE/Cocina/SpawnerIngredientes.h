#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnerIngredientes.generated.h"

class AIngrediente;
class UFusionActorComponent;

/** Una entrada de spawn: que BP de ingrediente y cuantas unidades. */
USTRUCT(BlueprintType)
struct FSpawnIngredienteEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    TSubclassOf<AIngrediente> Clase;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    int32 Cantidad = 3;
};

/**
 * Spawnea ingredientes al inicio. Solo el Master Client ejecuta el spawn; el
 * FusionNetDriver replica los actores al resto. Ownership = MasterClient.
 */
UCLASS()
class REDESCOCINAUE_API ASpawnerIngredientes : public AActor
{
    GENERATED_BODY()

public:
    ASpawnerIngredientes();

    /** Que ingredientes y cuantos spawnear. Configurar en el BP/instancia. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    TArray<FSpawnIngredienteEntry> Entradas;

    /** Separacion entre ingredientes spawneados (en cm). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    float Separacion = 80.f;

    /** Altura sobre el spawner a la que aparecen. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    float AlturaSpawn = 50.f;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;
};
