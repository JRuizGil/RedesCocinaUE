# 07 — Estaciones de cocina: estado replicado y procesado autoritativo

Objetivo §8 del enunciado: tres estaciones (Sartén, Zona de corte, Freidora) con tres estados visuales (verde/libre, amarillo/proceso, rojo/listo). El procesado lo gestiona el **Master Client** únicamente.

| Estación  | Ingrediente esperado | Tiempo | Puntos |
|-----------|---------------------|--------|--------|
| Sartén    | Carne               | 5 s    | 10     |
| Corte     | Lechuga             | 2 s    | 20     |
| Freidora  | Patata              | 10 s   | 30     |

---

## 7.1 Clase base `AEstacion`

### C++: `Estacion.h`

[Source/RedesCocinaUE/Cocina/Estacion.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Source/RedesCocinaUE/Cocina/Estacion.h):

```cpp
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
    Libre       UMETA(DisplayName="Libre"),       // verde
    Procesando  UMETA(DisplayName="Procesando"),  // amarillo
    Listo       UMETA(DisplayName="Listo"),       // rojo
};

UCLASS(Abstract)
class REDESCOCINAUE_API AEstacion : public AActor
{
    GENERATED_BODY()

public:
    AEstacion();

    /** Tipo de ingrediente que esta estación acepta. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cocina")
    ETipoIngrediente TipoEsperado = ETipoIngrediente::Carne;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cocina")
    float TiempoProcesado = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cocina")
    int32 PuntosOtorgados = 10;

    UPROPERTY(ReplicatedUsing=OnRep_Estado, BlueprintReadOnly, Category="Cocina")
    EEstadoEstacion Estado = EEstadoEstacion::Libre;

    /** Ingrediente actualmente sobre la estación. Replicado. */
    UPROPERTY(ReplicatedUsing=OnRep_IngredienteActual, BlueprintReadOnly, Category="Cocina")
    TWeakObjectPtr<AIngrediente> IngredienteActual;

    /** NetworkTime() en que arrancó el procesado. Permite reconstruir el progreso en cualquier cliente. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cocina")
    double StartTimestamp = -1.0;

    // --- Llamado desde el cliente al pulsar E con un ingrediente en mano ---

    /** El jugador depositó un ingrediente. Validación y procesado lo hace el MC vía SendRpc. */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    void RequestDepositar(APlayerCocina* Player, AIngrediente* Ing);

    /** El jugador quiere recoger el producto terminado. */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    void RequestRecoger(APlayerCocina* Player);

    /** Devuelve [0..1] progreso del procesado. Funciona en cualquier cliente. */
    UFUNCTION(BlueprintPure, Category="Cocina")
    float GetProgreso01() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    UFUNCTION() void OnRep_Estado();
    UFUNCTION() void OnRep_IngredienteActual();

    void UpdateColorByEstado();

    // --- Métodos autoritativos: solo en MC ---
    void MC_TryStartProcesado(AIngrediente* Ing);
    void MC_FinishProcesado();
    void MC_TryEntregarAJugador(APlayerCocina* Player);

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UFusionActorComponent> FusionComp;
};
```

### C++: `Estacion.cpp`

