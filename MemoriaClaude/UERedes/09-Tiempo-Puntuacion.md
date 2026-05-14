# 09 — Temporizador y puntuación sincronizados (HUD)

Objetivo §9 + §10: HUD que muestra el tiempo restante (esquina superior derecha) y la puntuación compartida (esquina superior izquierda), ambos en sincronía exacta entre todos los clientes.

---

## 9.1 Principio de sincronización temporal

**No** sumas o restas un float en `Tick`. Eso produce drift: cada cliente acumula error de frame y a los 2 minutos puedes ver 1-2s de diferencia entre pantallas.

**Sí**: el MC fija `StartTimestamp = NetworkTime()` una vez. Cada cliente calcula `Restante = Duracion - (NetworkTime() - StartTimestamp)`. Como `NetworkTime()` está sincronizado por Photon (es la misma reloj base ±RTT/2), el resultado es coherente.

Esto ya está implementado en [04-Sesion-Spawn.md](04-Sesion-Spawn.md) en `AGameStateCocina::GetTiempoRestante`. Aquí solo lo bindeamos al HUD.

---

## 9.2 `WBP_HUD`

`Content/UI/WBP_HUD` (UserWidget):

```
Canvas
├── TextBlock "TxtPuntos"  → top-left, anchor (0,0)
├── TextBlock "TxtTiempo"  → top-right, anchor (1,0)
├── TextBlock "TxtReceta"  → top-center, anchor (0.5,0)
└── HorizontalBox "PlatoPreview" → top-center under receta
```

### Bindings (Tick rate ~10Hz es suficiente)

En `WBP_HUD` Graph:

**Event Construct:**
- `Get Game State → Cast AGameStateCocina → Bind to OnRep_Puntuacion / OnRecetaActualizada / OnEstadoPartidaCambiado` (eventos BP que expones desde C++ con `BlueprintImplementableEvent` u `OnRep_*` directamente).
- Inicializa los Text con valores actuales.

**Refrescar Tiempo cada 0.1s:**
- Usa `Set Timer by Event` con loop=true, time=0.1.
- Callback: `Get Game State → GetTiempoRestante → Format = mm:ss → TxtTiempo.SetText`.

**Helper de formato:**
```
Pure function FormatTiempo(Time : Float) -> Text
  Mins = Floor(Time / 60)
  Secs = Floor(Time % 60)
  return "{Mins}:{Secs:02}"
```

**Puntos**: bindea directamente a `GS.Puntuacion` con un `Text Binding` (función pura) o reacciona a `OnRep_Puntuacion`.

**Receta**: en `OnRecetaActualizada` (BIE en `AGameStateCocina::OnRep_RecetaActiva`), pinta `Receta.DisplayName` y rellena el `PlatoPreview` con un icono por ingrediente.

### Mostrar el HUD

En `ACocinaGameMode` o en el `BP_PlayerController`:

```cpp
// CocinaPlayerController.cpp (creado si no existe)
void ACocinaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController() && HUDWidgetClass)
    {
        UUserWidget* W = CreateWidget<UUserWidget>(this, HUDWidgetClass);
        W->AddToViewport();
    }
}
```

---

## 9.3 Eventos BIE en `AGameStateCocina`

Para limpieza, en lugar de meter UMG en C++:

```cpp
// GameStateCocina.h
UFUNCTION(BlueprintImplementableEvent, Category="Cocina")
void K2_OnPuntuacionCambio(int32 NuevaPuntuacion);

UFUNCTION(BlueprintImplementableEvent, Category="Cocina")
void K2_OnEstadoPartidaCambio(EEstadoPartida Nuevo);

UFUNCTION(BlueprintImplementableEvent, Category="Cocina")
void K2_OnRecetaCambio(int32 NuevoIndice);

// .cpp dentro de las OnRep_*:
void AGameStateCocina::OnRep_Puntuacion()   { K2_OnPuntuacionCambio(Puntuacion); }
void AGameStateCocina::OnRep_EstadoPartida(){ K2_OnEstadoPartidaCambio(EstadoPartida); }
void AGameStateCocina::OnRep_RecetaActiva() { K2_OnRecetaCambio(RecetaActivaIndex); }
```

En `BP_GameStateCocina` (derivado), implementa los tres eventos. Como el WBP_HUD necesita escucharlos, expón un delegate dinámico desde `BP_GameStateCocina` o pásale referencia y suscríbete desde `WBP_HUD::Event Construct`.

> Patrón mínimo y suficiente: el HUD hace `Get Game State` cada frame y lee `Puntuacion`, `GetTiempoRestante`, `GetRecetaActiva()`. Es marginalmente menos eficiente pero más fácil de seguir.

---

## 9.4 Manejo del último segundo

Cuando `GetTiempoRestante() <= 0`:

- En el MC, dispara `FinalizarPartidaMC()` (estado pasa a Finalizada).
- En todos los clientes, `OnRep_EstadoPartida → K2_OnEstadoPartidaCambio` recibe `Finalizada` → el HUD muestra "Partida finalizada — Puntuación: 110" y un botón "Volver al menú".

Implementación en `ACocinaGameMode::Tick`:

```cpp
void ACocinaGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;

    AGameStateCocina* GS = GetGameState<AGameStateCocina>();
    if (!GS || GS->EstadoPartida != EEstadoPartida::EnCurso) return;
    if (GS->GetTiempoRestante() <= 0.f)
    {
        GS->FinalizarPartidaMC();
    }
}
```

Acuérdate de `PrimaryActorTick.bCanEverTick = true` en el constructor del GameMode si no lo está.

---

## 9.5 Smoke test

1. Empieza partida con 2 jugadores.
2. En ambos HUDs el contador arranca cerca de `2:00`.
3. Tras 60s, ambos muestran `1:00` con desviación < 0.3s (suficiente para coop casual).
4. Procesa una carne: el HUD pasa `0 → 10` simultáneamente en los dos clientes.
5. Termina la receta: `EstadoPartida=Finalizada`, el HUD muestra resumen.
6. Si el MC sale de la partida en medio (cierra ventana), el otro cliente es promovido a MC y el contador sigue corriendo sin parpadear (porque depende de `NetworkTime()` y `StartTimestamp` replicado, no del FTimerHandle local del MC).

---

## 9.6 Edge cases

| Caso | Resultado esperado |
|------|--------------------|
| Cliente entra a mitad de partida (joinin progress) | Recibe `StartTimestamp` y `Puntuacion` por replicación inicial. Su HUD pinta el estado correcto al instante. |
| MC desconecta a `1:30 restante` | Otro cliente promovido; `StartTimestamp` ya replicado, el nuevo MC sigue ticking. El HUD no muestra ningún glitch. |
| Sincronía drift > 1s | Generalmente bug de no usar `NetworkTime()`. Revisa que no estés haciendo `Restante -= Dt` en local. |
| El HUD se ve duplicado en pantalla | Estás creando el widget en `BeginPlay` del GameMode (que solo corre en MC pero también en clientes si está bien configurado) y además en el PlayerController. Mantén solo el del PC. |

---

## 9.7 Checklist de fin de fase

- [ ] HUD muestra `Puntos` arriba izquierda, `Tiempo` arriba derecha.
- [ ] Contador sincronizado entre clientes con desviación visible <0.5s.
- [ ] Puntos suben en ambos clientes a la vez al procesar/emplatar.
- [ ] Cuando el tiempo llega a 0, el MC marca `Finalizada` y todos lo ven.

---

> **Próxima fase:** [10-FinPartida-WAN.md](10-FinPartida-WAN.md) — pantalla de fin, vuelta al menú, robustez en red real y checklist de entrega.
