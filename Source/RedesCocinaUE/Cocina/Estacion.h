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
    Libre       UMETA(DisplayName = "Libre"),       // verde (vacia)
    Procesando  UMETA(DisplayName = "Procesando"),  // amarillo (algo cocinandose)
    Listo       UMETA(DisplayName = "Listo"),       // rojo (algo para recoger)
};

/**
 * Estacion de cocina con hasta MaxIngredientes huecos. Cada ingrediente se procesa
 * con su PROPIO temporizador. Ownership = MasterClient: solo el MC gestiona los slots.
 * A los clientes se les replican los conteos (color/validacion) y el proximo ingrediente
 * recogible.
 */
UCLASS(Abstract)
class REDESCOCINAUE_API AEstacion : public AActor
{
    GENERATED_BODY()

public:
    AEstacion();

    /** Tipo de ingrediente que esta estacion acepta. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    ETipoIngrediente TipoEsperado = ETipoIngrediente::Carne;

    /** Tiempo (segundos) de procesado de cada ingrediente. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    float TiempoProcesado = 5.f;

    /** Puntos sumados al GameState cuando termina el procesado de un ingrediente. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina")
    int32 PuntosOtorgados = 10;

    /** Capacidad maxima de la estacion (rejilla 2x2 = 4). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina", meta = (ClampMax = "4"))
    int32 MaxIngredientes = 4;

    /** Lado del cubo base sin escalar (el StaticMesh Cube de UE mide 100). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina|Layout")
    float TamCuboBase = 100.f;

    /** Altura extra a la que se posa el ingrediente sobre la cara superior. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cocina|Layout")
    float AlturaIngrediente = 15.f;

    /** Nº de huecos ocupados. Replicado para color y pre-validacion en clientes. */
    UPROPERTY(ReplicatedUsing = OnRep_Conteo, BlueprintReadOnly, Category = "Cocina")
    int32 NumOcupados = 0;

    /** Nº de ingredientes ya procesados (listos para recoger). Replicado. */
    UPROPERTY(ReplicatedUsing = OnRep_Conteo, BlueprintReadOnly, Category = "Cocina")
    int32 NumListos = 0;

    /** Proximo ingrediente recogible (primero que esta Listo). Replicado para que el
     *  cliente que recoge pueda tomar ownership de el. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<AIngrediente> ProximoListo = nullptr;

    /** Llamado desde el cliente local al pulsar E con un ingrediente en mano. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void RequestDepositar(APlayerCocina* Player, AIngrediente* Ing);

    /** Llamado desde el cliente local al pulsar E sin ingrediente, con algo Listo. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void RequestRecoger(APlayerCocina* Player);

    /** ¿Puedo depositar un ingrediente de este tipo? (hueco libre + tipo correcto) */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    bool PuedeDepositar(ETipoIngrediente Tipo) const;

    /** ¿Hay algun ingrediente listo para recoger? */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    bool TieneListo() const { return NumListos > 0; }

    // --- Caminos autoritativos: el MC los ejecuta (via canal de PlayerCocina o local). ---
    void MC_HandleDepositar(AIngrediente* Ing);
    void MC_HandleRecoger(APlayerCocina* Player);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_Conteo();

    void UpdateColorByEstado();

    /** Posicion mundial del hueco SlotIndex (rejilla 2x2 sobre la cara superior). */
    FVector SlotWorldLocation(int32 SlotIndex) const;

    /** MC: recalcula NumOcupados/NumListos/ProximoListo a partir de SlotsMC. */
    void RecalcularConteoMC();

    /** Estado solo-color, derivado de los conteos (no replicado directamente). */
    EEstadoEstacion Estado = EEstadoEstacion::Libre;

    /** MC-only: ingrediente por hueco (nullptr = libre). Tamaño = MaxIngredientes. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<AIngrediente>> SlotsMC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;
};
