# 03 — Menú principal, nombre del jugador y conexión a sala

Objetivo: implementar el flujo §6 del enunciado.

- Widget de menú con campo de nombre + tres botones (Crear sala, Unirse a sala, Salir).
- `UCocinaGameInstance` que guarda el `PlayerDisplayName` durante toda la sesión.
- Llamadas a `UFusionOnlineSubsystem` para crear/unirse a salas con nombre.
- Transición al mapa de cocina cuando estamos `InRoom`.

---

## 3.1 GameInstance personalizado

Necesitamos persistir el nombre del jugador entre el menú y la sala. La `GameInstance` sobrevive cambios de mapa, así que es el sitio correcto.

### C++: `CocinaGameInstance.h`

[Source/RedesCocinaUE/Cocina/CocinaGameInstance.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/CocinaGameInstance.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CocinaGameInstance.generated.h"

UCLASS()
class REDESCOCINAUE_API UCocinaGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    /** Nombre que introduce el jugador en el menú. Persiste toda la sesión. */
    UPROPERTY(BlueprintReadWrite, Category="Cocina")
    FString PlayerDisplayName = TEXT("Jugador");

    /** Validación simple: 1..16 caracteres, sin espacios al borde. */
    UFUNCTION(BlueprintCallable, Category="Cocina")
    bool SetPlayerName(const FString& Raw);
};
```

### C++: `CocinaGameInstance.cpp`

```cpp
#include "Cocina/CocinaGameInstance.h"

bool UCocinaGameInstance::SetPlayerName(const FString& Raw)
{
    FString Trimmed = Raw.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || Trimmed.Len() > 16) return false;
    PlayerDisplayName = Trimmed;
    return true;
}
```

### Activarla

`Edit → Project Settings → Maps & Modes → Game Instance Class = CocinaGameInstance`.

Recompila. Comprueba en BP que `GetGameInstance → CocinaGameInstance` existe.

---

## 3.2 Widget de menú: `WBP_MainMenu`

Crea `Content/UI/WBP_MainMenu` como `UserWidget`. Layout:

```
Canvas
├── Vertical Box (centrado)
│   ├── TextBlock "RedesCocina — Coop Online"
│   ├── EditableTextBox  (Name="TxtNombre", HintText="Introduce tu nombre")
│   ├── EditableTextBox  (Name="TxtSala",   HintText="Nombre de sala")
│   ├── Button "Crear sala"      (Name="BtnCrear")
│   ├── Button "Unirse a sala"   (Name="BtnUnirse")
│   ├── Button "Salir"           (Name="BtnSalir")
│   └── TextBlock (Name="TxtEstado", Text="")
```

### Lógica del widget (BP)

**Event Construct:**
- `Get Game Instance → Cast to CocinaGameInstance → TxtNombre.SetText(PlayerDisplayName)`.

**OnTextCommitted del TxtNombre:**
- `Cocina GI → SetPlayerName(TxtNombre.GetText())`. Si devuelve false, pinta el TxtEstado en rojo: "Nombre inválido (1..16 chars)".

**OnClicked BtnCrear:**
```
1. Get Game Instance → Cocina GI → SetPlayerName(TxtNombre)  (rechaza si inválido)
2. TxtEstado.SetText("Conectando...")
3. Get Fusion Online Subsystem
4. Construir FFusionConnectOptions { RegionSelectionMode = Best }
5. Construir FFusionRoomOptions {
      RoomName = TxtSala.GetText() OR "Sala_" + Random4Digits,
      MaxPlayers = 4,
      bIsOpen = true,
      bIsVisible = true,
      EmptyTTL = 0,
      PlayerTTL = 5,
      InitialWorld = LV_Cocina (asset reference)
   }
6. Nodo async ConnectAndJoinRoom (de UFusionOnlineSubsystem)
   - OnSuccess  → RemoveFromParent (el plugin nos llevará a LV_Cocina vía InitialWorld)
   - OnFailure  → TxtEstado.SetText("Error: <FailureCode>")
```

**OnClicked BtnUnirse:**
```
1. SetPlayerName (idem)
2. TxtEstado.SetText("Conectando...")
3. Si NO estamos conectados: Connect To Photon (Best region) → OnSuccess → JoinRoom(TxtSala)
   Si YA conectados: JoinRoom(TxtSala) directamente
