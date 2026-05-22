#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cocina/Ingrediente.h"
#include "Estacion.generated.h"

class UFusionActorComponent;
class UStaticMeshComponent;
class APlayerCocina;

UENUM(BlueprintType)
enum class EEstadoEstacion : uint8
{
    Libre       UMETA(DisplayName = "Libre"),       // verde
    Procesando  UMETA(DisplayName = "Procesando"),  // amarillo
    Listo       UMETA(DisplayName = "Listo"),       // rojo
};

UCLASS(Abstract)
class REDESCOCINAUE_API AEstacion : public AActor
{
    GENERATED_BODY()

public:
    AEstacion();

    /** Tipo de ingrediente que esta estacion acepta. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    ETipoIngrediente TipoEsperado = ETipoIngrediente::Carne;

    /** Tiempo (segundos) que dura el procesado autoritativo en el MC. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    float TiempoProcesado = 5.f;

    /** Puntos sumados al GameState cuando termina el procesado individual. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    int32 PuntosOtorgados = 10;

    /** Estado replicado. Solo el MC lo modifica. */
    UPROPERTY(ReplicatedUsing = OnRep_Estado, BlueprintReadOnly, Category = "Cocina")
    EEstadoEstacion Estado = EEstadoEstacion::Libre;

    /** Ingrediente actualmente sobre la estacion. Replicado. */
    UPROPERTY(ReplicatedUsing = OnRep_IngredienteActual, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<AIngrediente> IngredienteActual = nullptr;

    /** NetworkTime() en que arranco el procesado. -1 si no aplica. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cocina")
    double StartTimestamp = -1.0;

    /** Llamado desde el cliente local al pulsar E sobre la estacion con un ingrediente. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void RequestDepositar(APlayerCocina* Player, AIngrediente* Ing);

    /** Llamado desde el cliente local al pulsar E sobre la estacion sin ingrediente, en estado Listo. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void RequestRecoger(APlayerCocina* Player);

    // --- Caminos autoritativos: el MC los ejecuta. Llamados desde PlayerCocina::OnRep_Request
    //     (cuando un cliente no-MC pide la accion) o directamente si el invocador ya es el MC.
    //     No usamos Server RPC: en Fusion shared mode no cruzan la red de forma fiable. ---
    void MC_HandleDepositar(AIngrediente* Ing);
    void MC_HandleRecoger(APlayerCocina* Player);

    /** Progreso [0..1] del procesado. Funciona en cualquier cliente porque usa NetworkTime. */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    float GetProgreso01() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_Estado();
    UFUNCTION() void OnRep_IngredienteActual();

    void UpdateColorByEstado();

    // --- Logica interna del MC ---
    void MC_FinishProcesado();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;
};
