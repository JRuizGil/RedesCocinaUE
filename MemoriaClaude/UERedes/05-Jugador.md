# 05 — Personaje del jugador, nombre replicado y nameplate

Objetivo: implementar el §6.2 (nombre en estado de red) y §8 (control con WASD + ratón) usando la base de `RedesCocinaUECharacter` (ThirdPerson template) y añadiendo replicación Fusion.

---

## 5.1 Clase base: `APlayerCocina`

Hereda de `ARedesCocinaUECharacter` (que ya viene del template) para no duplicar el movimiento, animación, cámara y Enhanced Input.

### C++: `PlayerCocina.h`

[Source/RedesCocinaUE/Cocina/PlayerCocina.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/PlayerCocina.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "RedesCocinaUECharacter.h"
#include "PlayerCocina.generated.h"

class UFusionActorComponent;
class UWidgetComponent;
class UCocinaInteractor;     // se creará en fase 06

UCLASS()
class REDESCOCINAUE_API APlayerCocina : public ARedesCocinaUECharacter
{
    GENERATED_BODY()

public:
    APlayerCocina();

    /** Nombre del jugador. Solo el propio jugador escribe; replica al resto. */
    UPROPERTY(ReplicatedUsing=OnRep_PlayerName, BlueprintReadOnly, Category="Cocina")
    FString PlayerName = TEXT("Jugador");

    /** Llamado desde el Pawn local tras posesión para empujar el nombre al estado de red. */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    void SetMyNameFromGameInstance();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

    UFUNCTION()
    void OnRep_PlayerName();

    /** Componente Fusion para replicar este actor por Photon. */
    UPROPERTY(VisibleAnywhere, Category="Cocina|Net")
    TObjectPtr<UFusionActorComponent> FusionComp;

    /** Widget 3D que muestra PlayerName encima de la cabeza. */
    UPROPERTY(VisibleAnywhere, Category="Cocina|UI")
    TObjectPtr<UWidgetComponent> NameplateComp;

    /** Lógica de interacción E (pickup/drop, deposito en estación). Fase 06. */
    UPROPERTY(VisibleAnywhere, Category="Cocina")
    TObjectPtr<UCocinaInteractor> Interactor;

    void RefreshNameplate();
};
```

### C++: `PlayerCocina.cpp`

```cpp
#include "Cocina/PlayerCocina.h"
#include "Cocina/CocinaGameInstance.h"
#include "FusionActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"

APlayerCocina::APlayerCocina()
{
    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::Transaction;
    // El owner natural es el jugador que controla este Pawn. Fusion lo gestiona via PlayerController.

    NameplateComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
    NameplateComp->SetupAttachment(RootComponent);
    NameplateComp->SetRelativeLocation(FVector(0, 0, 110));
    NameplateComp->SetWidgetSpace(EWidgetSpace::Screen);
    NameplateComp->SetDrawSize(FVector2D(180, 36));

    bReplicates = true;
}

void APlayerCocina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APlayerCocina, PlayerName);
}

void APlayerCocina::BeginPlay()
{
    Super::BeginPlay();
    RefreshNameplate();
}

void APlayerCocina::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    // Solo el cliente local del jugador empuja su nombre al estado de red.
    if (IsLocallyControlled())
    {
        SetMyNameFromGameInstance();
    }
}

void APlayerCocina::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (IsLocallyControlled())
    {
        SetMyNameFromGameInstance();
    }
}

void APlayerCocina::SetMyNameFromGameInstance()
{
    UCocinaGameInstance* GI = Cast<UCocinaGameInstance>(GetGameInstance());
    if (!GI) return;
    if (PlayerName == GI->PlayerDisplayName) return;
    PlayerName = GI->PlayerDisplayName;
    OnRep_PlayerName(); // refresca local; los remotos lo recibirán por replicación
}

void APlayerCocina::OnRep_PlayerName()
{
    RefreshNameplate();
}

