#pragma once

#include "CoreMinimal.h"
#include "RedesCocinaUECharacter.h"
#include "PlayerCocina.generated.h"

class UFusionActorComponent;
class UWidgetComponent;
// class UCocinaInteractor;  // se a�adir� en fase 06

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

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

    UFUNCTION()
    void OnRep_PlayerName();

    UFUNCTION(BlueprintImplementableEvent, Category = "Cocina")
    void OnPlayerNameUpdated(const FString& NewName);

    /** Componente Fusion para replicar este actor por Photon. */
    UPROPERTY(VisibleAnywhere, Category = "Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;

    /** Widget 3D que muestra PlayerName encima de la cabeza. */
    UPROPERTY(VisibleAnywhere, Category = "Cocina|UI")
    TObjectPtr<UWidgetComponent> NameplateComp;

    // Interactor (fase 06) se a�adir� cuando exista UCocinaInteractor

    void RefreshNameplate();
};