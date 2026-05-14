# 06 — Recoger y soltar ingredientes (interacción E)

Objetivo §8 del enunciado: la tecla `E` recoge el objeto enfrente del jugador, lo posiciona delante mientras se mueve, y al volver a pulsar `E` lo suelta. La sincronización se hace con **transacción de ownership** del ingrediente.

---

## 6.1 Diseño

Tres clases nuevas:

- `AIngrediente` — Actor con mesh, type enum (Carne/Lechuga/Patata), `UFusionActorComponent` modo `Transaction`. Estado replicado: `bProcesado` (raw vs procesado) + opcionalmente índice de receta a la que pertenece.
- `UCocinaInteractor` — Componente colocado en `APlayerCocina` que detecta el actor "mirado" y arbitra el RPC de pickup.
- `IInteractuable` — Interfaz simple con `TryPickup(APlayerCocina*)`, `TryDrop(APlayerCocina*)`, `OnHoverEnter/Exit`. La implementan `AIngrediente` y (parcial) `AEstacion`.

Flujo:

```
Cliente local                                        Master Client (o owner actual del ingrediente)
-------------                                        -----------------------------------------------
1. Pulsa E
2. CocinaInteractor::ServerTryInteract()
   (UFUNCTION(Server, Reliable) sobre el Pawn,
    que el FusionNetDriver enruta al owner = yo)
3. Como yo soy owner del Pawn, ejecuto local:
   - Si tengo algo en mano: TryDrop
   - Si no: raycast → AIngrediente → SetWantsOwner(true)
4. SetWantsOwner abre la transacción de ownership.
   Fusion la concede en próximos 1-2 ticks (RTT).
5. Cuando OnOwnerWasGiven dispara en el cliente:
   - Attach el ingrediente al socket "Hand_R"
   - Replica bAttached = true → resto ve attach por OnRep
```

---

## 6.2 `AIngrediente`

### C++: `Ingrediente.h`

[Source/RedesCocinaUE/Cocina/Ingrediente.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/Ingrediente.h):

```cpp
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
    Carne   UMETA(DisplayName="Carne"),
    Lechuga UMETA(DisplayName="Lechuga"),
    Patata  UMETA(DisplayName="Patata"),
};

UENUM(BlueprintType)
enum class EEstadoIngrediente : uint8
{
    Crudo     UMETA(DisplayName="Crudo"),
    Procesado UMETA(DisplayName="Procesado"),
};

UCLASS()
class REDESCOCINAUE_API AIngrediente : public AActor
{
    GENERATED_BODY()

public:
    AIngrediente();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cocina")
    ETipoIngrediente Tipo = ETipoIngrediente::Carne;

    UPROPERTY(ReplicatedUsing=OnRep_Estado, BlueprintReadOnly, Category="Cocina")
    EEstadoIngrediente Estado = EEstadoIngrediente::Crudo;

    /** Pawn que lo lleva, o nullptr si está suelto. Replicado. */
    UPROPERTY(ReplicatedUsing=OnRep_Holder, BlueprintReadOnly, Category="Cocina")
    TWeakObjectPtr<APlayerCocina> Holder;

    /** Llamado desde el cliente que quiere recoger. NO autoritativo todavía:
     *  solo dispara la transacción de ownership. La validación final ocurre tras
     *  el OnOwnerWasGiven o tras un RPC al MC para casos exclusivos. */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    bool RequestPickup(APlayerCocina* Requester);

    UFUNCTION(BlueprintCallable, Category="Cocina")
    bool RequestDrop(APlayerCocina* Requester);

    /** Solo MC. Marca como procesado tras pasar por la estación. */
    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void SetProcesadoMC();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    UFUNCTION() void OnRep_Estado();
    UFUNCTION() void OnRep_Holder();
    UFUNCTION() void HandleOwnerGiven();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UFusionActorComponent> FusionComp;

    void AttachToHolder();
    void DetachFromHolder();
    void UpdateVisualByEstado();
};
```

### C++: `Ingrediente.cpp`

