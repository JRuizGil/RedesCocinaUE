# 01 — Setup de Photon Fusion 3 en Unreal Engine 5.6.1

Objetivo de esta fase: dejar el proyecto **conectándose** a Photon, **listo para crear salas**, sin tocar gameplay todavía. Si al final ves `EFusionStatus::Connected` en el log y puedes crear una sala vacía, la fase está completa.

---

## 1.1 Cuenta y App ID de Photon

1. Crea cuenta en https://dashboard.photonengine.com/ (si no tienes).
2. **Create a New App** → tipo **Fusion**. Photon emite un **App ID** (GUID hex de 32 caracteres).
3. Plan **Free**: suficiente para esta práctica (20 CCU concurrentes, sobra para 2-4 jugadores en pruebas WAN).
4. Anota el App ID. No lo subas público al repo.

> Si el commit `Photon Linked` ya está en el repo, lo más probable es que el AppID ya esté volcado a la `DefaultPhotonFusion.ini`. Verifícalo en la sección [Verificar AppID](#verificar-appid).

---

## 1.2 Verificar el plugin instalado

En el proyecto, abre [Plugins/PhotonFusion/PhotonFusion.uplugin](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/PhotonFusion.uplugin):

```json
{
  "FriendlyName": "Photon Fusion",
  "Version": 1214,
  "VersionName": "3.0.0-Preview-1214",
  "EngineVersion": "5.6.0",
  "Modules": [
    { "Name": "PhotonFusion", "Type": "Runtime", "LoadingPhase": "Default" },
    { "Name": "PhotonFusionEditor", "Type": "UncookedOnly", "LoadingPhase": "PreDefault" }
  ]
}
```

✅ Si lo ves, el plugin ya está. **NO** lo descargues otra vez del Marketplace — la versión 1214 es la que se ha probado para esta práctica.

---

## 1.3 Habilitar el plugin para BP/C++

### En el editor

`Edit → Plugins → Photon → Photon Fusion` debe estar marcado **Enabled**. Reinicia el editor si lo activas ahora.

### Para usar la API en C++

Edita [Source/RedesCocinaUE/RedesCocinaUE.Build.cs](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/RedesCocinaUE.Build.cs) y añade `"PhotonFusion"` a `PublicDependencyModuleNames`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",
    "AIModule",
    "StateTreeModule",
    "GameplayStateTreeModule",
    "UMG",
    "Slate",
    "PhotonFusion"     // <-- AÑADIR
});
```

Regenera proyecto VS: clic derecho en `RedesCocinaUE.uproject` → `Generate Visual Studio project files`. Recompila desde el editor (Live Coding `Ctrl+Alt+F11`) o desde Rider/VS.

---

## 1.4 Configurar el AppID

### Vía editor (recomendado)

`Edit → Project Settings → Plugins → Photon Fusion` (o ruta similar; depende del build del plugin). Pega el **App ID** en el campo correspondiente (`AppId` o `Fusion Online Subsystem Settings → AppId`) y el **AppVersion** déjalo en `1.0` para todas las sesiones.

### <a id="verificar-appid"></a>Verificar AppID directamente en el .ini

Inspecciona [Config/DefaultEngine.ini](../../../Unreal%20Projects/RedesCocinaUE/Config/DefaultEngine.ini) y [Config/DefaultPhotonFusion.ini](../../../Unreal%20Projects/RedesCocinaUE/Config/DefaultPhotonFusion.ini) (si existe). Debe aparecer una entrada tipo:

```ini
[/Script/PhotonFusion.FusionOnlineSubsystemSettings]
AppId=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
AppVersion=1.0.0
```

⚠️ **No subas el AppID al repo si va a ser público.** Si lo dejas, considera mover a `Config/DefaultPhotonFusion.local.ini` y gitignore.

---

## 1.5 Configurar el FusionNetDriver

Este es el paso crítico que mucha gente olvida y produce errores "no se puede crear sesión": Unreal por defecto usa `IpNetDriver`. Fusion proporciona su propio NetDriver que enruta paquetes vía Photon Cloud.

Edita [Config/DefaultEngine.ini](../../../Unreal%20Projects/RedesCocinaUE/Config/DefaultEngine.ini) y añade (si no está ya):

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/PhotonFusion.FusionNetDriver",DriverClassNameFallback="/Script/PhotonFusion.FusionNetDriver")

[/Script/PhotonFusion.FusionNetDriver]
NetConnectionClassName="/Script/PhotonFusion.FusionNetConnection"
MaxClientRate=15000
MaxInternetClientRate=15000

[/Script/Engine.OnlineSubsystem]
DefaultPlatformService=PhotonFusion

[OnlineSubsystem]
DefaultPlatformService=PhotonFusion
```

> Los nombres `FusionNetDriver` y `FusionNetConnection` están confirmados en [Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionNetDriver.h](../../../Unreal%20Projects/RedesCocinaUE/Plugins/PhotonFusion/Source/PhotonFusion/Public/FusionNetDriver.h) y `FusionNetConnection.h`.

---

## 1.6 Smoke test: conectar a Photon

Vamos a crear una clase de prueba mínima que conecte al cloud y loguee el estado.

### C++: `UCocinaConnectTest.h`

Crea [Source/RedesCocinaUE/Cocina/CocinaConnectTest.h](../../../Unreal%20Projects/RedesCocinaUE/Source/RedesCocinaUE/Cocina/CocinaConnectTest.h):

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CocinaConnectTest.generated.h"

