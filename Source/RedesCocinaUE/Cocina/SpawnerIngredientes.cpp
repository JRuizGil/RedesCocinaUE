#include "Cocina/SpawnerIngredientes.h"
#include "Cocina/Ingrediente.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Engine/World.h"

ASpawnerIngredientes::ASpawnerIngredientes()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::MasterClient;
}

void ASpawnerIngredientes::BeginPlay()
{
    Super::BeginPlay();

    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    int32 Columna = 0;
    for (const FSpawnIngredienteEntry& Entrada : Entradas)
    {
        if (!Entrada.Clase) { ++Columna; continue; }

        for (int32 i = 0; i < Entrada.Cantidad; ++i)
        {
            const FVector Loc = GetActorLocation()
                + FVector(Columna * Separacion, i * Separacion, AlturaSpawn);
            World->SpawnActor<AIngrediente>(Entrada.Clase, Loc, FRotator::ZeroRotator);
        }
        ++Columna;
    }

    UE_LOG(LogTemp, Log, TEXT("[Spawner] %s: ingredientes spawneados por el MC"), *GetNameSafe(this));
}