```cpp
#include "Cocina/Estacion.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AEstacion::AEstacion()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::MasterClient;
}

void AEstacion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AEstacion, Estado);
    DOREPLIFETIME(AEstacion, IngredienteActual);
    DOREPLIFETIME(AEstacion, StartTimestamp);
}

void AEstacion::BeginPlay()
{
    Super::BeginPlay();
    UpdateColorByEstado();
}

void AEstacion::Tick(float Dt)
{
    Super::Tick(Dt);

    // Solo el MC progresa el contador.
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado != EEstadoEstacion::Procesando) return;
    if (StartTimestamp < 0) return;

    const double Elapsed = Fusion->NetworkTime() - StartTimestamp;
    if (Elapsed >= TiempoProcesado)
    {
        MC_FinishProcesado();
    }
}

void AEstacion::RequestDepositar(APlayerCocina* Player, AIngrediente* Ing)
{
    if (!Player || !Ing) return;

    // Envío de intent al MC vía custom RPC. Id = 1 = "depositar".
    // El payload identifica al ingrediente por su NetworkObjectId; simplificación: serializamos el FName.
    // Para esta práctica, una alternativa más simple: hacer la validación local (lectura no autoritativa)
    // y dejar que el MC valide al recibir el ownership del ingrediente.

    // Camino simple: pedir ownership del ingrediente al MC y, una vez en sus manos, MC arranca procesado.
    // Pero como el MC ya recibe la replicación del Holder del ingrediente, podemos hacerlo más declarativo:

    // Suelta el ingrediente sobre la estación → Ingrediente.Holder = null + posición = socket de la estación.
    Ing->RequestDrop(Player);
    Ing->SetActorLocation(GetActorLocation() + FVector(0, 0, 50));

    // El MC ve por replicación que el ingrediente está suelto encima de la estación y arranca.
    // Para evitar ambigüedad, podemos forzar via RPC explícito:
    UFusionOnlineSubsystem::SendRpc(this, /*RpcId=*/1, /*Data=*/{});
    // Nota: el MC recibirá este RPC y en ProcessCustomRpc decidirá. Detalle en 7.3.
}

void AEstacion::RequestRecoger(APlayerCocina* Player)
{
    if (!Player) return;
    if (Estado != EEstadoEstacion::Listo) return;
    UFusionOnlineSubsystem::SendRpc(this, /*RpcId=*/2, {});
}

void AEstacion::MC_TryStartProcesado(AIngrediente* Ing)
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado != EEstadoEstacion::Libre) return;
    if (!Ing || Ing->Tipo != TipoEsperado) return;
    if (Ing->Estado == EEstadoIngrediente::Procesado) return; // ya procesado

    IngredienteActual = Ing;
    StartTimestamp = Fusion->NetworkTime();
    Estado = EEstadoEstacion::Procesando;
    OnRep_Estado();
    OnRep_IngredienteActual();
}

void AEstacion::MC_FinishProcesado()
{
    Estado = EEstadoEstacion::Listo;
    if (IngredienteActual.IsValid())
    {
        IngredienteActual->SetProcesadoMC();
    }
    OnRep_Estado();

    // Sumar puntos por procesado individual.
    if (AGameStateCocina* GS = GetWorld()->GetGameState<AGameStateCocina>())
    {
        GS->SumarPuntosMC(PuntosOtorgados);
    }
}

void AEstacion::MC_TryEntregarAJugador(APlayerCocina* Player)
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado != EEstadoEstacion::Listo) return;
    if (!IngredienteActual.IsValid()) return;

    // Reasigna ownership del ingrediente al jugador.
    AIngrediente* Ing = IngredienteActual.Get();
    UFusionOnlineSubsystem::SetWantsOwner(Ing, false); // libera ownership MC
    Ing->Holder = Player;   // el cliente del Player tomará attach via OnRep_Holder

    IngredienteActual = nullptr;
    StartTimestamp = -1.0;
    Estado = EEstadoEstacion::Libre;
    OnRep_IngredienteActual();
    OnRep_Estado();
}

float AEstacion::GetProgreso01() const
{
    if (Estado != EEstadoEstacion::Procesando) return 0.f;
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || StartTimestamp < 0) return 0.f;
    const double E = Fusion->NetworkTime() - StartTimestamp;
    return FMath::Clamp(static_cast<float>(E / TiempoProcesado), 0.f, 1.f);
}

void AEstacion::OnRep_Estado() { UpdateColorByEstado(); }
void AEstacion::OnRep_IngredienteActual() {}

void AEstacion::UpdateColorByEstado()
{
    if (!Mesh) return;
    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;
    FLinearColor C = FLinearColor::Green;
    switch (Estado)
    {
        case EEstadoEstacion::Libre:      C = FLinearColor(0.2f, 0.9f, 0.2f); break;
        case EEstadoEstacion::Procesando: C = FLinearColor(1.0f, 0.85f, 0.0f); break;
        case EEstadoEstacion::Listo:      C = FLinearColor(0.9f, 0.15f, 0.15f); break;
    }
    MID->SetVectorParameterValue(TEXT("BaseColor"), C);
}
```

---

## 7.2 Tres subclases concretas

Sárten, Corte y Freidora son la misma clase con valores distintos. Puedes hacerlo con tres `BP_Estacion_*` derivados de `BP_Estacion` (que a su vez deriva de `AEstacion`) y configurar `TipoEsperado`, `TiempoProcesado`, `PuntosOtorgados` en cada uno desde el editor. No hace falta crear tres clases C++ separadas.

| Blueprint | TipoEsperado | Tiempo | Puntos |
|-----------|--------------|--------|--------|
| `BP_Estacion_Sarten`   | Carne   | 5  | 10 |
| `BP_Estacion_Corte`    | Lechuga | 2  | 20 |
| `BP_Estacion_Freidora` | Patata  | 10 | 30 |

Coloca una de cada en `LV_Cocina`.

---

## 7.3 Recibir el custom RPC en el MC

