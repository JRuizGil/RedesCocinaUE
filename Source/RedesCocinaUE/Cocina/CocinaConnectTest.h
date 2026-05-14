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
}s