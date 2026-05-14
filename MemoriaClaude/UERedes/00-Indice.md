# RedesCocinaUE — Guía de implementación

Guía paso a paso para construir el proyecto **AEG6 — Cooperativo de cocina online** en **Unreal Engine 5.6.1** + **Photon Fusion 3 (plugin v1214, Preview 3.0.0)** en modo **Shared**.

Esta guía cubre el flujo completo: configuración de Photon → arquitectura de red → menú/lobby/sala → spawn → gameplay (jugador, pickup, estaciones, recetas, emplatado) → fin de partida y pruebas WAN.

## Stack confirmado

- **Motor:** Unreal Engine 5.6.1
- **Plugin red:** Photon Fusion 3 Unreal v1214 (`3.0.0-Preview-1214`, EngineVersion 5.6.0)
- **Modo de red:** Shared (autoridad en Master Client por *ownership modes*)
- **Lenguaje:** Híbrido C++ + Blueprints. Clases base en C++; ajustes visuales y de gameplay ligero en BP derivado.
- **Base del proyecto:** ThirdPerson Template ya presente en [Source/RedesCocinaUE/](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/) (`RedesCocinaUECharacter`, `RedesCocinaUEGameMode`, `RedesCocinaUEPlayerController`).
- **Plugin instalado:** [Plugins/PhotonFusion/](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/) con módulo runtime `PhotonFusion` y editor `PhotonFusionEditor`.
- **AppID Photon:** ya configurado (commit `Photon Linked`). Verificación en [01-Setup-Photon.md](01-Setup-Photon.md#verificar-appid).

## Orden de implementación (síguelo en este orden)

1. **[01-Setup-Photon.md](01-Setup-Photon.md)** — Cuenta Photon, AppID, configuración del plugin, dependencias en `Build.cs`, `DefaultEngine.ini` para usar `FusionNetDriver`. Verificación: conectar a Photon y volcar `EFusionStatus::Connected` en log.
2. **[02-Arquitectura-Red.md](02-Arquitectura-Red.md)** — Conceptos clave: ownership modes (`MasterClient`, `Transaction`, `PlayerAttached`, `Dynamic`, `GameGlobal`), `IsMasterClient()`, `IsOwner()`, `CanModify()`, mapeo *Estado / Eventos / Inputs* a Unreal replication + Photon RPC. Tabla de qué actor lleva qué ownership.
3. **[03-Menu-Conexion.md](03-Menu-Conexion.md)** — Widget de menú, `UMenuGameInstance` con `PlayerDisplayName`, llamadas a `UFusionOnlineSubsystem::ConnectAndJoinRoom`, `CreateRoom`, `JoinRoom`. Handling de `OnSuccess`/`OnFailure`.
4. **[04-Sesion-Spawn.md](04-Sesion-Spawn.md)** — `FFusionRoomOptions::InitialWorld`, transición al mapa de cocina, `GameMode_Cocina` que spawnea PlayerStart, `AGameStateCocina` autoritativo del Master Client.
5. **[05-Jugador.md](05-Jugador.md)** — `APlayerCocina : APlayerCharacter`, `UFusionActorComponent`, `PlayerName` replicado con `OnRep_PlayerName`, widget nameplate sobre la cabeza.
6. **[06-Pickup-Drop.md](06-Pickup-Drop.md)** — `AIngrediente : AActor` con `UFusionActorComponent` modo `Transaction`. Tecla `E` envía intención. Master Client (o el solicitante via transacción) valida y attacha.
7. **[07-Estaciones-Procesado.md](07-Estaciones-Procesado.md)** — `AEstacion` base con ownership `MasterClient`. Estados `Libre/Procesando/Listo` replicados, contador en tick del Master Client, feedback de color en `OnRep_Estado`.
8. **[08-Recetas-Emplatado.md](08-Recetas-Emplatado.md)** — `FReceta` (DataAsset), receta activa en `AGameStateCocina`, `AZonaEmplatado` valida combinación, reward de puntos y nueva receta.
9. **[09-Tiempo-Puntuacion.md](09-Tiempo-Puntuacion.md)** — Cuenta atrás 2 min sincronizada usando `UFusionOnlineSubsystem::NetworkTime()`, puntuación cooperativa replicada, UI HUD (esquina superior izquierda = puntos, derecha = tiempo).
10. **[10-FinPartida-WAN.md](10-FinPartida-WAN.md)** — Condiciones de fin (tiempo / receta cumplida), pantalla de resumen, `LeaveRoom` + `ChangeWorld` al menú, manejo de salida de jugadores, packaging y checklist de prueba WAN.

## Convenciones de la guía

- **Rutas del proyecto Unreal:** absolutas desde la raíz del proyecto. Ejemplo: [Source/RedesCocinaUE/RedesCocinaUE.Build.cs](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/RedesCocinaUE.Build.cs).
- **C++ vs BP:** primero se muestra el código C++ base; debajo, los nodos BP equivalentes o el setup del Blueprint derivado en `Content/`. Las clases C++ van en `Source/RedesCocinaUE/Cocina/`.
- **Naming:** clases C++ con prefijo de UE (`A` actor, `U` componente, `F` struct). Blueprints derivados con prefijo `BP_`. Ej: `APlayerCocina` → `BP_PlayerCocina`.
- **Master Client = autoridad:** cuando leas "el Master Client hace X", se traduce en código a `if (UFusionOnlineSubsystem::Get(this)->IsMasterClient()) { ... }`.

## Glosario rápido

- **Master Client (MC):** jugador que Photon elige como autoridad lógica. Si se va, otro toma el rol automáticamente. Equivale conceptualmente al "servidor" en este modelo.
- **Ownership Mode:** cómo se decide quién puede escribir el estado de un actor en red. Lo configura `UFusionActorComponent::Ownership`.
- **`UFusionActorComponent`:** componente que marca un Actor como replicado por Fusion. Necesario en cualquier actor cuya `Transform` o estado quieras sincronizar.
- **`UFusionOnlineSubsystem`:** `GameInstanceSubsystem` que expone toda la API alto nivel: conectar, crear sala, unirse, cambiar mundo, `IsMasterClient()`, `NetworkTime()`, `SendRpc()`.
- **Intención (intent):** acción que el cliente solicita (ej. "quiero coger esto"). NUNCA modifica estado global; envía un RPC y deja que el MC decida.
- **Estado replicado:** `UPROPERTY(Replicated)` o `UPROPERTY(ReplicatedUsing=OnRep_Foo)`. El `FusionNetDriver` los enruta vía Photon.

## Decisiones técnicas tomadas (sin ambigüedad)

| Tema | Decisión | Justificación |
|------|----------|---------------|
| Modo Photon | Shared | Lo exige el enunciado §13. |
| Autoridad de reglas | Master Client (ownership `MasterClient` en GameState/Estaciones/Receta) | §13.1 enunciado + match con plugin v1214. |
| Movimiento del jugador | Owner del PlayerCharacter es el propio jugador (`Transaction`) | Cada cliente controla su propio personaje sin lag; el MC no necesita validar input granular. |
| Ingredientes (pickup) | Ownership `Transaction` | Permite traspaso limpio entre jugadores con semántica de transacción. |
| Estaciones | Ownership `MasterClient` | Solo el MC arranca/finaliza el procesado y arbitra estado. |
| Tiempo de partida | `NetworkTime()` como base + `StartTimestamp` replicado | Sincronización exacta sin tick-based drift. |
| Puntuación | Replicated en `AGameStateCocina`, escrita solo por MC | §10 enunciado. |
| Receta activa | DataAsset estática + índice replicado en GameState | Permite cambiar de receta sin re-replicar todo el contenido. |
| Mapa inicial post-conexión | `LV_Cocina` cargado vía `FFusionRoomOptions::InitialWorld` | Patrón recomendado del plugin (`ChangeWorld`/`InitialWorld`). |

## Estado actual conocido del proyecto

- ✅ Plugin Photon Fusion compilando.
- ✅ AppID configurado en `DefaultPhotonFusion.ini` / settings del plugin.
- ⏳ Falta: menú, lobby, gameplay completo.
- 📂 Carpetas relevantes: [Content/ThirdPerson/](../../../Unreal%20Projects/RedesCocinaUE/Content/ThirdPerson/) (base del personaje).

---

> Los archivos `01-` a `10-` son los tutoriales paso a paso. Empieza siempre por `01-Setup-Photon.md` para confirmar que el AppID y el NetDriver están bien antes de tocar gameplay.
