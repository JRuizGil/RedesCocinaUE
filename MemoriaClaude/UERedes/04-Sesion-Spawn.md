# 04 — Sesión, mapa de cocina y spawn de jugadores

Objetivo: cuando ambos clientes están en sala, cargan `LV_Cocina`, aparece un Pawn por cada uno en `PlayerStart` distintos, y un `AGameStateCocina` autoritativo se crea para llevar el estado global.

---

## 4.1 Crear el mapa `LV_Cocina`

1. `File → New Level → Basic`. Guardar como `Content/Maps/LV_Cocina`.
2. Iluminación mínima: `DirectionalLight`, `SkyLight`, `SkyAtmosphere`, `ExponentialHeightFog`.
3. Suelo: un cubo escalado `40x40x0.2` con material gris.
4. **PlayerStarts**: añade 4 `PlayerStart` (uno por jugador, max 4 según §15). Colócalos en círculo a 200 cm del centro, mirando al interior. Tag opcional `PStart_0..PStart_3`.
5. Guarda. Asegúrate de que el `WBP_MainMenu` referencia este asset en su `FFusionRoomOptions.InitialWorld`.

---

## 4.2 GameMode de la cocina

### C++: `CocinaGameMode.h`

[Source/RedesCocinaUE/Cocina/CocinaGameMode.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/CocinaGameMode.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CocinaGameMode.generated.h"

UCLASS()
class REDESCOCINAUE_API ACocinaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACocinaGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
    /** Asigna PStart_0..3 en orden de entrada. */
    int32 NextStartIndex = 0;
};
```

### C++: `CocinaGameMode.cpp`

```cpp
#include "Cocina/CocinaGameMode.h"
#include "Cocina/PlayerCocina.h"     // se creará en la siguiente fase
#include "Cocina/CocinaGameInstance.h"
#include "FusionOnlineSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

ACocinaGameMode::ACocinaGameMode()
{
    DefaultPawnClass = APlayerCocina::StaticClass();
}

AActor* ACocinaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Starts.Add(*It);
    if (Starts.Num() == 0) return Super::ChoosePlayerStart_Implementation(Player);

    APlayerStart* Pick = Starts[NextStartIndex % Starts.Num()];
    NextStartIndex++;
    return Pick;
}

void ACocinaGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UE_LOG(LogTemp, Log, TEXT("[Cocina] PostLogin: nuevo controller %s"), *GetNameSafe(NewPlayer));
}
```

> En Fusion Shared no hay "Login" en el sentido del modelo dedicated server clásico, pero `PostLogin` se sigue disparando cuando el `FusionNetDriver` crea el `PlayerController` para un cliente local que entra. El bucle de `PlayerStart` solo lo ejecuta el MC (gracias al GameMode existiendo únicamente en el MC), exactamente lo que queremos.

### Activar el GameMode

- Edita `LV_Cocina → World Settings → Game Mode Override = ACocinaGameMode`.
- Crea `BP_CocinaGameMode` derivado si quieres tocar parámetros desde editor (DefaultPawnClass por ejemplo).

---

## 4.3 GameState autoritativo: `AGameStateCocina`

El enunciado §13.1 obliga a centralizar tiempo, puntuación, estado de partida y receta activa. Va en el `AGameState` que Unreal crea automáticamente al iniciar la partida — sobrevive a la duración entera y replica.

### C++: `GameStateCocina.h`

[Source/RedesCocinaUE/Cocina/GameStateCocina.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/GameStateCocina.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameStateCocina.generated.h"

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

    /** Tiempo NetworkTime() del MC cuando arrancó la partida. */
    UPROPERTY(ReplicatedUsing=OnRep_StartTimestamp, BlueprintReadOnly, Category="Cocina")
    double StartTimestamp = -1.0;

    /** Duración total. Por enunciado §9 son 120s. Replicado por si lo configuramos. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Cocina")
    float DuracionPartida = 120.f;

    UPROPERTY(ReplicatedUsing=OnRep_EstadoPartida, BlueprintReadOnly, Category="Cocina")
    EEstadoPartida EstadoPartida = EEstadoPartida::Esperando;

    UPROPERTY(ReplicatedUsing=OnRep_Puntuacion, BlueprintReadOnly, Category="Cocina")
    int32 Puntuacion = 0;

    /** Índice en el array de recetas del DataAsset. -1 si no hay receta activa. */
    UPROPERTY(ReplicatedUsing=OnRep_RecetaActiva, BlueprintReadOnly, Category="Cocina")
    int32 RecetaActivaIndex = -1;

    // --- API autoritativa (solo llamar desde el MC) ---

    /** Arranca la partida si todavía está Esperando. Solo MC. */
    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void IniciarPartidaMC();

    /** Suma puntos por una acción válida. Solo MC. */
    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void SumarPuntosMC(int32 Delta);

    /** Cambia la receta activa. Solo MC. */
    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void SetRecetaActivaMC(int32 NewIndex);

    /** Finaliza la partida. Solo MC. */
    UFUNCTION(BlueprintCallable, Category="Cocina|MC")
    void FinalizarPartidaMC();

    /** Devuelve segundos restantes según NetworkTime(). Cualquier cliente. */
    UFUNCTION(BlueprintPure, Category="Cocina")
    float GetTiempoRestante() const;

protected:
    UFUNCTION() void OnRep_StartTimestamp();
    UFUNCTION() void OnRep_EstadoPartida();
    UFUNCTION() void OnRep_Puntuacion();
    UFUNCTION() void OnRep_RecetaActiva();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Helper que verifica que somos MC y loguea si no. */
    bool EnsureMC(const TCHAR* Op) const;
};
```

### C++: `GameStateCocina.cpp`

```cpp
#include "Cocina/GameStateCocina.h"
#include "FusionOnlineSubsystem.h"
#include "Net/UnrealNetwork.h"

AGameStateCocina::AGameStateCocina()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
}

void AGameStateCocina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AGameStateCocina, StartTimestamp);
    DOREPLIFETIME(AGameStateCocina, DuracionPartida);
    DOREPLIFETIME(AGameStateCocina, EstadoPartida);
    DOREPLIFETIME(AGameStateCocina, Puntuacion);
    DOREPLIFETIME(AGameStateCocina, RecetaActivaIndex);
}

bool AGameStateCocina::EnsureMC(const TCHAR* Op) const
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (Fusion && Fusion->IsMasterClient()) return true;
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] %s ignorado: no soy MC"), Op);
    return false;
}

void AGameStateCocina::IniciarPartidaMC()
{
    if (!EnsureMC(TEXT("IniciarPartidaMC"))) return;
    if (EstadoPartida != EEstadoPartida::Esperando) return;

    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    StartTimestamp = Fusion->NetworkTime();
    EstadoPartida = EEstadoPartida::EnCurso;
    Puntuacion = 0;
    // RecetaActivaIndex la pondrá el subsistema de recetas en la fase 08.
    OnRep_StartTimestamp();
    OnRep_EstadoPartida();
    OnRep_Puntuacion();
}

void AGameStateCocina::SumarPuntosMC(int32 Delta)
{
    if (!EnsureMC(TEXT("SumarPuntosMC"))) return;
    if (EstadoPartida != EEstadoPartida::EnCurso) return;
    Puntuacion += Delta;
    OnRep_Puntuacion();
}

void AGameStateCocina::SetRecetaActivaMC(int32 NewIndex)
{
    if (!EnsureMC(TEXT("SetRecetaActivaMC"))) return;
    RecetaActivaIndex = NewIndex;
    OnRep_RecetaActiva();
}

void AGameStateCocina::FinalizarPartidaMC()
{
    if (!EnsureMC(TEXT("FinalizarPartidaMC"))) return;
    if (EstadoPartida != EEstadoPartida::EnCurso) return;
    EstadoPartida = EEstadoPartida::Finalizada;
    OnRep_EstadoPartida();
}

float AGameStateCocina::GetTiempoRestante() const
{
    if (EstadoPartida == EEstadoPartida::Esperando) return DuracionPartida;
    if (EstadoPartida == EEstadoPartida::Finalizada) return 0.f;
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || StartTimestamp < 0) return DuracionPartida;
    const double Elapsed = Fusion->NetworkTime() - StartTimestamp;
    return FMath::Max(0.f, DuracionPartida - static_cast<float>(Elapsed));
}

void AGameStateCocina::OnRep_StartTimestamp() {}
void AGameStateCocina::OnRep_EstadoPartida() {}
void AGameStateCocina::OnRep_Puntuacion() {}
void AGameStateCocina::OnRep_RecetaActiva() {}
```

