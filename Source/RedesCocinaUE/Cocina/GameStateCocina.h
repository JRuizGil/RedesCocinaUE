#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameStateCocina.generated.h"

class URecetaAsset;

UENUM(BlueprintType)
enum class EEstadoPartida : uint8
{
    Esperando UMETA(DisplayName="Esperando"),
    EnCurso   UMETA(DisplayName="En curso"),
    Finalizada UMETA(DisplayName="Finalizada")
};

UCLASS()
class REDESCOCINAUE_API AGameStateCocina : public AGameStateBase
{
    GENERATED_BODY()

public:
    AGameStateCocina();

    UPROPERTY(ReplicatedUsing=OnRep_StartTimestamp, BlueprintReadOnly, Category="Cocina")
    double StartTimestamp = -1.0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cocina")
    float DuracionPartida = 120.f;

    UPROPERTY(ReplicatedUsing=OnRep_EstadoPartida, BlueprintReadOnly, Category="Cocina")
    EEstadoPartida EstadoPartida = EEstadoPartida::Esperando;

    UPROPERTY(ReplicatedUsing=OnRep_Puntuacion, BlueprintReadOnly, Category="Cocina")
    int32 Puntuacion = 0;

    UPROPERTY(ReplicatedUsing=OnRep_RecetaActiva, BlueprintReadOnly, Category="Cocina")
    int32 RecetaActivaIndex = -1;

    /** Catalogo de recetas disponibles. Poblar en el BP derivado del GameState. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocina")
    TArray<TObjectPtr<URecetaAsset>> Recetas;

    /** Receta apuntada por RecetaActivaIndex (nullptr si el indice no es valido). */
    UFUNCTION(BlueprintPure, Category="Cocina")
    URecetaAsset* GetRecetaActiva() const;

    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void IniciarPartidaMC();

    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void SumarPuntosMC(int32 Delta);

    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void SetRecetaActivaMC(int32 NewIndex);

    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void FinalizarPartidaMC();

    UFUNCTION(BlueprintPure, Category="Cocina")
    float GetTiempoRestante() const;

protected:
    UFUNCTION() void OnRep_StartTimestamp();
    UFUNCTION() void OnRep_EstadoPartida();
    UFUNCTION() void OnRep_Puntuacion();
    UFUNCTION() void OnRep_RecetaActiva();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool EnsureMC(const TCHAR* Op) const;
};
