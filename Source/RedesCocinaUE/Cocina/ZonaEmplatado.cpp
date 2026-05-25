#include "Cocina/ZonaEmplatado.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "Cocina/RecetaAsset.h"
#include "Components/StaticMeshComponent.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AZonaEmplatado::AZonaEmplatado()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    // Bloquea el canal Visibility para que el SphereTrace del interactor lo detecte.
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::MasterClient;
}

void AZonaEmplatado::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AZonaEmplatado, PlatoActual);
}

void AZonaEmplatado::BeginPlay()
{
    Super::BeginPlay();
}

// ---------------- Cliente local ----------------

void AZonaEmplatado::RequestDepositarPlato(APlayerCocina* Player, AIngrediente* Ing)
{
    if (!Player || !Ing) return;

    if (Ing->Holder != Player)
    {
        UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] rechazado: no soy holder de %s"), *GetNameSafe(Ing));
        return;
    }
    if (Ing->Estado != EEstadoIngrediente::Procesado)
    {
        UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] rechazado: %s no esta procesado"), *GetNameSafe(Ing));
        return;
    }

    // Suelta el ingrediente (libera Holder y ownership) antes de cederlo al MC.
    Ing->RequestDrop(Player);

    // Comunica la intencion al MC por el canal replicado del pawn.
    Player->SubmitDepositarPlato(this, Ing);

    UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] %s: depositar plato enviado al MC (Ing=%s)"),
           *GetNameSafe(this), *GetNameSafe(Ing));
}

// ---------------- Logica autoritativa (solo MC) ----------------

void AZonaEmplatado::MC_HandleAgregar(AIngrediente* Ing)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (!Ing)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ZonaEmplatado] MC_HandleAgregar rechazado: Ing=null"));
        return;
    }
    if (Ing->Estado != EEstadoIngrediente::Procesado)
    {
        UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] MC_HandleAgregar rechazado: ingrediente crudo"));
        return;
    }
    if (Ing->bEnPlato) return;

    // El MC toma ownership para colocar y marcar el ingrediente de forma autoritativa.
    UFusionOnlineSubsystem::SetWantsOwner(Ing, true);

    // Coloca en una rejilla simple sobre la zona (3 por fila). SetEnPlatoMC desengancha
    // de la mano, posiciona y marca emplatado de forma autoritativa (en este orden).
    const int32 Idx = PlatoActual.Num();
    const FVector Offset(((Idx % 3) - 1) * 30.f, (Idx / 3) * 30.f, AlturaPlato);
    Ing->SetEnPlatoMC(GetActorLocation() + Offset);

    PlatoActual.Add(Ing->Tipo);
    OnRep_PlatoActual();

    UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] %s: agregado %d al plato (total=%d)"),
           *GetNameSafe(this), (int32)Ing->Tipo, PlatoActual.Num());

    MC_ChequearReceta();
}

void AZonaEmplatado::MC_ChequearReceta()
{
    AGameStateCocina* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateCocina>() : nullptr;
    if (!GS) return;

    URecetaAsset* Receta = GS->GetRecetaActiva();
    if (!Receta) return;
    if (PlatoActual.Num() < Receta->Ingredientes.Num()) return;

    // Compara como multiconjunto (el orden no importa).
    TArray<ETipoIngrediente> Esperados = Receta->Ingredientes;
    TArray<ETipoIngrediente> Tengo = PlatoActual;
    auto Cmp = [](ETipoIngrediente A, ETipoIngrediente B) { return (uint8)A < (uint8)B; };
    Esperados.Sort(Cmp);
    Tengo.Sort(Cmp);

    if (Esperados != Tengo)
    {
        UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] composicion no coincide con la receta"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[ZonaEmplatado] RECETA COMPLETA (+%d puntos)"), Receta->PuntosCompletar);
    GS->SumarPuntosMC(Receta->PuntosCompletar);

    PlatoActual.Empty();
    OnRep_PlatoActual();

    // §15: la partida finaliza al completar la receta.
    GS->FinalizarPartidaMC();

    // --- Alternativa modo continuo (comentar la linea de arriba y descomentar esto):
    // if (GS->Recetas.Num() > 0)
    //     GS->SetRecetaActivaMC((GS->RecetaActivaIndex + 1) % GS->Recetas.Num());
}

void AZonaEmplatado::OnRep_PlatoActual()
{
    OnPlatoActualizado();
}