> Las `OnRep_*` están vacías ahora. Las usaremos en las fases 09 (UI HUD) y 08 (refresh de receta). Importante dejarlas declaradas para que `ReplicatedUsing` no falle.

### Activar el GameState

En `ACocinaGameMode` constructor:

```cpp
GameStateClass = AGameStateCocina::StaticClass();
```

---

## 4.4 Arranque automático de la partida

¿Cuándo llamar a `IniciarPartidaMC()`? Estrategia simple sin lobby separado:

- Cuando el MC detecta `PlayerCount() >= 2`, espera 2-3 segundos (cuenta atrás opcional §7.2), y arranca.
- Si entras solo en sala (modo prueba en editor), arranca inmediatamente al cabo de unos segundos.

Implementación: en `ACocinaGameMode::PostLogin` o en un `TickActor` ligero, comprobar y llamar.

```cpp
// CocinaGameMode.cpp
void ACocinaGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;

    AGameStateCocina* GS = GetGameState<AGameStateCocina>();
    if (!GS) return;

    if (GS->EstadoPartida == EEstadoPartida::Esperando && Fusion->PlayerCount() >= 1)
    {
        // Da 3s para que el segundo jugador termine de cargar. Si no llega, jugamos solos.
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([this]
        {
            if (UFusionOnlineSubsystem* F = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>())
            {
                if (!F->IsMasterClient()) return; // por si dejé de serlo
                if (AGameStateCocina* G = GetGameState<AGameStateCocina>())
                {
                    G->IniciarPartidaMC();
                }
            }
        }), 3.0f, false);
    }
}
```

> Si quieres un lobby explícito con "Pulsa para empezar" del MC, expón un `BtnIniciar` en un widget visible solo si `IsMasterClient()` que llame a `IniciarPartidaMC`. Es la versión limpia y cumple §7.2.

---

## 4.5 Smoke test

1. Cliente A en `LV_Menu`, **Crear sala** "S1" → carga `LV_Cocina`.
2. Cliente B en `LV_Menu`, **Unirse a sala** "S1" → carga `LV_Cocina`.
3. Ambos ven un Pawn ThirdPerson en su PlayerStart asignado.
4. Log del MC dice `[GameStateCocina] IniciarPartidaMC ok` 3 segundos después de entrar el segundo.
5. En el cliente no-MC, `GetTiempoRestante()` devuelve un valor entre 117 y 120 — coincidente con el MC ±RTT/2.

---

## 4.6 Checklist de fin de fase

- [ ] `LV_Cocina` existe con 4 PlayerStarts y un suelo.
- [ ] `ACocinaGameMode` activo en `LV_Cocina` con DefaultPawnClass placeholder.
- [ ] `AGameStateCocina` replicando StartTimestamp, EstadoPartida, Puntuacion, RecetaActivaIndex.
- [ ] Solo el MC puede llamar a `IniciarPartidaMC` (probar lanzando la llamada en cliente y verificar log "ignorado").
- [ ] `GetTiempoRestante` devuelve valor coherente en ambos clientes.

---

> **Próxima fase:** [05-Jugador.md](05-Jugador.md) — `APlayerCocina` derivado del template, replicación del nombre, nameplate.
