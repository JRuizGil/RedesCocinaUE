#pragma once

#include "CoreMinimal.h"
#include "RedesCocinaUECharacter.h"
#include "PlayerCocina.generated.h"

class UFusionActorComponent;
class UWidgetComponent;
class UInputMappingContext;
class UInputAction;
class UCocinaInteractor;
class AEstacion;
class AZonaEmplatado;
class AIngrediente;

/** Accion autoritativa que un cliente solicita al Master Client. */
UENUM()
enum class ECocinaAccion : uint8
{
    Ninguna,
    DepositarEstacion,
    RecogerEstacion,
    DepositarPlato,
};

UCLASS()
class REDESCOCINAUE_API APlayerCocina : public ARedesCocinaUECharacter
{
    GENERATED_BODY()

public:
    APlayerCocina();

    /** Nombre del jugador. Solo el propio jugador escribe; replica al resto. */
    UPROPERTY(ReplicatedUsing = OnRep_PlayerName, BlueprintReadOnly, Category = "Cocina")
    FString PlayerName = TEXT("Jugador");

    /** Llamado desde el Pawn local tras posesi�n para empujar el nombre al estado de red. */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void SetMyNameFromGameInstance();

    /** Canal de peticion al MC. El cliente local escribe estas props (que SI posee en
     *  su pawn); el MC las recibe por replicacion y ejecuta la logica autoritativa.
     *  Sustituye a los Server RPC, que no cruzan la red en Fusion shared mode. */
    void SubmitDepositarEstacion(AEstacion* Estacion, AIngrediente* Ing);
    void SubmitRecogerEstacion(AEstacion* Estacion);
    void SubmitDepositarPlato(AZonaEmplatado* Zona, AIngrediente* Ing);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION()
    void OnRep_PlayerName();

    // --- Canal de peticion replicado (cliente -> MC) ---
    /** Actor objetivo (AEstacion o AZonaEmplatado). */
    UPROPERTY(Replicated)
    TObjectPtr<AActor> ReqTarget = nullptr;

    /** Ingrediente implicado (solo en peticiones de deposito). */
    UPROPERTY(Replicated)
    TObjectPtr<AIngrediente> ReqIngrediente = nullptr;

    /** Accion solicitada. */
    UPROPERTY(Replicated)
    ECocinaAccion ReqAccion = ECocinaAccion::Ninguna;

    /** Se incrementa en cada peticion para forzar OnRep aunque el resto no cambie. */
    UPROPERTY(ReplicatedUsing = OnRep_Request)
    int32 ReqNonce = 0;

    /** Se ejecuta en el MC al recibir una nueva peticion. */
    UFUNCTION()
    void OnRep_Request();

    /** Helper comun: fija los campos, incrementa el nonce y ejecuta ya si soy el MC. */
    void SubmitCocinaRequest(AActor* Target, AIngrediente* Ing, ECocinaAccion Accion);

    /** Ejecuta la accion autoritativa segun ReqAccion (solo debe llamarse en el MC). */
    void DispatchCocinaRequest();

    UFUNCTION(BlueprintImplementableEvent, Category = "Cocina")
    void OnPlayerNameUpdated(const FString& NewName);

    /** Componente Fusion para replicar este actor por Photon. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;

    /** Widget 3D que muestra PlayerName encima de la cabeza. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina|UI")
    TObjectPtr<UWidgetComponent> NameplateComp;

    /** Mapping Contexts registrados al cliente local en BeginPlay.
     *  Configurar en BP con al menos IMC_Default e IMC_MouseLook. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina|Input")
    TArray<TObjectPtr<UInputMappingContext>> MappingContexts;

    /** Prioridad aplicada a todos los mapping contexts (mayor = gana en conflictos). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina|Input")
    int32 MappingPriority = 0;

    /** Accion de interaccion (tecla E). Asignar IA_Interact en Class Defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina|Input")
    TObjectPtr<UInputAction> InteractAction;

    /** Componente que arbitra pickup/drop/uso de estaciones. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cocina")
    TObjectPtr<UCocinaInteractor> Interactor;

    /** Handler bindeado a IA_Interact. */
    void OnInteractTriggered();

    void RefreshNameplate();
};