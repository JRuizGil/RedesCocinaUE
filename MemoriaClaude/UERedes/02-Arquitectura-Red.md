# 02 — Arquitectura de red: Shared Mode, Master Client y ownership

Esta página **no escribe código**. Define los conceptos y decisiones que el resto de tutoriales aplican. Léela entera antes de tocar gameplay — el 80% de los bugs de red en proyectos de cocina online vienen de no tener claro quién es la autoridad de qué.

---

## 2.1 Shared Mode en Photon Fusion 3

En Fusion 3 hay tres modos: **Hosted** (servidor dedicado), **Server** (uno de los clientes hace de servidor) y **Shared** (todos son clientes, pero uno tiene rol de **Master Client** elegido por Photon). Aquí usamos **Shared** por imposición del enunciado §13.

Implicaciones:

- No hay servidor dedicado. Todo el código corre en clientes.
- **El Master Client (MC) actúa como autoridad lógica.** Si se desconecta, Photon promueve a otro automáticamente.
- La autoridad se expresa **por ownership de objeto**, no por "soy server / soy client" como en el modelo nativo de Unreal.
- El `FusionNetDriver` enruta la replicación estándar de Unreal por Photon, pero el "quién puede escribir" lo decide cada `UFusionActorComponent::Ownership`.

---

## 2.2 Ownership modes — la decisión arquitectónica más importante

