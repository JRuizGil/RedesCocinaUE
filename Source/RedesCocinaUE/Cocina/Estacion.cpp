#include "Cocina/Estacion.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AEstacion::AEstacion()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::MasterClient;
}

void AEstacion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEstacion, Estado);
    DOREPLIFETIME(AEstacion, IngredienteActual);
    DOREPLIFETIME(AEstacion, StartTimestamp);
}

void AEstacion::BeginPlay()
{
    Super::BeginPlay();
    UpdateColorByEstado();
}

void AEstacion::Tick(float Dt)
{
    Super::Tick(Dt);

    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (Estado != EEstadoEstacion::Procesando) return;
    if (StartTimestamp < 0.0) return;

    const double Elapsed = Fusion->NetworkTime() - StartTimestamp;
    if (Elapsed >= TiempoProcesado)
    {
        MC_FinishProcesado();
    }
}

// ---------------- Cliente local: peticiones ----------------

void AEstacion::RequestDepositar(APlayerCocina* Player, AIngrediente* Ing)
{
    if (!Player || !Ing) return;

    // Pre-validacion local: evita el bug de "patata aparece en estacion equivocada"
    // cuando el cliente tenia Holder=Player por una replicacion atrasada del MC.
    if (Ing->Holder != Player)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestDepositar rechazado: no soy holder de %s"),
               *GetNameSafe(this), *GetNameSafe(Ing));
        return;
    }
    if (Estado != EEstadoEstacion::Libre)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestDepositar rechazado: estado=%d"),
               *GetNameSafe(this), (int32)Estado);
        return;
    }
    if (Ing->Tipo != TipoEsperado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestDepositar rechazado: tipo %d != esperado %d"),
               *GetNameSafe(this), (int32)Ing->Tipo, (int32)TipoEsperado);
        return;
    }

    // Posicion/rotacion donde queremos el ingrediente: encima de la estacion y
    // alineado a ella (no a la mano).
    const FVector PlaceLoc = GetActorLocation() + FVector(0, 0, 60.f);
    const FRotator PlaceRot = GetActorRotation();

    // Suelta el ingrediente localmente: dispara replicacion de Holder=null.
    Ing->RequestDrop(Player);
    // Reposicionar tras RequestDrop sobreescribe el detach (KeepWorldTransform).
    // En el MC se vuelve a fijar de forma autoritativa cuando toma ownership.
    Ing->SetActorLocationAndRotation(PlaceLoc, PlaceRot);

    // Comunica la intencion al MC escribiendo una peticion replicada en el pawn
    // (que el cliente local SI posee). Si el invocador ya es el MC, se ejecuta ya.
    Player->SubmitDepositarEstacion(this, Ing);

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestDepositar enviado al MC (Ing=%s)"),
           *GetNameSafe(this), *GetNameSafe(Ing));
}

void AEstacion::RequestRecoger(APlayerCocina* Player)
{
    if (!Player) return;
    if (Estado != EEstadoEstacion::Listo) return;
    if (!IngredienteActual) return;

    // El jugador coge el ingrediente procesado como un pickup normal: toma ownership
    // y se lo attacha en su propio cliente (asi luego podra soltarlo/depositarlo).
    if (!IngredienteActual->RequestPickup(Player))
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestRecoger: pickup rechazado"), *GetNameSafe(this));
        return;
    }

    // Notifica al MC para que resetee la estacion a Libre.
    Player->SubmitRecogerEstacion(this);
    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestRecoger enviado al MC"), *GetNameSafe(this));
}

// ---------------- Logica autoritativa (solo MC) ----------------

