#include "Cocina/Ingrediente.h"
#include "Cocina/PlayerCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AIngrediente::AIngrediente()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::Transaction;
}

void AIngrediente::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AIngrediente, Estado);
    DOREPLIFETIME(AIngrediente, Holder);
}

void AIngrediente::BeginPlay()
{
    Super::BeginPlay();

    if (FusionComp)
    {
        FusionComp->OnOwnerWasGiven.AddDynamic(this, &AIngrediente::HandleOwnerGiven);
        UE_LOG(LogTemp, Log, TEXT("[Ingrediente] %s: delegate OnOwnerWasGiven enganchado"), *GetNameSafe(this));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] %s: FusionComp null en BeginPlay"), *GetNameSafe(this));
    }
    UpdateVisualByEstado();
}

bool AIngrediente::RequestPickup(APlayerCocina* Requester)
{
    if (!Requester)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] RequestPickup rechazado: Requester=null"));
        return false;
    }
    if (Holder)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] RequestPickup rechazado: ya tiene Holder=%s"),
               *GetNameSafe(Holder));
        return false;
    }

    const float Dist = FVector::Dist(GetActorLocation(), Requester->GetActorLocation());
    if (Dist > MaxPickupDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] RequestPickup rechazado: Dist=%.1f > Max=%.1f"),
               Dist, MaxPickupDistance);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[Ingrediente] RequestPickup ok. Dist=%.1f. SetWantsOwner(true)..."), Dist);
    UFusionOnlineSubsystem::SetWantsOwner(this, true);

    // Workaround: en Fusion Shared con actores placed-in-level, OnOwnerWasGiven
    // no siempre dispara. Forzamos el attach inmediato. La replicacion de Holder
    // se encarga de mostrarlo en los demas clientes cuando la transferencia llega.
    Holder = Requester;
    OnRep_Holder();
    return true;
}

bool AIngrediente::RequestDrop(APlayerCocina* Requester)
{
    if (!UFusionOnlineSubsystem::IsOwner(this)) return false;
    if (Holder != Requester) return false;

    Holder = nullptr;
    OnRep_Holder();
    UFusionOnlineSubsystem::SetWantsOwner(this, false);
    return true;
}

void AIngrediente::HandleOwnerGiven()
{
    UE_LOG(LogTemp, Log, TEXT("[Ingrediente] HandleOwnerGiven en %s"), *GetNameSafe(this));

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APlayerCocina* MyPawn = Cast<APlayerCocina>(PC->GetPawn()))
            {
                UE_LOG(LogTemp, Log, TEXT("[Ingrediente] Asignando Holder=%s"), *GetNameSafe(MyPawn));
                Holder = MyPawn;
                OnRep_Holder();
                return;
            }
            UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] PC sin Pawn o no es APlayerCocina"));
        }
    }
}

void AIngrediente::SetProcesadoMC()
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado == EEstadoIngrediente::Procesado) return;

    Estado = EEstadoIngrediente::Procesado;
    OnRep_Estado();
}

void AIngrediente::OnRep_Estado()
{
    UpdateVisualByEstado();
}

void AIngrediente::OnRep_Holder()
{
    UE_LOG(LogTemp, Log, TEXT("[Ingrediente] OnRep_Holder en %s. Holder=%s"),
           *GetNameSafe(this), *GetNameSafe(Holder));

    if (Holder)
    {
        AttachToHolder();
    }
    else
    {
        DetachFromHolder();
    }
}

void AIngrediente::AttachToHolder()
{
    if (!Holder) return;
    USkeletalMeshComponent* HolderMesh = Holder->GetMesh();

    if (!HolderMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Ingrediente] Holder sin SkeletalMesh, attach al root"));
        AttachToActor(Holder, FAttachmentTransformRules::KeepRelativeTransform);
        SetActorRelativeLocation(FVector(50, 0, 50));
    }
    else
    {
        const bool bSocketExists = HolderMesh->DoesSocketExist(HandSocketName);
        UE_LOG(LogTemp, Log, TEXT("[Ingrediente] Attach a %s socket '%s' existe=%d"),
               *GetNameSafe(HolderMesh), *HandSocketName.ToString(), bSocketExists ? 1 : 0);

        // Snap a la posicion/rotacion del socket, pero conserva la escala mundial previa
        // (asi el ingrediente no se deforma por la escala del bone de la mano).
        const FAttachmentTransformRules AttachRules(
            EAttachmentRule::SnapToTarget,  // Location
            EAttachmentRule::SnapToTarget,  // Rotation
            EAttachmentRule::KeepWorld,     // Scale
            false /* WeldSimulatedBodies */);
        AttachToComponent(HolderMesh, AttachRules, HandSocketName);

        UE_LOG(LogTemp, Log, TEXT("[Ingrediente] Tras attach, mundo=%s"),
               *GetActorLocation().ToString());
    }
    if (Mesh)
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void AIngrediente::DetachFromHolder()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    if (Mesh)
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void AIngrediente::UpdateVisualByEstado()
{
    if (!Mesh) return;
    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;
    const FLinearColor Col = (Estado == EEstadoIngrediente::Procesado) ? ColorProcesado : ColorCrudo;
    MID->SetVectorParameterValue(TEXT("BaseColor"), Col);
}