El plugin define cinco modos en `EFusionObjectOwnerFlags` ([FusionActorComponent.h:38](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionActorComponent.h#L38)):

| Modo | Cuándo usarlo | En este proyecto |
|------|---------------|------------------|
| `Transaction` | Default. Cualquier jugador puede pedir ownership y el plugin coordina el traspaso. | **Ingredientes** (alguien los recoge), **Personaje del jugador** (él mismo es owner). |
| `PlayerAttached` | Igual que `Transaction`, pero **se destruye** si el owner se va. | No usamos. (Un ingrediente en mano debería volver al spawn si su dueño se va, no desaparecer.) |
| `Dynamic` | Ownership cambia por proximidad. Para físicas de alto throughput. | No necesario para cocina cooperativa. |
| `MasterClient` | El MC siempre es owner. Si el MC cambia, el ownership se transfiere automático. | **GameState (puntuación, tiempo, receta activa)**, **Estaciones de cocina**, **Zona de emplatado**, **Spawners de ingredientes**. |
| `GameGlobal` | Lifetime y ownership totalmente manuales. | No necesario en este proyecto. |

### Tabla de ownership del proyecto

| Actor | Ownership | Quién puede escribir su estado |
|-------|-----------|--------------------------------|
| `AGameStateCocina` (timer, score, receta activa) | `MasterClient` | Solo el MC |
| `AEstacion_Sarten` / `AEstacion_Corte` / `AEstacion_Freidora` | `MasterClient` | Solo el MC |
| `AZonaEmplatado` | `MasterClient` | Solo el MC |
| `ASpawner_Ingredientes` | `MasterClient` | Solo el MC |
| `AIngrediente` (carne, lechuga, patata) | `Transaction` | El jugador que lo lleva en mano; libre si no |
| `APlayerCocina` (Pawn) | `Transaction` (efectivo: lo posee el jugador que lo controla) | El propio jugador |

> Configura este modo en el `UFusionActorComponent` del Actor, propiedad `Ownership` ([FusionActorComponent.h:171](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionActorComponent.h#L171)).

---

## 2.3 APIs clave para razonar sobre autoridad

Todas vienen de `UFusionOnlineSubsystem` ([FusionOnlineSubsystem.h](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionOnlineSubsystem.h)):

```cpp
// "¿Soy el Master Client?"
bool bIsMC = Fusion->IsMasterClient();

// "¿Soy el dueño de este actor concreto?"
bool bIsOwner = UFusionOnlineSubsystem::IsOwner(SomeActor);

// "¿Puedo modificar el estado replicado de este objeto?"
//  Devuelve true si soy owner O si el ownership permite escritura local
bool bCanWrite = UFusionOnlineSubsystem::CanModify(SomeActor);

// "Quiero ser el owner de este actor" (intent — el plugin coordina la transacción)
UFusionOnlineSubsystem::SetWantsOwner(SomeActor, true);

// "¿Cuántos jugadores hay?"
int32 N = Fusion->PlayerCount();

// "Mi ID de jugador en la sala" (1-based desde Photon)
int32 MyId = Fusion->GetLocalPlayerId();

// "Tiempo de red sincronizado, monotónico, en segundos"
double T = Fusion->NetworkTime();
```

**Patrón canónico** en cualquier callback de gameplay autoritativo:

```cpp
if (UFusionOnlineSubsystem* Fusion = UGameInstance::GetSubsystem<UFusionOnlineSubsystem>(GetGameInstance()))
{
    if (Fusion->IsMasterClient())
    {
        // Lógica autoritativa: validar acción, mutar estado replicado.
    }
    else
    {
        // Soy un cliente normal: enviar intent (RPC) al MC y esperar.
    }
}
```

---

## 2.4 Estado vs Eventos vs Inputs (mapeo §12.2 del enunciado)

El enunciado pide distinguir tres tipos de información. Así se mapea cada uno a la API:

### Estado persistente

Cosas que se mantienen entre frames y deben ser idénticas en todos los clientes.

- **Cómo:** `UPROPERTY(Replicated)` o `ReplicatedUsing=OnRep_X` en variables del actor con `UFusionActorComponent`.
- **Quién escribe:** el owner del actor. Por nuestra tabla, casi siempre el MC.
- **Ejemplos:** `PuntuacionActual` (en `AGameStateCocina`), `EstadoEstacion` (en `AEstacion`), `TiempoRestante` derivado de `StartTimestamp` replicado, `RecetaActivaIndex`, `NombreJugador` (en `APlayerCocina`).

```cpp
// AGameStateCocina.h
UPROPERTY(ReplicatedUsing=OnRep_Puntuacion)
int32 Puntuacion;

UFUNCTION()
void OnRep_Puntuacion();  // refresca la UI

// .cpp
void AGameStateCocina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AGameStateCocina, Puntuacion);
}
```

### Eventos (one-shot)

Cosas que **ocurren una vez** y no representan estado persistente: "se completó una receta", "se cambió de estación a `Listo`".

- **Cómo:** RPC Multicast (`UFUNCTION(NetMulticast, Reliable)`) emitida desde el MC, o el sistema `UFusionOnlineSubsystem::SendRpc(Actor, Id, Data)`.
- **Diferencia clave con estado:** si un cliente entra a mitad de partida, **no replica** el evento histórico, sólo el estado actual. Por eso "puntuación = 30" es estado, pero "el jugador X ha sumado +30 con un emoji volador" es evento.
- **Ejemplos:** efecto VFX/SFX cuando termina la cocción, popup "+30", animación de plato listo.

### Inputs

Lo que el jugador pulsa. No replican: cada cliente los procesa local y emite intents al MC.

- **Cómo:** Enhanced Input → `IA_Move`, `IA_Look`, `IA_Pickup`. El handler local mueve directamente al `APlayerCocina` (es owner), y para acciones que tocan estado global emite un RPC `Server*`-style hacia el MC.
- **Por qué no replicar el input bruto:** sería un gasto innecesario. En Shared Mode no hay rollback de inputs como en Hosted; la latencia entre `pulsar E` y `recoger` es la del RTT con el MC, lo cual es aceptable para este género.

> En Unreal nativo `UFUNCTION(Server, Reliable)` requiere `RemoteRole` correcto. En Fusion Shared eso se traduce a: el RPC sale por `FusionNetDriver`, llega al owner del actor (si lo dirigimos al actor poseído por el MC), y se ejecuta allí. Más detalle en [08-Recetas-Emplatado.md#rpc-de-deposito](08-Recetas-Emplatado.md).

---

## 2.5 Patrón "Intent → Validación → Estado"

Es el patrón que repetirás cinco veces en este proyecto. Conviene memorizarlo:

```
Cliente                                Master Client
-------                                -------------
1. Pulsa E sobre Estacion
2. UFusionOnlineSubsystem::SendRpc(
     Estacion, RPC_SolicitarProcesar,
     {IngredienteId})
                              ───────►  3. Recibe RPC. Valida:
                                          - ¿Estacion en estado Libre?
                                          - ¿Ingrediente correcto para la estacion?
                                          - ¿El jugador X está realmente cerca?
                                        4. Si OK: Estado = Procesando, StartTime = NetworkTime()
                                        5. La replicación a clientes ocurre automática
6. OnRep_EstadoEstacion ◄──────────────────────
   → cambia color a amarillo
```

**Reglas de oro:**

1. **Nunca** mutes una `UPROPERTY(Replicated)` directamente desde un cliente que no es owner. Si lo haces, Fusion **descartará** el cambio cuando llegue el snapshot del owner, y verás el bug del "se desincroniza un segundo y vuelve".
2. **Nunca** valides en el cliente solicitante. Aunque puedas predecir local ("voy a pintar la estación amarilla ya"), la validación auténtica vive en el MC.
3. **Tiempo de procesado, puntuación, fin de partida**: SIEMPRE las decide el MC.

---

## 2.6 Promoción de Master Client

Si el jugador MC se desconecta, Photon escoge otro automáticamente. Tu código se adapta así:

- Las propiedades replicadas con ownership `MasterClient` cambian de owner sin perder valor — `Puntuacion=120` sigue siendo `120`.
- Pero **timers locales en C++** (ej. `GetWorldTimerManager().SetTimer(...)` en el MC) **no se transfieren**. Por eso para el contador de cocción, en vez de un FTimerHandle local, replicaremos `StartTimestamp` y haremos que cualquier cliente derive `(NetworkTime() - StartTimestamp)`.
- Suscríbete a `UFusionActorComponent::OnOwnerChanged` ([FusionActorComponent.h:267](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionActorComponent.h#L267)) si necesitas reaccionar al cambio de MC en algún Actor concreto.

---

## 2.7 Lista de checks antes de empezar a escribir gameplay

Antes de pasar a [03-Menu-Conexion.md](03-Menu-Conexion.md), confirma que entiendes:

- [ ] Diferencia entre `IsMasterClient()` (rol global) e `IsOwner(Actor)` (sobre un actor concreto).
- [ ] Por qué la puntuación va en `AGameStateCocina` con ownership `MasterClient` y no en cada `APlayerCocina`.
- [ ] Por qué los ingredientes son `Transaction` pero las estaciones son `MasterClient`.
- [ ] Que un input nunca toca `UPROPERTY(Replicated)` directamente: emite RPC al owner.
- [ ] Que `NetworkTime()` es la fuente de verdad para cualquier cuenta atrás o medición temporal.

> Si dudas en alguno de estos puntos, vuelve a leer la sección correspondiente antes de avanzar. No es trivial y ahorra horas de bugs.

---

> **Próxima fase:** [03-Menu-Conexion.md](03-Menu-Conexion.md) — implementación del menú principal, captura de nombre y entrada a sala.
