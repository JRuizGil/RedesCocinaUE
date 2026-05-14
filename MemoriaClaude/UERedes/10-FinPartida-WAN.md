# 10 — Fin de partida, robustez en red real y empaquetado

Objetivos §7.3, §14, §15, §16.2.

- Pantalla de fin con puntuación y botón "Volver al menú".
- Manejo robusto de salida de jugadores (incluido el MC).
- Empaquetado de build.
- Checklist completo para grabar el vídeo de demostración y la prueba WAN.

---

## 10.1 Pantalla de fin de partida

### `WBP_FinPartida`

```
Canvas (centrado)
├── TextBlock "TxtTitulo"   → "Partida finalizada"
├── TextBlock "TxtResultado"→ "Puntuación final: {N}"
├── Button    "BtnVolverMenu"
└── Button    "BtnReiniciar"  (opcional, §15)
```

### Disparar el widget

En el `BP_GameStateCocina → K2_OnEstadoPartidaCambio`:

```
if NewState == EEstadoPartida::Finalizada:
  Get Player Controller (0) → Create Widget WBP_FinPartida → Add to Viewport → Set Input Mode UI Only
```

### BtnVolverMenu

Acción:

```
1. Get Fusion Online Subsystem → LeaveRoom (WorldContext = self)
   - OnSuccess  → Open Level "LV_Menu"
   - OnFailure  → log y open level igualmente (fallback)
```