```cpp
#include "Cocina/Ingrediente.h"
#include "Cocina/PlayerCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AIngrediente::AIngrediente()
{
    bReplicates = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::Transaction;
}

void AIngrediente::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AIngrediente, Estado);
    DOREPLIFETIME(AIngrediente, Holder);
}

void AIngrediente::BeginPlay()
{
    Super::BeginPlay();
    if (FusionComp)
    {
        FusionComp->OnOwnerWasGiven.AddDynamic(this, &AIngrediente::HandleOwnerGiven);
    }
    UpdateVisualByEstado();
}

bool AIngrediente::RequestPickup(APlayerCocina* Requester)
{
    if (!Requester) return false;
    if (Holder.IsValid()) return false; // ya cogido

    // Distancia mínima. Validación blanda en cliente; el resultado real lo da la transacción.
    const float Dist = FVector::Dist(GetActorLocation(), Requester->GetActorLocation());
    if (Dist > 200.f) return false;

    // Pide ownership. Cuando el plugin nos lo conceda, HandleOwnerGiven se ejecuta.
    UFusionOnlineSubsystem::SetWantsOwner(this, true);
    return true;
}

bool AIngrediente::RequestDrop(APlayerCocina* Requester)
{
    if (!UFusionOnlineSubsystem::IsOwner(this)) return false;
    if (Holder.Get() != Requester) return false;

    Holder = nullptr;
    OnRep_Holder();
    UFusionOnlineSubsystem::SetWantsOwner(this, false);
    return true;
}

void AIngrediente::HandleOwnerGiven()
{
    // Soy el nuevo owner. Si fui yo quien lo pidió, attach al pawn local.
    APlayerCocina* MyPawn = Cast<APlayerCocina>(GetWorld()->GetFirstPlayerController()->GetPawn());
    if (!MyPawn) return;
    Holder = MyPawn;
    OnRep_Holder();
}

void AIngrediente::SetProcesadoMC()
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado == EEstadoIngrediente::Procesado) return;
    Estado = EEstadoIngrediente::Procesado;
    OnRep_Estado();
}

void AIngrediente::OnRep_Estado()
{
    UpdateVisualByEstado();
}

void AIngrediente::OnRep_Holder()
{
    if (Holder.IsValid()) AttachToHolder();
    else DetachFromHolder();
}

void AIngrediente::AttachToHolder()
{
    if (!Holder.IsValid()) return;
    AttachToComponent(Holder->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("Hand_R"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AIngrediente::DetachFromHolder()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AIngrediente::UpdateVisualByEstado()
{
    if (!Mesh) return;
    // Cambia color/material: enunciado §8 acepta cambio de color como feedback.
    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;
    const FLinearColor Col = (Estado == EEstadoIngrediente::Procesado) ? FLinearColor(0.2f, 0.9f, 0.2f) : FLinearColor::White;
    MID->SetVectorParameterValue(TEXT("BaseColor"), Col);
}
```

> ⚠️ El socket `Hand_R` debe existir en el SK_Manny. Si no lo tiene, crea un Socket en la mano derecha del esqueleto (en el editor de skeleton) — toma 30 segundos.

---

## 6.3 `UCocinaInteractor`

Componente en el `APlayerCocina` que arbitra el "qué tengo delante".

### C++: `CocinaInteractor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CocinaInteractor.generated.h"

class AIngrediente;
class APlayerCocina;

UCLASS(ClassGroup=(Cocina), meta=(BlueprintSpawnableComponent))
class REDESCOCINAUE_API UCocinaInteractor : public UActorComponent
{
    GENERATED_BODY()

public:
    UCocinaInteractor();

    /** Llamar desde IA_Interact (tecla E). */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    void OnInteractPressed();

    /** Ingrediente actualmente en mano, si lo hay. */
    UFUNCTION(BlueprintPure, Category="Cocina")
    AIngrediente* GetHeldIngrediente() const;

protected:
    virtual void TickComponent(float, ELevelTick, FActorComponentTickFunction*) override;
    virtual void BeginPlay() override;

    UPROPERTY() TObjectPtr<AActor> ActorEnfocado;

    void UpdateActorEnfocado();
};
```

### C++: `CocinaInteractor.cpp` (extracto clave)

```cpp
void UCocinaInteractor::OnInteractPressed()
{
    APlayerCocina* OwnerPawn = Cast<APlayerCocina>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

    // Si llevo algo en mano → soltar.
    if (AIngrediente* Held = GetHeldIngrediente())
    {
        Held->RequestDrop(OwnerPawn);
        return;
    }

    // Si estoy mirando un ingrediente suelto → coger.
    if (AIngrediente* Ing = Cast<AIngrediente>(ActorEnfocado))
    {
        Ing->RequestPickup(OwnerPawn);
        return;
    }

    // Si estoy mirando una estación con un ingrediente en mano → depositar.
    // (Detalle en fase 07.)
}