4. OnSuccess → RemoveFromParent (al unirse a sala que tenía InitialWorld, el MC ya hizo ChangeWorld)
5. OnFailure → TxtEstado.SetText("Sala no encontrada")
```

> ⚠️ Cuando el creador de la sala configuró `InitialWorld = LV_Cocina`, todos los clientes que se unan **cargarán ese mapa automáticamente** — lo gestiona `UFusionOnlineSubsystem`. No hagas `OpenLevel` manual desde el menú; es redundante y rompe el flujo.

**OnClicked BtnSalir:**
- `Quit Game (Quit Preference = Quit)`.

---

## 3.3 Activar el widget en el mapa menú

1. Crea un mapa vacío `Content/Maps/LV_Menu.umap`. Solo necesita un `SkyAtmosphere` para que no sea negro y nada más.
2. Crea `BP_GameMode_Menu` (mínimo, sin Pawn ni HUD especial). En el editor, `LV_Menu` → `World Settings → Game Mode Override = BP_GameMode_Menu`.
3. En `BP_GameMode_Menu → Event BeginPlay`:
   - `Create Widget (WBP_MainMenu) → Add to Viewport`
   - `Get Player Controller(0) → Set Show Mouse Cursor = true`
   - `Set Input Mode UI Only`
4. `Project Settings → Maps & Modes → Editor Startup Map = LV_Menu`, `Game Default Map = LV_Menu`.

---

## 3.4 Cómo entrega Fusion el `InitialWorld`

Internamente lo que pasa cuando llamas `CreateRoom` con `InitialWorld = LV_Cocina`:

1. El `UFusionOnlineSubsystem` guarda esa referencia en las propiedades de sala (Photon Custom Room Properties).
2. Cuando otro cliente hace `JoinRoom`, el subsystem lee la prop `InitialWorld` y dispara su flujo de carga (delegate `OnMapLoadRequested → OnMapLoadPerform → OnMapLoadDone`, declarados en [FusionOnlineSubsystem.h:108-110](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionOnlineSubsystem.h#L108-L110)).
3. Resultado: todos los clientes acaban en `LV_Cocina` con el `FusionNetDriver` activo.

No hace falta que escribas tú el `ServerTravel`/`ClientTravel`; lo gestiona el plugin.

---

## 3.5 Smoke test del menú

1. Compila.
2. Play in editor con `Number of Players = 1`, Net Mode = `Play Standalone`. Te aparece el widget.
3. Escribe `Pepe` en nombre y `TestSala1` en sala. Pulsa **Crear sala**.
4. Esperado:
   - TxtEstado pasa a "Conectando…" durante ~1-3 s.
   - El widget se cierra.
   - El mapa carga `LV_Cocina` (que aún no existe; verás un error de mapa no encontrado; lo arreglamos en [04-Sesion-Spawn.md](04-Sesion-Spawn.md)).
5. Para validar el join: lanza dos instancias (Build → Package, o `Number of Players = 2 / Net Mode = Play Standalone`). En la primera "Crear sala TestSala1". En la segunda escribe el mismo nombre y "Unirse a sala".

---

## 3.6 Validación del nombre en cliente vs en red

El nombre se valida y guarda en `CocinaGameInstance` **antes** de la conexión. Pero el enunciado §6.2 dice **"debe formar parte del estado del jugador en red"**, así que más adelante (en [05-Jugador.md](05-Jugador.md)) lo replicaremos en una `UPROPERTY(Replicated)` del `APlayerCocina`. Aquí solo lo cacheamos local; la propagación ocurre cuando el pawn se spawnea.

---

## 3.7 Checklist de fin de fase

- [ ] `UCocinaGameInstance` set como Game Instance en Project Settings.
- [ ] `WBP_MainMenu` muestra los 4 elementos (3 botones + 1 input nombre + 1 input sala + estado).
- [ ] `BP_GameMode_Menu` añade el widget en BeginPlay.
- [ ] `LV_Menu` es el Editor Startup Map y Game Default Map.
- [ ] Crear sala arranca el flow y produce error de mapa (`LV_Cocina` no existe aún) — eso es lo esperado al final de esta fase.
- [ ] Salir cierra el juego.

---

## 3.8 Errores comunes

| Síntoma | Causa | Fix |
|---------|-------|-----|
| El widget no aparece | GameMode override no aplicado, o widget no añadido al viewport | Recheck `BP_GameMode_Menu BeginPlay` |
| `OnFailure: Disconnected` al hacer JoinRoom | El segundo cliente no llamó `ConnectToPhoton` antes | Usa el nodo `ConnectAndJoinRoom` que hace los dos pasos, o encadena Connect → Join en BP |
| El primer cliente entra en sala pero la UI no se quita | Falta `RemoveFromParent` en `OnSuccess` | Añádelo |
| Nombre se pierde tras entrar a sala | Estás guardándolo en el widget en vez de en GameInstance | Mueve a GI |
| `JoinRoom` da `Unknown` aleatoriamente | Race condition: estabas todavía en `Connecting` | Comprueba `Fusion->Status() == EFusionStatus::Connected` antes de llamar |

---

> **Próxima fase:** [04-Sesion-Spawn.md](04-Sesion-Spawn.md) — crear `LV_Cocina`, el `GameMode_Cocina` y el spawn de jugadores autoritativo.