`LeaveRoom` está expuesto en [FusionOnlineSubsystem.h:186](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionOnlineSubsystem.h#L186). El subsystem se encarga de desconectar limpiamente y, si llamas `Open Level` o `ClientTravel` después, vuelves al menú offline. Alternativa: `Fusion->StopFusionSession(this, LV_Menu_softref)` que combina ambos pasos.

### BtnReiniciar (opcional)

Solo el MC puede iniciar otra partida. Implementación:

```
if not IsMasterClient: disable button.
else:
  Get GameState → Reset state (puntuacion=0, StartTimestamp=-1, EstadoPartida=Esperando)
  Borrar ingredientes en escena (spawner respawnea)
  Llamar IniciarPartidaMC tras 1s
```

> El enunciado lo marca como opcional. Si vas justo de tiempo, omítelo y deja solo "Volver al menú".

---

## 10.2 Salida de jugadores

Photon ya gestiona la salida nativa (PlayerTTL en `FFusionRoomOptions` permite reconexión durante esos segundos). Aspectos a cuidar:

### Si sale un cliente normal

- Su Pawn se destruye automáticamente.
- Los ingredientes que llevaba (`Holder = ese pawn`): como `Ownership = Transaction`, **NO se destruyen** por defecto. El plugin marca el `Holder` como inválido (`TWeakObjectPtr` apunta a nullptr).
- En tu `AIngrediente::OnRep_Holder` añade:
  ```cpp
  if (!Holder.IsValid())
  {
      DetachFromHolder();
      // Opcional: respawn al spawner para evitar ingredientes "perdidos" en sitios extraños.
  }
  ```

### Si sale el Master Client

- Photon promueve a otro cliente. `UFusionActorComponent::OnOwnerChanged` se dispara en actores con `Ownership = MasterClient`.
- `StartTimestamp`, `Puntuacion`, `EstadoPartida` ya están replicados como estado; el nuevo MC continúa con esos valores.
- **Importante**: los `FTimerHandle` locales del MC anterior **se pierden**. Por eso evitamos timers locales para lógica autoritativa: todo va por `NetworkTime() - StartTimestamp`.

### Si solo queda un jugador a mitad de partida

Decisión de diseño:
- **Opción A (recomendada):** sigue solo. El juego no se rompe; el último jugador puede completar la receta solo.
- **Opción B:** finalizar con `FinalizarPartidaMC()` si `PlayerCount() < 2`. Más estricto. Implementa en `ACocinaGameMode::Tick`:
  ```cpp
  if (Fusion->IsMasterClient() && Fusion->PlayerCount() < 2 && GS->EstadoPartida == EEstadoPartida::EnCurso)
      GS->FinalizarPartidaMC();
  ```

---

## 10.3 Anti-cheat blando — verificaciones cliente

§12 enunciado: el cliente no debe poder hacer trampa. Como Shared Mode tiene autoridad en el MC (que es otro cliente, no servidor cerrado), el "cheat" es modesto: alguien podría modificar su build local. Cosas que ya hace bien tu código:

- **Procesado**: solo el MC pasa estaciones a `Listo`. Un cliente trucado no puede.
- **Sumar puntos**: solo MC vía `SumarPuntosMC`. Un cliente que llame a `Puntuacion = 9999` no replicará al resto (no es owner).
- **Mover su personaje a velocidades absurdas**: como su propio pawn es `Transaction` con él de owner, podría enviar posiciones falsas. Esto NO es un problema en una práctica académica, pero conviene saberlo.

> Si quieres demostrar adicionalmente la diferencia: pon un PrintString en `MC_TryStartProcesado` y en el cliente no-MC intenta forzarlo. Verás que el log solo aparece en el MC.

---

## 10.4 Build y empaquetado

### Configurar build target

`Edit → Project Settings → Project → Maps & Modes`:
- `Game Default Map = LV_Menu`
- `Editor Startup Map = LV_Menu`
- `Default Game Mode = BP_GameMode_Menu` (el del menú)

`Edit → Project Settings → Packaging`:
- `Build Configuration = Shipping` o `Development`.
- En `List of maps to include in a packaged build`: añadir `LV_Menu` y `LV_Cocina`.
- `Use Pak File = true`.

### Empaquetar Windows

`File → Package Project → Windows → Windows (64-bit)`. Sale un `.exe` y carpeta `RedesCocinaUE/`. Empaqueta ambas máquinas con el mismo build o redistribúyelo.

### Antes de probar en WAN

- Confirma que `Config/DefaultPhotonFusion.ini` se incluye en el .pak (sí por defecto si está en `Config/`).
- Confirma que el AppID es válido y la región está disponible (`RegionSelectionMode=Best` ayuda).
- Abre el `.exe` en máquina A y máquina B. Cada uno crea/une sala. Si el log del Saved/Logs no muestra `EFusionStatus::Connected`, lee el error y revisa [01-Setup-Photon.md#errores-comunes](01-Setup-Photon.md#errores-comunes).

---

## 10.5 Checklist de prueba WAN (obligatorio §14 + §16.2)

Marca cada item antes de grabar el vídeo.

- [ ] Dos PCs en **redes distintas** (no la misma WiFi). Móvil con hotspot vale para el segundo si hace falta.
- [ ] Ambas builds usan el mismo AppID y AppVersion.
- [ ] PC A crea sala `Practica_AEG6` → carga LV_Cocina.
- [ ] PC B introduce nombre, mismo nombre de sala, "Unirse" → carga LV_Cocina.
- [ ] Ambos ven al otro Pawn con su nameplate correcto.
- [ ] Movimiento WASD sincronizado (sin teletransportes de >50cm).
- [ ] PC A coge un ingrediente. PC B lo ve attachado.
- [ ] Procesado: deposita carne → 5s → recoge. Ambos HUDs reflejan +10 puntos.
- [ ] Emplatado completo: receta → +50 puntos, EstadoPartida=Finalizada en ambos.
- [ ] Botón "Volver al menú" funciona en ambos PCs y vuelven a `LV_Menu`.
- [ ] Cerrar el PC A (el MC) a mitad: PC B promovido, sigue jugando.
- [ ] Ping/RTT del HUD razonable (puedes mostrarlo con `Get RTT` de Fusion).

---

## 10.6 Checklist de entregables (§16)

- [ ] **Build ejecutable** para Windows (`RedesCocinaUE.exe` + assets).
- [ ] **Repositorio Git** público o entregado al docente. Verifica que el AppID NO está en el repo si es público (mueve a `Config/DefaultPhotonFusion.local.ini` en .gitignore, o coméntalo).
- [ ] **Documento técnico exhaustivo** que cubra (§16.1):
  - Modelo cliente-servidor (Master Client autoridad).
  - Decisiones de ownership (la tabla de [02-Arquitectura-Red.md](02-Arquitectura-Red.md)).
  - Estado vs Eventos vs Inputs.
  - Captura de pantalla del menú, lobby, partida, fin.
  - Diagrama de secuencia del pickup (intent → transacción → attach).
  - Cumplimiento de §15 (2-4 jugadores, 1 receta, 3 ingredientes, procesado individual, composición final, fin claro).
  - Mismo documento por integrante (no se reparte).
- [ ] **Vídeo de demostración** (§16.2) que muestre:
  - Pantalla de menú con introducción de nombre.
  - Crear sala + unirse desde otra red (mostrar IP / ifconfig de cada PC al inicio si quieres ser explícito).
  - Movimiento de ambos jugadores.
  - Recogida y entrega de ingredientes.
  - Procesado en 2 o 3 estaciones distintas (cambio de color visible).
  - Zona de emplatado y composición.
  - Cambios de puntuación y tiempo durante la partida.
  - Fin de partida con resumen.
  - Visualización clara de que es WAN (basta que se vea el nombre del otro PC, o la cara en webcam de cada participante).

---

## 10.7 Para el documento técnico — secciones recomendadas

1. **Resumen del proyecto** — 1 párrafo.
2. **Stack y decisiones técnicas** — copia de la tabla de decisiones en [00-Indice.md](00-Indice.md).
3. **Setup Photon** — referencia [01-Setup-Photon.md](01-Setup-Photon.md).
4. **Modelo de autoridad** — referencia [02-Arquitectura-Red.md](02-Arquitectura-Red.md), incluir la tabla de ownership por actor.
5. **Flujo de menú → sala → partida** — diagrama de estados con `UFusionOnlineSubsystem::Status()`.
6. **Sistema de personaje** — replicación de nombre, nameplate.
7. **Sistema de objetos** — pickup/drop con transacción.
8. **Estaciones y procesado** — diagrama del estado machine + tabla de tiempos/puntos.
9. **Recetas y emplatado** — flujo de validación en MC.
10. **Sincronización temporal** — uso de `NetworkTime()`.
11. **Robustez** — promoción MC, salida de clientes.
12. **Pruebas WAN** — capturas o link al vídeo.
13. **Riesgos y limitaciones** — qué no funcionaría con >4 jugadores, qué cheats son posibles, etc.

---

## 10.8 Checklist final

Antes de entregar:

- [ ] Las 10 fases de esta guía completadas y cada smoke test pasado.
- [ ] Build empaquetada probada en 2 PCs distintos en redes distintas.
- [ ] Vídeo grabado con todos los items del §16.2.
- [ ] Documento técnico revisado por ambos integrantes (deben poder explicar cualquier parte del sistema).
- [ ] Repo Git pusheado y URL apuntada en el documento.
- [ ] AppID **no expuesto** en el repo público (si aplica).

---

> **Volver a:** [00-Indice.md](00-Indice.md) — índice de la guía.