El método `UFusionOnlineSubsystem::SendRpc(Actor, Id, Data)` ([FusionOnlineSubsystem.h:232](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionOnlineSubsystem.h#L232)) envía un RPC custom dirigido al actor. Llega a su owner (en estaciones, el MC). Para procesarlo necesitas que el actor implemente un callback de recepción.

Versión sencilla: en lugar de Custom RPC, usa el patrón clásico de Unreal `UFUNCTION(Server, Reliable)` en un actor cuyo owner sea el MC. Como el `FusionNetDriver` enruta por owner:

```cpp
// AEstacion.h
UFUNCTION(Server, Reliable, BlueprintCallable, Category="Cocina")
void Server_TryDepositar(AIngrediente* Ing);

UFUNCTION(Server, Reliable, BlueprintCallable, Category="Cocina")
void Server_TryRecoger(APlayerCocina* Player);

// AEstacion.cpp
void AEstacion::Server_TryDepositar_Implementation(AIngrediente* Ing) { MC_TryStartProcesado(Ing); }
void AEstacion::Server_TryRecoger_Implementation(APlayerCocina* Player) { MC_TryEntregarAJugador(Player); }
```

Y desde el cliente:

```cpp
void AEstacion::RequestDepositar(APlayerCocina* Player, AIngrediente* Ing)
{
    Ing->RequestDrop(Player); // libera ownership
    Server_TryDepositar(Ing); // llega al MC, que es owner de la estación
}

void AEstacion::RequestRecoger(APlayerCocina* Player)
{
    Server_TryRecoger(Player);
}
```

> Este patrón funciona porque `Ownership = MasterClient` en la estación, así que `Server_*` se ejecuta en el MC, no en el cliente solicitante. **Validado por el FusionNetDriver** del plugin v1214.

---

## 7.4 Integración con el `UCocinaInteractor`

En `UCocinaInteractor::OnInteractPressed`, extender el flujo:

```cpp
void UCocinaInteractor::OnInteractPressed()
{
    APlayerCocina* OwnerPawn = Cast<APlayerCocina>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

    AIngrediente* Held = GetHeldIngrediente();

    // 1) Si estoy mirando una Estacion:
    if (AEstacion* Est = Cast<AEstacion>(ActorEnfocado))
    {
        if (Held)
        {
            Est->RequestDepositar(OwnerPawn, Held);
        }
        else if (Est->Estado == EEstadoEstacion::Listo)
        {
            Est->RequestRecoger(OwnerPawn);
        }
        return;
    }

    // 2) Si llevo algo en mano y NO miro estación → soltar.
    if (Held) { Held->RequestDrop(OwnerPawn); return; }

    // 3) Si miro un ingrediente suelto → recoger.
    if (AIngrediente* Ing = Cast<AIngrediente>(ActorEnfocado))
    {
        Ing->RequestPickup(OwnerPawn);
    }
}
```

---

## 7.5 Smoke test

1. Coloca 3 estaciones en el mapa con tipos distintos.
2. Coge una carne, acércate al Sartén, mira y pulsa E.
3. La carne se posiciona sobre el Sartén; la estación cambia a **amarillo**.
4. Tras 5s la estación cambia a **rojo**, la carne pasa a verde (procesada) y el HUD muestra +10 puntos (lo conectamos en fase 09).
5. Acércate, pulsa E → recoges la carne procesada. La estación vuelve a **verde**.
6. **Test cooperativo**: Cliente A deposita, Cliente B recoge tras los 5s. Funciona porque el ingrediente cambia de owner via `Holder` replicado.

---

## 7.6 Edge cases

- **Depositar tipo equivocado**: el MC ignora silenciosamente (no crashea). Mejora UX: añade RPC de respuesta "Wrong type" y un PrintString al cliente.
- **Depositar mientras está procesando**: `Estado != Libre` → MC rechaza.
- **Recoger antes de tiempo**: `Estado != Listo` → MC rechaza.
- **El jugador que depositó se va antes de completar**: como el ingrediente está bajo ownership MC (lo configura `MC_TryStartProcesado` implícitamente al referenciarlo desde un actor MC), no pasa nada. El MC sigue procesando.

---

## 7.7 Checklist de fin de fase

- [ ] Las 3 estaciones existen en `LV_Cocina`.
- [ ] Colores verde/amarillo/rojo cambian sincronizados en ambos clientes.
- [ ] Cualquier cliente puede iniciar el procesado depositando.
- [ ] La cuenta de tiempo es coherente en ambos clientes (usa `GetProgreso01`).
- [ ] Solo el MC modifica `Estado`, `StartTimestamp` e `IngredienteActual` (verificable poniendo logs en `MC_*`).

---

> **Próxima fase:** [08-Recetas-Emplatado.md](08-Recetas-Emplatado.md) — sistema de recetas, zona de emplatado y composición final.
