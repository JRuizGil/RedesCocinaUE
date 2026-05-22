#include "Cocina/CocinaInteractor.h"
#include "Cocina/Ingrediente.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/Estacion.h"
#include "Cocina/ZonaEmplatado.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

UCocinaInteractor::UCocinaInteractor()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCocinaInteractor::BeginPlay()
{
    Super::BeginPlay();
}

void UCocinaInteractor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DeltaTime, TickType, Fn);

    // Solo el cliente local del jugador hace raycast.
    APlayerCocina* Pawn = Cast<APlayerCocina>(GetOwner());
    if (!Pawn || !Pawn->IsLocallyControlled()) return;

    UpdateActorEnfocado();
}

void UCocinaInteractor::UpdateActorEnfocado()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World) return;

    const FVector Start = OwnerActor->GetActorLocation() + FVector(0, 0, TraceHeightOffset);
    const FVector Fwd   = OwnerActor->GetActorForwardVector();
    const FVector End   = Start + Fwd * TraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CocinaInteract), false, OwnerActor);
    Params.AddIgnoredActor(OwnerActor);

    // Sphere trace: mas tolerante que LineTrace para objetos pequenyos a alturas variables.
    const bool bHit = World->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(TraceRadius), Params);

    ActorEnfocado = bHit ? Hit.GetActor() : nullptr;

#if !UE_BUILD_SHIPPING
    const FColor C = bHit ? FColor::Green : FColor::Red;
    DrawDebugLine(World, Start, End, C, false, -1.f, 0, 1.f);
    DrawDebugSphere(World, End, TraceRadius, 12, C, false, -1.f, 0, 1.f);
    if (bHit && Hit.GetActor())
    {
        DrawDebugSphere(World, Hit.ImpactPoint, 10.f, 8, FColor::Yellow, false, -1.f, 0, 1.f);
    }
#endif
}

void UCocinaInteractor::OnInteractPressed()
{
    APlayerCocina* OwnerPawn = Cast<APlayerCocina>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

    AActor* Focused = ActorEnfocado.Get();
    AIngrediente* Held = GetHeldIngrediente();

    UE_LOG(LogTemp, Log, TEXT("[Interactor] E pulsada. Enfocado=%s, Held=%s"),
           *GetNameSafe(Focused), *GetNameSafe(Held));

    // 1) Mirando una estacion -> depositar (si llevo algo) o recoger (si esta Listo).
    if (AEstacion* Est = Cast<AEstacion>(Focused))
    {
        if (Held)
        {
            Est->RequestDepositar(OwnerPawn, Held);
            UE_LOG(LogTemp, Log, TEXT("[Interactor] Depositar en %s"), *GetNameSafe(Est));
        }
        else if (Est->Estado == EEstadoEstacion::Listo)
        {
            Est->RequestRecoger(OwnerPawn);
            UE_LOG(LogTemp, Log, TEXT("[Interactor] Recoger de %s"), *GetNameSafe(Est));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[Interactor] Estacion %s no aceptable (estado=%d, manos vacias)"),
                   *GetNameSafe(Est), (int32)Est->Estado);
        }
        return;
    }

    // 2) Mirando la zona de emplatado con un ingrediente procesado -> emplatar.
    if (AZonaEmplatado* Zona = Cast<AZonaEmplatado>(Focused))
    {
        if (Held)
        {
            Zona->RequestDepositarPlato(OwnerPawn, Held);
            UE_LOG(LogTemp, Log, TEXT("[Interactor] Emplatar en %s"), *GetNameSafe(Zona));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[Interactor] Zona de emplatado pero manos vacias"));
        }
        return;
    }

    // 3) Llevo algo en mano y no miro estacion/zona -> soltar.
    if (Held)
    {
        const bool bOk = Held->RequestDrop(OwnerPawn);
        UE_LOG(LogTemp, Log, TEXT("[Interactor] RequestDrop -> %s"), bOk ? TEXT("OK") : TEXT("rechazado"));
        return;
    }

    // 4) Miro un ingrediente suelto -> coger.
    if (AIngrediente* Ing = Cast<AIngrediente>(Focused))
    {
        const bool bOk = Ing->RequestPickup(OwnerPawn);
        UE_LOG(LogTemp, Log, TEXT("[Interactor] RequestPickup(%s) -> %s"),
               *GetNameSafe(Ing), bOk ? TEXT("OK") : TEXT("rechazado"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Interactor] Nada que hacer (no enfocado, no held)."));
}

AIngrediente* UCocinaInteractor::GetHeldIngrediente() const
{
    APlayerCocina* Pawn = Cast<APlayerCocina>(GetOwner());
    if (!Pawn) return nullptr;
    UWorld* World = Pawn->GetWorld();
    if (!World) return nullptr;

    for (TActorIterator<AIngrediente> It(World); It; ++It)
    {
        if (It->Holder == Pawn) return *It;
    }
    return nullptr;
}