void APlayerCocina::RefreshNameplate()
{
    // Implementación BP: el widget tiene una variable Text "DisplayedName".
    // Aquí podríamos hacer el cast a UUserWidget; lo dejamos al Blueprint derivado para simplicidad.
    if (NameplateComp)
    {
        NameplateComp->SetVisibility(true);
    }
}
```

---

## 5.2 Replicación con FusionNetDriver

Notas importantes que cambian respecto al netcode "vanilla" de Unreal:

- `bReplicates = true` + `DOREPLIFETIME` siguen funcionando: el `FusionNetDriver` enruta exactamente como `IpNetDriver`.
- **Pero la "autoridad de escritura" la decide el `UFusionActorComponent::Ownership`**, no `HasAuthority()`. Si miras `HasAuthority()` en este modo, devuelve true en el jugador owner del pawn, no en el MC.
- Por eso en `SetMyNameFromGameInstance` no hacemos `if (HasAuthority())` sino `if (IsLocallyControlled())`. El owner del Pawn es el jugador local; el dueño escribe.

---

## 5.3 Blueprint derivado: `BP_PlayerCocina`

1. `Content/Cocina/BP_PlayerCocina` derivado de `APlayerCocina`.
2. En el Mesh: usa el `SK_Manny` del template (ya está en `Content/Characters/`).
3. Animation Blueprint: `ABP_Manny` del template.
4. Configurar `NameplateComp` widget class:
   - Crea `WBP_Nameplate` (UserWidget): un Border con TextBlock dentro, `Text` binding a una variable `DisplayedName : String`.
   - En `BP_PlayerCocina`, en `Event OnRep_PlayerName` (expón un evento custom desde C++ con `BlueprintImplementableEvent`, o usa el evento de actualización), haz: `NameplateComp → Get Widget → Cast WBP_Nameplate → Set DisplayedName(PlayerName)`.

> Alternativa más limpia: en `APlayerCocina::OnRep_PlayerName` haz un `BlueprintImplementableEvent OnPlayerNameUpdated(FString NewName)` y en BP refresca el widget. Te ahorras imports de UMG en C++.

```cpp
// En el .h
UFUNCTION(BlueprintImplementableEvent, Category="Cocina")
void OnPlayerNameUpdated(const FString& NewName);

// En el .cpp dentro de OnRep_PlayerName():
OnPlayerNameUpdated(PlayerName);
```

---

## 5.4 GameMode usa el BP

Vuelve a `ACocinaGameMode` y, en lugar de fijar `DefaultPawnClass = APlayerCocina::StaticClass()`, deja que el `BP_CocinaGameMode` apunte a `BP_PlayerCocina`. Más flexible y los settings de mesh/animation se editan visualmente.

---

## 5.5 Smoke test

1. Inicia partida con 2 clientes.
2. Cliente A introduce "Marta" en el menú; Cliente B introduce "Iker".
3. Tras spawn:
   - Marta ve un Pawn con nameplate "Iker" en la otra esquina del mapa.
   - Iker ve un Pawn con nameplate "Marta".
   - Cada uno ve su propio nameplate (puedes ocultar el local si prefieres: `if (IsLocallyControlled()) NameplateComp->SetVisibility(false)`).
4. Mueve con WASD: el otro cliente ve el movimiento replicado fluido (gracias al `CharacterMovementComponent` estándar enrutado por FusionNetDriver).
5. Si fuerzas un cambio de nombre desde la consola en el cliente A (`KISMET CALL FunctionName=SetMyNameFromGameInstance`), el cliente B lo ve actualizado en <1s.

---

## 5.6 Control de input (recordatorio)

El template ya trae:
- `IMC_Default` (Input Mapping Context) en `Content/Input/`.
- `IA_Move`, `IA_Look`, `IA_Jump`.

Para esta práctica añadiremos `IA_Interact` (tecla `E`) en [06-Pickup-Drop.md](06-Pickup-Drop.md). El movimiento ya funciona out-of-the-box; no hay nada que tocar aquí.

---

## 5.7 Checklist de fin de fase

- [ ] `APlayerCocina` compila y `BP_PlayerCocina` derivado existe.
- [ ] `UFusionActorComponent` está en el Pawn con `Ownership = Transaction`.
- [ ] `PlayerName` replica: ambos clientes ven el nombre del otro en el nameplate.
- [ ] El nameplate se posiciona encima de la cabeza y se ve en pantalla (Screen Space).
- [ ] Movimiento WASD replicado correctamente (sin saltos extraños tras 1-2 RTT).

---

## 5.8 Errores comunes

| Síntoma | Causa | Fix |
|---------|-------|-----|
| El nameplate del remoto sale "Jugador" en vez del nombre real | `SetMyNameFromGameInstance` no se llama porque `OnRep_PlayerState` aún no había llegado | Asegúrate de llamarlo también en `PossessedBy` (ya lo hace el código) |
| Mi propio nameplate aparece a veces vacío al primer frame | `RefreshNameplate` corre antes de que el widget esté instanciado | Llámalo también en `OnInitialized` del widget, o usa el `OnPlayerNameUpdated` event |
| Otro cliente "salta" varios metros tras un pickup | Conflictos entre transacción de ownership del ingrediente y el `CharacterMovementComponent` | Asegúrate de que el ingrediente se attacha como child y desactiva su simulación local. Detalle en [06-Pickup-Drop.md](06-Pickup-Drop.md) |
| `PlayerName` se ve correcto en local pero todos los remotos se quedan a "Jugador" | Olvidaste `DOREPLIFETIME(APlayerCocina, PlayerName)` | Añádelo |

---

> **Próxima fase:** [06-Pickup-Drop.md](06-Pickup-Drop.md) — interacción E para recoger/soltar ingredientes con transacción de ownership.
