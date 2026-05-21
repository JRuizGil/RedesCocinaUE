#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CocinaInteractor.generated.h"

class AIngrediente;
class APlayerCocina;

UCLASS(ClassGroup = (Cocina), meta = (BlueprintSpawnableComponent))
class REDESCOCINAUE_API UCocinaInteractor : public UActorComponent
{
    GENERATED_BODY()

public:
    UCocinaInteractor();

    /** Distancia maxima del raycast frontal. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina")
    float TraceDistance = 200.f;

    /** Altura sobre el actor desde la que parte el trace (negativo = bajo el centro). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina")
    float TraceHeightOffset = -30.f;

    /** Radio del sphere trace. Un valor mayor hace la deteccion mas tolerante. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cocina")
    float TraceRadius = 40.f;

    /** Llamar desde el binding de IA_Interact (tecla E). */
    UFUNCTION(BlueprintCallable, Category = "Cocina")
    void OnInteractPressed();

    /** Ingrediente actualmente en mano del owner del componente. */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    AIngrediente* GetHeldIngrediente() const;

    /** Actor actualmente enfocado por el raycast. */
    UFUNCTION(BlueprintPure, Category = "Cocina")
    AActor* GetActorEnfocado() const { return ActorEnfocado.Get(); }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> ActorEnfocado;

    void UpdateActorEnfocado();
};
