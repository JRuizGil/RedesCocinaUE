# 08 — PASOS de asignación (Recetas y emplatado)

> Guía práctica para cablear en el editor lo que ya está implementado en C++.
> Los scripts de la fase 8 están escritos y **compilan**. Aquí solo se asigna en editor.

## Cambio de arquitectura importante (léelo)

A diferencia del borrador de [08-Recetas-Emplatado.md](08-Recetas-Emplatado.md), **no se usan Server RPC**. En este setup de Fusion (modo Shared) los `UFUNCTION(Server)` **no cruzan la red**: se ejecutan en local. Por eso solo funcionaban en el host.

Patrón real usado en todo el gameplay:
- Solo el **Master Client (MC)** modifica el estado autoritativo (estación, plato, puntos).
- Un cliente normal **no llama directamente** al MC. Escribe una *petición* en propiedades replicadas de **su propio pawn** (`APlayerCocina`), que sí posee. El MC recibe esa petición por replicación (`OnRep_Request`) y ejecuta la lógica.

Esto ya está implementado: `SubmitDepositarEstacion`, `SubmitRecogerEstacion`, `SubmitDepositarPlato` → `DispatchCocinaRequest()` en el MC. **No tienes que tocar nada de esto.**

---

## Clases C++ nuevas de esta fase

| Clase | Archivo | Qué es |
|-------|---------|--------|
| `URecetaAsset` | `Cocina/RecetaAsset.h` | DataAsset: lista de `ETipoIngrediente` + puntos |
| `AZonaEmplatado` | `Cocina/ZonaEmplatado.*` | Donde se combinan los ingredientes procesados |
| `ASpawnerIngredientes` | `Cocina/SpawnerIngredientes.*` | Genera ingredientes al inicio (solo MC) |
| Cambios en `AGameStateCocina` | — | array `Recetas` + `GetRecetaActiva()` |
| Cambios en `AIngrediente` | — | flag replicado `bEnPlato` |

---

## Paso 0 — Recompilar y abrir

1. Cierra el editor si está abierto.
2. Compila (ya hecho: *Succeeded*). Si editas más C++, usa **Ctrl+Alt+F11** solo para cambios de cuerpo; cualquier cambio de `UPROPERTY/UFUNCTION` requiere rebuild con el editor cerrado.
3. Abre el proyecto.

---

## Paso 1 — Crear la receta (`DA_Receta_Hamburguesa`)

1. En el *Content Browser*, ve a `Content/Cocina/` y crea la carpeta `Recetas`.
2. Botón derecho → **Miscellaneous → Data Asset**.
3. En el diálogo de clase, elige **RecetaAsset**.
4. Nómbralo `DA_Receta_Hamburguesa`. Ábrelo y rellena:
   - **Display Name**: `Hamburguesa con patatas`
   - **Ingredientes** (array, añade 3 elementos): `Carne`, `Lechuga`, `Patata`
   - **Puntos Completar**: `50`
5. Guarda.

> El orden de los ingredientes **no importa**: la comparación es por multiconjunto.

---

## Paso 2 — GameState con catálogo de recetas

`Recetas` es `EditDefaultsOnly`, así que necesitas un **Blueprint** del GameState para poblarlo.

1. Content Browser → botón derecho → **Blueprint Class** → busca y elige **GameStateCocina**.
2. Nómbralo `BP_GameStateCocina`, guárdalo en `Content/Cocina/`.
3. Ábrelo → pestaña **Class Defaults** → categoría **Cocina** → **Recetas**:
   - Añade 1 elemento y asigna `DA_Receta_Hamburguesa`.
4. Abre `BP_CocinaGameMode` → **Class Defaults** → **Classes → Game State Class** = `BP_GameStateCocina`.
5. Guarda ambos.

> El GameMode ya llama a `SetRecetaActivaMC(0)` al arrancar la partida (3 s tras el primer login del MC), así que la receta activa será la del índice 0.

---

## Paso 3 — Blueprint de la zona de emplatado

1. Content Browser → **Blueprint Class** → elige **ZonaEmplatado** → nómbralo `BP_ZonaEmplatado` en `Content/Cocina/`.
2. Ábrelo. El componente raíz `Mesh` (StaticMesh) ya está creado en C++:
   - Selecciona **Mesh** → asígnale una malla visible (p. ej. un *Cube* o un plato). Es importante que tenga malla porque el *raycast* del jugador necesita impactarla para detectar la zona.
   - Comprueba que su **Collision Preset** sea `BlockAllDynamic` (ya viene así desde C++; no lo pongas en "Trigger/Overlap" o el trace no la detectará).
3. (Opcional) Ajusta **Altura Plato** (cm sobre la zona donde se apilan los ingredientes). Por defecto 50.
4. Coloca `BP_ZonaEmplatado` en el mapa `LV_Cocina`, cerca de las estaciones.