void AEstacion::MC_HandleDepositar(AIngrediente* Ing)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (Estado != EEstadoEstacion::Libre)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_HandleDepositar rechazado: estado=%d"), (int32)Estado);
        return;
    }
    if (!Ing)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Estacion] MC_HandleDepositar rechazado: Ing=null"));
        return;
    }
    if (Ing->Tipo != TipoEsperado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_HandleDepositar rechazado: tipo %d != esperado %d"),
               (int32)Ing->Tipo, (int32)TipoEsperado);
        return;
    }
    if (Ing->Estado == EEstadoIngrediente::Procesado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_HandleDepositar rechazado: ya procesado"));
        return;
    }

    // El MC toma ownership del ingrediente para que nadie lo recoja durante el procesado.
    UFusionOnlineSubsystem::SetWantsOwner(Ing, true);

    // Reposicion autoritativa: si la transform del cliente no llego o no replico,
    // el MC fija el ingrediente sobre la estacion y alineado a ella.
    Ing->SetActorLocationAndRotation(
        GetActorLocation() + FVector(0, 0, 60.f),
        GetActorRotation());

    IngredienteActual = Ing;
    StartTimestamp = Fusion->NetworkTime();
    Estado = EEstadoEstacion::Procesando;

    OnRep_IngredienteActual();
    OnRep_Estado();

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: procesado ARRANCADO @ %.2f"), *GetNameSafe(this), StartTimestamp);
}

void AEstacion::MC_FinishProcesado()
{
    Estado = EEstadoEstacion::Listo;

    if (IngredienteActual)
    {
        IngredienteActual->SetProcesadoMC();
        // Libera ownership: el ingrediente queda "suelto" sobre la estacion para que
        // cualquier jugador pueda cogerlo (RequestPickup tomara ownership limpio).
        UFusionOnlineSubsystem::SetWantsOwner(IngredienteActual, false);
    }

    OnRep_Estado();

    if (AGameStateCocina* GS = GetWorld()->GetGameState<AGameStateCocina>())
    {
        GS->SumarPuntosMC(PuntosOtorgados);
    }

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: procesado COMPLETADO (+%d puntos)"),
           *GetNameSafe(this), PuntosOtorgados);
}

void AEstacion::MC_HandleRecoger(APlayerCocina* Player)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (Estado != EEstadoEstacion::Listo) return;

    // El jugador ya tomo ownership del ingrediente y se lo attacho en su cliente
    // (ver RequestRecoger). Aqui el MC solo resetea la estacion para que vuelva a
    // estar disponible.
    IngredienteActual = nullptr;
    StartTimestamp = -1.0;
    Estado = EEstadoEstacion::Libre;

    OnRep_IngredienteActual();
    OnRep_Estado();

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: reseteada tras recoger de %s"),
           *GetNameSafe(this), *GetNameSafe(Player));
}

float AEstacion::GetProgreso01() const
{
    if (Estado != EEstadoEstacion::Procesando) return 0.f;
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || StartTimestamp < 0.0) return 0.f;
    const double E = Fusion->NetworkTime() - StartTimestamp;
    if (TiempoProcesado <= 0.f) return 1.f;
    return FMath::Clamp(static_cast<float>(E / TiempoProcesado), 0.f, 1.f);
}

void AEstacion::OnRep_Estado()
{
    UpdateColorByEstado();
}

void AEstacion::OnRep_IngredienteActual()
{
    // Mientras hay un ingrediente sobre la estacion (procesando o listo) apagamos
    // su colision en todos los clientes: el sphere trace del Interactor atraviesa
    // el ingrediente y detecta la estacion. Asi pulsar E mirando la zona llama a
    // RequestRecoger (o RequestDepositar) en vez de "robar" via RequestPickup.
    if (IngredienteActual)
    {
        if (UStaticMeshComponent* M = IngredienteActual->GetMesh())
        {
            M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    // Cuando IngredienteActual pasa a null: AttachToHolder (al entregarse) ya
    // gestiona la colision, y al soltarse fuera de estacion DetachFromHolder
    // la restaura a QueryAndPhysics.
}

void AEstacion::UpdateColorByEstado()
{
    if (!Mesh) return;
    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;

    FLinearColor C = FLinearColor::Green;
    switch (Estado)
    {
        case EEstadoEstacion::Libre:      C = FLinearColor(0.2f, 0.9f, 0.2f);  break; // verde
        case EEstadoEstacion::Procesando: C = FLinearColor(1.0f, 0.85f, 0.0f); break; // amarillo
        case EEstadoEstacion::Listo:      C = FLinearColor(0.9f, 0.15f, 0.15f); break; // rojo
    }
    MID->SetVectorParameterValue(TEXT("BaseColor"), C);
}
