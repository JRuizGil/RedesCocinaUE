#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ingrediente.generated.h"

class UFusionActorComponent;
class UStaticMeshComponent;
class APlayerCocina;

UENUM(BlueprintType)
enum class ETipoIngrediente : uint8
{
    Carne   UMETA(DisplayName = "Carne"),
    Lechuga UMETA(DisplayName = "Lechuga"),
    Patata  UMETA(DisplayName = "Patata"),
};

UENUM(BlueprintType)
enum class EEstadoIngrediente : uint8
{
    Crudo     UMETA(DisplayName = "Crudo"),
    Procesado UMETA(DisplayName = "Procesado"),
};

UCLASS()
class REDESCOCINAUE_API AIngrediente : public AActor
{
    GENERATED_BODY()

public:
    AIngrediente();

    /** Tipo de ingrediente (Carne / Lechuga / Patata). Editable por BP derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    ETipoIngrediente Tipo = ETipoIngrediente::Carne;

    /** Estado actual. Lo modifica el Master Client tras procesado en una estacion. */
    UPROPERTY(ReplicatedUsing = OnRep_Estado, BlueprintReadOnly, Category = "Cocina")
    EEstadoIngrediente Estado = EEstadoIngrediente::Crudo;

    /** Pawn que lo lleva en la mano. nullptr = suelto en el mundo. */
    UPROPERTY(ReplicatedUsing = OnRep_Holder, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<APlayerCocina> Holder = nullptr;

    /** true cuando ya esta colocado en el plato (zona de emplatado). No se puede recoger. */
    UPROPERTY(ReplicatedUsing = OnRep_EnPlato, BlueprintReadOnly, Category = "Cocina")
    bool bEnPlato = false;

    /** true mientras esta posado en una estacion (cocinandose o listo). Apaga colision
     *  para que el trace del jugador detecte la estacion y no el ingrediente. */
    UPROPERTY(ReplicatedUsing = OnRep_EnEstacion, BlueprintReadOnly, Category = "Cocina")
    bool bEnEstacion = false;

    /** NetworkTime en que empezo a procesarse en una estacion. -1 si no aplica. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cocina")
    double ProcInicio = -1.0;

    /** Duracion del procesado (segundos) fijada por la estacion al depositarlo. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cocina")
    float ProcDuracion = 0.f;

    /** Socket en el SkeletalMesh del Pawn al que se attacha el ingrediente. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina")
    FName HandSocketName = TEXT("hand_r");

    /** Color visual cuando el ingrediente esta procesado. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina|FX")
    FLinearColor ColorProcesado = FLinearColor(0.2f, 0.9f, 0.2f);

    /** Color visual cuando esta crudo. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina|FX")
    FLinearColor ColorCrudo = FLinearColor::White;

    /** Distancia maxima desde la que un jugador puede iniciar pickup. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina")
    float MaxPickupDistance = 300.f;

    /** Pide ownership al plugin Fusion. Llamar desde el cliente local del jugador. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    bool RequestPickup(APlayerCocina* Requester);

    /** Suelta el ingrediente. Solo el actual Holder puede soltarlo. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    bool RequestDrop(APlayerCocina* Requester);

    /** Llamado por la estacion (Master Client) cuando termina el procesado. */
    UFUNCTION(BlueprintCallable, Category = "Cocina|MC")
    void SetProcesadoMC();

    /** Llamado por la zona de emplatado (Master Client) al colocarlo en el plato.
     *  El MC (owner) desengancha de la mano, posiciona y marca emplatado de forma
     *  autoritativa, asi el detach replica a todas las vistas (no solo a quien empla). */
    void SetEnPlatoMC(const FVector& Loc);

    /** Llamado por la estacion (MC) al posarlo en un slot: posicion, rotacion y timer. */
    void SetEnEstacionMC(const FVector& Loc, const FRotator& Rot, float Duracion, double Inicio);

    /** Llamado por la estacion (MC) al entregar el ingrediente listo a un jugador.
     *  El MC es owner del ingrediente, asi que su escritura de Holder SI replica a todos. */
    void EntregarAJugadorMC(APlayerCocina* NewHolder);

    /** Progreso [0..1] del procesado de ESTE ingrediente. Funciona en cualquier cliente. */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    float GetProgresoProcesado01() const;

    UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_Estado();
    UFUNCTION() void OnRep_Holder();
    UFUNCTION() void OnRep_EnPlato();
    UFUNCTION() void OnRep_EnEstacion();

    /** Se invoca cuando Fusion nos concede ownership tras un SetWantsOwner(true). Re-afirma
     *  Holder para que el pickup local se vea en el resto de clientes (guard abajo). */
    UFUNCTION() void HandleOwnerGiven();

    /** Actualiza color/material segun Estado. */
    UFUNCTION(BlueprintCallable, Category = "Cocina|FX")
    void UpdateVisualByEstado();

    void AttachToHolder();
    void DetachFromHolder();

    /** true entre el pickup local y la concesion de ownership; lo consume HandleOwnerGiven.
     *  No replicado: es intencion local del cliente que coge. */
    bool bPendingLocalPickup = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;
};
