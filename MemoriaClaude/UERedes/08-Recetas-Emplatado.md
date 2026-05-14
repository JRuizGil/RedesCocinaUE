# 08 — Recetas y zona de emplatado

Objetivo §8 / §15: implementar al menos **una receta** de 3 ingredientes que requieren procesado individual. La receta se completa al combinar los 3 procesados en la zona de emplatado, momento en que se otorgan puntos y opcionalmente se genera una nueva receta.

---

## 8.1 Datos de receta

### `URecetaAsset` (PrimaryDataAsset)

[Source/RedesCocinaUE/Cocina/RecetaAsset.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/RecetaAsset.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Cocina/Ingrediente.h"
#include "RecetaAsset.generated.h"

UCLASS(BlueprintType)
class REDESCOCINAUE_API URecetaAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    /** Los ingredientes necesarios, deben estar todos en estado Procesado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<ETipoIngrediente> Ingredientes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 PuntosCompletar = 50;
};
```

Crea un asset `DA_Receta_Hamburguesa` con:
- `Ingredientes = [Carne, Lechuga, Patata]`
- `PuntosCompletar = 50`
- `DisplayName = "Hamburguesa con patatas"`

> §15 pide solo 1 receta. Crea más si quieres ampliar (§4 / §15 lo permiten).

---

## 8.2 Catálogo de recetas en `AGameStateCocina`

Añade al GameState (extendiendo lo de fase 04):

```cpp
// GameStateCocina.h
UPROPERTY(EditDefaultsOnly, Category="Cocina")
TArray<TObjectPtr<URecetaAsset>> Recetas;

UFUNCTION(BlueprintPure, Category="Cocina")
URecetaAsset* GetRecetaActiva() const;
```

```cpp
// GameStateCocina.cpp
URecetaAsset* AGameStateCocina::GetRecetaActiva() const
{
    if (RecetaActivaIndex < 0 || RecetaActivaIndex >= Recetas.Num()) return nullptr;
    return Recetas[RecetaActivaIndex];
}
```

En `BP_CocinaGameMode → BP_GameStateCocina` (el blueprint derivado del GameState), arrastra `DA_Receta_Hamburguesa` al array `Recetas`.

Modifica `ACocinaGameMode::PostLogin` para arrancar también la receta:

```cpp
// Tras IniciarPartidaMC dentro del lambda del timer:
G->SetRecetaActivaMC(0);
```

Y conecta `OnRep_RecetaActiva` en el GameState a un evento BP `OnRecetaActualizada` que el HUD escuche.

---

## 8.3 Zona de emplatado: `AZonaEmplatado`

Es un trigger box con ownership MC. Mantiene una lista replicada de ingredientes "colocados" en el plato. Cuando coincide con la receta activa, suma puntos y vacía.

### C++: `ZonaEmplatado.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cocina/Ingrediente.h"
#include "ZonaEmplatado.generated.h"

class UBoxComponent;
class UFusionActorComponent;
class APlayerCocina;

UCLASS()
class REDESCOCINAUE_API AZonaEmplatado : public AActor
{
    GENERATED_BODY()

public:
    AZonaEmplatado();

    UPROPERTY(ReplicatedUsing=OnRep_PlatoActual, BlueprintReadOnly, Category="Cocina")
    TArray<ETipoIngrediente> PlatoActual;

    UFUNCTION(BlueprintCallable, Category="Cocina")
    void RequestDepositarPlato(APlayerCocina* Player, AIngrediente* Ing);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    UFUNCTION() void OnRep_PlatoActual();

    UFUNCTION(Server, Reliable)
    void Server_TryAgregarIngrediente(AIngrediente* Ing);

    void MC_TryAgregarIngrediente(AIngrediente* Ing);
    void MC_ChequearReceta();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Trigger;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UFusionActorComponent> FusionComp;
};
```

### C++: `ZonaEmplatado.cpp` (extracto)

```cpp
#include "Cocina/ZonaEmplatado.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "Cocina/RecetaAsset.h"
#include "Components/BoxComponent.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Net/UnrealNetwork.h"

AZonaEmplatado::AZonaEmplatado()
{
    bReplicates = true;
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    SetRootComponent(Trigger);
    Trigger->SetBoxExtent(FVector(80, 80, 50));

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::MasterClient;
}

void AZonaEmplatado::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AZonaEmplatado, PlatoActual);
}

void AZonaEmplatado::RequestDepositarPlato(APlayerCocina* Player, AIngrediente* Ing)
{
    if (!Ing || Ing->Estado != EEstadoIngrediente::Procesado) return;
    // Suelta antes de depositar.
    Ing->RequestDrop(Player);
    Server_TryAgregarIngrediente(Ing);
}

void AZonaEmplatado::Server_TryAgregarIngrediente_Implementation(AIngrediente* Ing)
{
    MC_TryAgregarIngrediente(Ing);
}

void AZonaEmplatado::MC_TryAgregarIngrediente(AIngrediente* Ing)
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (!Ing || Ing->Estado != EEstadoIngrediente::Procesado) return;

    PlatoActual.Add(Ing->Tipo);

    // Mueve el ingrediente a la zona y le quita ownership a quien sea (destrucción opcional).
    Ing->SetActorLocation(GetActorLocation() + FVector(0, 0, 50));
    Ing->SetActorScale3D(FVector(0.5f)); // representación final §8 "reducción de escala"
    Ing->SetActorEnableCollision(false);

    OnRep_PlatoActual();
    MC_ChequearReceta();
}

void AZonaEmplatado::MC_ChequearReceta()
{
    AGameStateCocina* GS = GetWorld()->GetGameState<AGameStateCocina>();
    if (!GS) return;
    URecetaAsset* Receta = GS->GetRecetaActiva();
    if (!Receta) return;

    // Convierte ambos a sets ordenados y compara.
    if (PlatoActual.Num() < Receta->Ingredientes.Num()) return;

    TArray<ETipoIngrediente> Esperados = Receta->Ingredientes;
    TArray<ETipoIngrediente> Tengo = PlatoActual;
    Esperados.Sort([](ETipoIngrediente a, ETipoIngrediente b){ return (uint8)a < (uint8)b; });
    Tengo.Sort([](ETipoIngrediente a, ETipoIngrediente b){ return (uint8)a < (uint8)b; });

    if (Esperados == Tengo)
    {
        GS->SumarPuntosMC(Receta->PuntosCompletar);

        // Limpia plato (destruye o devuelve al pool).
        PlatoActual.Empty();
        OnRep_PlatoActual();

        // Receta completa → fin de partida según §15.
        GS->FinalizarPartidaMC();
        // O alternativa: GS->SetRecetaActivaMC((GS->RecetaActivaIndex + 1) % GS->Recetas.Num());
    }
}

void AZonaEmplatado::OnRep_PlatoActual()
{
    // Hook para que el HUD muestre los ingredientes ya colocados.
}
```

> §15 dice "La partida finaliza cuando se completa la receta o se agota el tiempo". Aquí elegimos finalizar al completar; si quieres bucles infinitos hasta que se agote el tiempo, comenta la línea `FinalizarPartidaMC()` y descomenta el ciclo de receta.

---

## 8.4 Integración con el `UCocinaInteractor`

Extensión del flujo de [06](06-Pickup-Drop.md):

```cpp
void UCocinaInteractor::OnInteractPressed()
{
    APlayerCocina* OwnerPawn = ...;
    AIngrediente* Held = GetHeldIngrediente();

    if (AEstacion* Est = Cast<AEstacion>(ActorEnfocado)) { /* ya cubierto */ return; }

    if (AZonaEmplatado* Zona = Cast<AZonaEmplatado>(ActorEnfocado))
    {
        if (Held) { Zona->RequestDepositarPlato(OwnerPawn, Held); }
        return;
    }

    // ... resto del flujo (pickup/drop ingrediente).
}
```

---

## 8.5 <a id="spawner-de-ingredientes"></a>Spawner de ingredientes (recomendado)

Para que no te quedes sin ingredientes tras emplatar:

### `ASpawnerIngredientes`

- Ownership `MasterClient`.
- En `BeginPlay` (si soy MC): spawnea `N_Carne, N_Lechuga, N_Patata` (3-4 de cada) en posiciones fijas o aleatorias en el mapa.
- Suscríbete a la destrucción/consumo de ingredientes (un delegate del `AZonaEmplatado`) y re-spawnea tras 2s.

Versión mínima:

```cpp
void ASpawnerIngredientes::BeginPlay()
{
    Super::BeginPlay();
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;

    for (int32 i = 0; i < 3; ++i)
    {
        SpawnByTipo(ETipoIngrediente::Carne,   GetActorLocation() + FVector(100*i, 0, 50));
        SpawnByTipo(ETipoIngrediente::Lechuga, GetActorLocation() + FVector(100*i, 100, 50));
        SpawnByTipo(ETipoIngrediente::Patata,  GetActorLocation() + FVector(100*i, 200, 50));
    }
}