> Ownership = `MasterClient` ya está fijado en C++. No toques nada de red.

---

## Paso 4 — (Recomendado) Spawner de ingredientes

Para no quedarte sin ingredientes (sobre todo si activas el modo continuo de recetas).

1. **Blueprint Class** → elige **SpawnerIngredientes** → `BP_SpawnerIngredientes` en `Content/Cocina/`.
2. Ábrelo → **Class Defaults** → categoría **Cocina** → **Entradas** (array). Añade 3 elementos:
   - Elemento 0: **Clase** = `BP_Ingrediente_Carne`, **Cantidad** = 3
   - Elemento 1: **Clase** = `BP_Ingrediente_Lechuga`, **Cantidad** = 3
   - Elemento 2: **Clase** = `BP_Ingrediente_Patata`, **Cantidad** = 3
3. Ajusta **Separacion** (80 cm) y **Altura Spawn** (50 cm) si quieres.
4. Coloca **un** `BP_SpawnerIngredientes` en el mapa, en una zona despejada (los ingredientes aparecen en una rejilla a su alrededor).

> Solo el MC ejecuta el spawn; el `FusionNetDriver` replica los actores al resto. Si ya tienes ingredientes colocados a mano en el nivel, el spawner es **adicional** (puedes borrar los manuales o dejarlos).

---

## Paso 5 — Verificar el cableado de interacción (sin tocar nada)

El `UCocinaInteractor` (en `BP_PlayerCocina`) ya gestiona la tecla **E** con esta prioridad:

1. Miro **Estación** + llevo ingrediente → **depositar** (si está Libre y el tipo coincide).
2. Miro **Estación** + manos vacías + estado **Listo** → **recoger**.
3. Miro **Zona de emplatado** + llevo ingrediente **procesado** → **emplatar**.
4. Llevo ingrediente y no miro nada útil → **soltar**.
5. Miro ingrediente suelto → **coger**.

No requiere cambios en Blueprint. Solo asegúrate de que `BP_PlayerCocina` tenga `IA_Interact` asignado (fase 06) y los `MappingContexts`.

---

## Paso 6 — Smoke test (2 jugadores)

1. **Play** con 2 clientes (Net Mode: *Play As Client*, 2 jugadores) o dos instancias.
2. Espera ~3 s: la partida arranca y la receta activa es Hamburguesa.
3. Jugador A: coge **carne** → deposita en **Sartén** → espera → recoge (la estación pasa verde→amarillo→rojo).
4. Jugador B: coge **lechuga** → **Corte** → recoge.
5. Cualquiera: coge **patata** → **Freidora** → recoge.
6. Lleva los **tres procesados** a `BP_ZonaEmplatado` y pulsa **E** con cada uno mirando la zona. Cada ingrediente se reduce de escala y se queda en el plato.
7. Al colocar el tercero correcto: **+50** puntos (total esperado 10+20+30+50 = 110 si tus estaciones dan 10/20/30) y `EstadoPartida → Finalizada`.

### Qué mirar en el log
- `[ZonaEmplatado] ... agregado X al plato (total=N)`
- `[ZonaEmplatado] RECETA COMPLETA (+50 puntos)`
- Si depositas un ingrediente **crudo**: `rechazado: ingrediente crudo`.

---

## Paso 7 — Checklist de fin de fase

- [ ] `DA_Receta_Hamburguesa` creado con Carne+Lechuga+Patata, 50 pts.
- [ ] `BP_GameStateCocina` con `Recetas[0] = DA_Receta_Hamburguesa`.
- [ ] `BP_CocinaGameMode → Game State Class = BP_GameStateCocina`.
- [ ] `BP_ZonaEmplatado` en el mapa con malla visible y colisión `BlockAllDynamic`.
- [ ] (Opcional) `BP_SpawnerIngredientes` con sus 3 entradas.
- [ ] Emplatar 3 procesados correctos suma puntos y finaliza la partida.
- [ ] Funciona **en cliente, no solo en el host** (clave de esta práctica).

---

## ¿Quieres modo continuo en vez de fin de partida?

En `ZonaEmplatado.cpp → MC_ChequearReceta()`:
- Comenta `GS->FinalizarPartidaMC();`
- Descomenta la línea de `SetRecetaActivaMC((RecetaActivaIndex + 1) % Recetas.Num())`.

Así la partida sigue hasta que se agote el tiempo (§09/§10), generando una receta nueva tras cada emplatado.

---

> **Próxima fase:** [09-Tiempo-Puntuacion.md](09-Tiempo-Puntuacion.md) — HUD con timer y puntuación sincronizados con `NetworkTime()`.