void UCocinaInteractor::UpdateActorEnfocado()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;
    const FVector Start = OwnerActor->GetActorLocation();
    const FVector Fwd = OwnerActor->GetActorForwardVector();
    const FVector End = Start + Fwd * 200.f;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerActor);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        ActorEnfocado = Hit.GetActor();
    }
    else
    {
        ActorEnfocado = nullptr;
    }
}

AIngrediente* UCocinaInteractor::GetHeldIngrediente() const
{
    APlayerCocina* Pawn = Cast<APlayerCocina>(GetOwner());
    if (!Pawn) return nullptr;
    for (TActorIterator<AIngrediente> It(GetWorld()); It; ++It)
    {
        if (It->Holder.Get() == Pawn) return *It;
    }
    return nullptr;
}
```

> Para optimizar `GetHeldIngrediente`, guarda una referencia local actualizada en `OnRep_Holder` del ingrediente (notificar al pawn). Aquí está la versión sencilla.

### Conectar al Pawn

En `APlayerCocina::APlayerCocina()`:

```cpp
Interactor = CreateDefaultSubobject<UCocinaInteractor>(TEXT("Interactor"));
```

### Input

1. Crea `IA_Interact` (Boolean) en `Content/Input/Actions/`.
2. En `IMC_Default` añade el mapping: tecla `E` → `IA_Interact`.
3. En `BP_PlayerCocina` o en `RedesCocinaUEPlayerController`:
   ```
   On IA_Interact Triggered → Get Owner Pawn → Get Cocina Interactor → On Interact Pressed
   ```
   o en C++ en `APlayerCocina::SetupPlayerInputComponent`:
   ```cpp
   EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &APlayerCocina::OnInteractPressed);
   ```

---

## 6.4 Por qué transacción de ownership y no RPC al MC

Podrías hacer un RPC al MC: "quiero coger este X". El MC valida y replica. **Funciona**, pero:

- El RTT al MC se nota mucho (200-300ms WAN) → "lag" al pulsar E.
- Si el MC se desconecta justo al validar, pierdes la acción.

Con `SetWantsOwner` el plugin gestiona la transferencia entre owners actuales (que pueden ser otros jugadores) sin pasar siempre por el MC. Es el patrón canónico de Fusion Shared para objetos transferibles.

**Excepción:** si dos jugadores piden el mismo ingrediente con <50ms de diferencia, hay carrera. El plugin la resuelve internamente (uno gana, otro recibe rechazo). Tu UI debe reflejar `if (!Holder.IsValid()) /* aún puedo intentarlo */`.

---

## 6.5 Smoke test

1. Spawn manual (o spawner — fase 07) de 3 ingredientes en el mapa.
2. Cliente A se acerca, pulsa E → coge.
3. Cliente B en su pantalla ve que el ingrediente sigue al Cliente A (replicación del attach).
4. Cliente A se mueve por el mapa con el ingrediente "en mano".
5. Cliente A pulsa E de nuevo → suelta. Ambos lo ven caer en su sitio actual.
6. Cliente B se acerca al ingrediente suelto, pulsa E → lo recoge sin problemas.

---

## 6.6 Spawner de ingredientes (opcional ahora, recomendado)

Un `ASpawner_Ingrediente` con `Ownership = MasterClient`:

- Mantiene una lista de slots con `AIngrediente*`.
- En `BeginPlay` (si soy MC), spawnea N por tipo.
- Cuando un ingrediente cambia a `Procesado` y se consume en emplatado (fase 08), el spawner instancia otro tras 1-2s para no quedarse sin material.

Más detalle en [08-Recetas-Emplatado.md](08-Recetas-Emplatado.md#spawner-de-ingredientes).

---

## 6.7 Checklist de fin de fase

- [ ] `AIngrediente` replica `Holder` y `Estado`.
- [ ] Pulsar E sobre un ingrediente lo recoge en <200ms WAN.
- [ ] Pulsar E con un ingrediente en mano lo suelta.
- [ ] El otro cliente ve el ingrediente attachado al Pawn que lo recogió.
- [ ] No se puede coger un ingrediente que ya tiene Holder.
- [ ] El nameplate del jugador y el ingrediente se mueven juntos sin desfase.

---

> **Próxima fase:** [07-Estaciones-Procesado.md](07-Estaciones-Procesado.md) — estaciones de cocina con estado replicado y procesado autoritativo del MC.
