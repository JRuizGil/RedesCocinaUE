#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cocina/Ingrediente.h"
#include "ZonaEmplatado.generated.h"

class UStaticMeshComponent;
class UFusionActorComponent;
class APlayerCocina;

/**
 * Zona de emplatado: el jugador deposita aqui ingredientes ya procesados. Cuando la
 * composicion coincide con la receta activa del GameState, el MC suma puntos y vacia
 * el plato. Ownership = MasterClient: solo el MC modifica PlatoActual.
 */
UCLASS()
class REDESCOCINAUE_API AZonaEmplatado : public AActor
{
    GENERATED_BODY()

public:
    AZonaEmplatado();

    /** Tipos colocados actualmente en el plato. Replicado; solo el MC lo escribe. */
    UPROPERTY(ReplicatedUsing = OnRep_PlatoActual, BlueprintReadOnly, Category = "Cocina")
    TArray<ETipoIngrediente> PlatoActual;

    /** Altura sobre la zona a la que se colocan los ingredientes emplatados. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    float AlturaPlato = 50.f;

    /** Cliente local: pulsa E mirando la zona llevando un ingrediente procesado. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void RequestDepositarPlato(APlayerCocina* Player, AIngrediente* Ing);

    /** Autoritativo (MC): agrega el ingrediente al plato. Lo llama el canal de PlayerCocina. */
    void MC_HandleAgregar(AIngrediente* Ing);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_PlatoActual();

    /** MC: comprueba si PlatoActual coincide con la receta activa y, si si, puntua. */
    void MC_ChequearReceta();

    /** Evento BP para que el HUD/efectos reaccionen al cambio del plato. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Cocina")
    void OnPlatoActualizado();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;
};