UCLASS()
class REDESCOCINAUE_API UCocinaConnectTest : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Cocina|Test", meta=(WorldContext="WorldContextObject"))
    static void TryConnectToPhoton(UObject* WorldContextObject);
};
```

### C++: `CocinaConnectTest.cpp`

```cpp
#include "Cocina/CocinaConnectTest.h"
#include "FusionOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UCocinaConnectTest::TryConnectToPhoton(UObject* WorldContextObject)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    if (!World) return;

    UGameInstance* GI = World->GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cocina] FusionOnlineSubsystem no disponible"));
        return;
    }

    FFusionConnectOptions Options;
    Options.RegionSelectionMode = EFusionRegionSelectionMode::Best;

    UFusionConnectToPhotonAsync* Action = Fusion->ConnectToPhoton(Options, WorldContextObject);
    if (!Action)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cocina] ConnectToPhoton devolvió null"));
        return;
    }

    Action->OnSuccess.AddDynamic(UCocinaConnectTest::StaticClass()->GetDefaultObject<UCocinaConnectTest>(), &UCocinaConnectTest::ReportSuccess);
    // Para BP es más sencillo: usa el nodo async directamente.
    UE_LOG(LogTemp, Log, TEXT("[Cocina] Conectando a Photon (region=Best)..."));
}
```

> Si la firma del delegate falla a compilar (es `DECLARE_DYNAMIC_MULTICAST_DELEGATE`), añade una función miembro `UFUNCTION()` que reciba 0 parámetros como callback. Para simplicidad en producción usa el nodo BP, que es lo recomendado por el plugin.

### BP: nodo async (versión recomendada)

En el `BP_MainMenu` (la crearás en [03-Menu-Conexion.md](03-Menu-Conexion.md)) o en un widget de prueba:

1. Botón → evento OnClicked.
2. Nodo `Connect To Photon` (categoría **Photon**, expuesto por `UFusionOnlineSubsystem::ConnectToPhoton`).
3. Estructura `FFusionConnectOptions`: `RegionSelectionMode = Best`.
4. El nodo tiene dos pines de salida: `On Success` y `On Failure`.
5. En `On Success`, `Print String "Conectado a Photon"`.
6. En `On Failure`, `Print String "Fallo: {FailureCode}"` (pasa el enum `EFusionActionFailureCodes`).

Compila el blueprint y juega en PIE. Deberías ver el log en pocos segundos.

---

## 1.7 Smoke test: crear sala

Una vez conectado:

- Nodo `Create Room` con `FFusionRoomOptions`:
  - `RoomName = "TestSala"`
  - `MaxPlayers = 4`
  - `bIsOpen = true`, `bIsVisible = true`
  - `EmptyTTL = 0`, `PlayerTTL = 5` (segundos antes de auto-kick si pierdes conexión)
  - `InitialWorld` = (déjalo vacío de momento; en fase 4 apuntará a `LV_Cocina`)
- En `On Success` → `Get Fusion Online Subsystem → Current Room Info` debería devolver `Name="TestSala"`, `Players=1`.

✅ **Hito 1 alcanzado**: el proyecto se conecta a Photon Cloud, crea sala y puedes verlo en log.

---

## 1.8 Smoke test WAN (2 instancias en redes distintas)

Esto es lo que diferencia tu práctica de una LAN. Hay dos formas de validar:

1. **Build standalone** (recomendado): `File → Package Project → Windows`. Pasa el `.exe` a tu pareja por otra red.
2. **PIE multi-instance**: `Editor Preferences → Play → Number of Players = 2`, `Net Mode = Play As Client`. Cada instancia hace `ConnectToPhoton → JoinRoom`. Aunque ambas estén en el mismo PC, el tráfico va por Photon Cloud y simula WAN correctamente.

> Para la demo final del enunciado §16.2 necesitas grabar la **build empaquetada** corriendo en dos máquinas distintas. PIE no cuenta.

---

## 1.9 Checklist de fin de fase

- [ ] AppID configurado y verificado en `.ini`.
- [ ] `PhotonFusion` añadido a `Build.cs`.
- [ ] `FusionNetDriver` configurado en `DefaultEngine.ini`.
- [ ] `OnlineSubsystem = PhotonFusion`.
- [ ] PIE: `ConnectToPhoton` devuelve `OnSuccess` y el log dice `[Cocina] Conectando…` seguido de `OnSuccess`.
- [ ] PIE: `CreateRoom` devuelve éxito y `CurrentRoomInfo` reporta la sala.
- [ ] (Opcional ahora, obligatorio antes de entregar) Build empaquetada conecta desde dos redes distintas.

---

## 1.10 Errores comunes

| Síntoma | Causa probable | Fix |
|---------|----------------|-----|
| `OnFailure → InvalidRegion` | Region inválida o no disponible | Cambia a `RegionSelectionMode = Default` o `Best`. |
| `OnFailure → ClientAlreadyExists` | Intentaste conectar dos veces sin `DisconnectFromPhoton` | Antes de reconectar, llama a `Disconnect From Photon`. |
| `FusionNetDriver not found` | Plugin no cargado o nombre mal escrito en .ini | Verifica `[Plugins/PhotonFusion/...uplugin]` y que está enabled. |
| `Cannot bind LowLevelNetDriver` | Falta `+NetDriverDefinitions` con clase Fusion | Revisa la sección 1.5. |
| Log dice "AppId vacío" | El campo no se guardó | Edita directamente `DefaultPhotonFusion.ini` y commit. |

---

> **Próxima fase:** [02-Arquitectura-Red.md](02-Arquitectura-Red.md) — antes de escribir más código necesitas entender ownership modes y el mapeo de Estado/Eventos/Inputs.