AIngrediente* ASpawnerIngredientes::SpawnByTipo(ETipoIngrediente T, FVector Loc)
{
    AIngrediente* Ing = GetWorld()->SpawnActor<AIngrediente>(IngredienteClass, Loc, FRotator::ZeroRotator);
    if (Ing) Ing->Tipo = T;
    return Ing;
}
```

> El `SpawnActor` ejecuta en el MC. El `FusionNetDriver` los replica al resto. Funciona por defecto.

---

## 8.6 Smoke test

1. Inicia partida 2 jugadores. Receta activa: Hamburguesa = Carne+Lechuga+Patata.
2. Jugador A: coge carne → deposita en Sartén → espera 5s → recoge.
3. Mientras tanto, Jugador B: coge lechuga → corte → 2s → recoge.
4. Jugador A: coge patata → freidora → 10s → recoge.
5. Cualquiera: llevar los tres a la `BP_ZonaEmplatado` → deposita uno a uno.
6. Al tercer ingrediente correcto, `PuntosCompletar = 50` se suman al GS.Puntuacion (total esperado: 10+20+30+50 = 110).
7. `EstadoPartida → Finalizada`. HUD reacciona en [09](09-Tiempo-Puntuacion.md) y [10](10-FinPartida-WAN.md).

---

## 8.7 Checklist de fin de fase

- [ ] `URecetaAsset` definido y al menos `DA_Receta_Hamburguesa` en `Content/Cocina/Recetas/`.
- [ ] `AGameStateCocina.Recetas` poblado en el BP derivado.
- [ ] `AZonaEmplatado` en el mapa con su `BoxComponent` visible (Trigger box).
- [ ] Depositar un ingrediente procesado lo añade al `PlatoActual`.
- [ ] Depositar uno crudo es rechazado por el MC.
- [ ] Al completar receta, puntos suben y `EstadoPartida = Finalizada`.

---

> **Próxima fase:** [09-Tiempo-Puntuacion.md](09-Tiempo-Puntuacion.md) — HUD con timer y puntuación sincronizados.
