#include "Cocina/Estacion.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
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

void AEstacion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
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

    // Suelta el ingrediente localmente: dispara replicacion de Holder=null + posicion sobre la estacion.
    Ing->RequestDrop(Player);
    Ing->SetActorLocation(GetActorLocation() + FVector(0, 0, 60.f));

    // El FusionNetDriver enruta este Server RPC al owner del actor (MC, por Ownership=MasterClient).
    Server_TryDepositar(Ing);

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestDepositar enviado al MC (Ing=%s)"),
           *GetNameSafe(this), *GetNameSafe(Ing));
}

void AEstacion::RequestRecoger(APlayerCocina* Player)
{
    if (!Player) return;
    if (Estado != EEstadoEstacion::Listo) return;
    Server_TryRecoger(Player);
    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: RequestRecoger enviado al MC"), *GetNameSafe(this));
}

// ---------------- Server RPCs (se ejecutan en el MC) ----------------

void AEstacion::Server_TryDepositar_Implementation(AIngrediente* Ing)
{
    MC_TryStartProcesado(Ing);
}

void AEstacion::Server_TryRecoger_Implementation(APlayerCocina* Player)
{
    MC_TryEntregarAJugador(Player);
}

// ---------------- Logica autoritativa (solo MC) ----------------

void AEstacion::MC_TryStartProcesado(AIngrediente* Ing)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (Estado != EEstadoEstacion::Libre)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_TryStartProcesado rechazado: estado=%d"), (int32)Estado);
        return;
    }
    if (!Ing)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Estacion] MC_TryStartProcesado rechazado: Ing=null"));
        return;
    }
    if (Ing->Tipo != TipoEsperado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_TryStartProcesado rechazado: tipo %d != esperado %d"),
               (int32)Ing->Tipo, (int32)TipoEsperado);
        return;
    }
    if (Ing->Estado == EEstadoIngrediente::Procesado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_TryStartProcesado rechazado: ya procesado"));
        return;
    }

    // El MC toma ownership del ingrediente para que nadie lo recoja durante el procesado.
    UFusionOnlineSubsystem::SetWantsOwner(Ing, true);

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
    }

    OnRep_Estado();

    if (AGameStateCocina* GS = GetWorld()->GetGameState<AGameStateCocina>())
    {
        GS->SumarPuntosMC(PuntosOtorgados);
    }

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: procesado COMPLETADO (+%d puntos)"),
           *GetNameSafe(this), PuntosOtorgados);
}

void AEstacion::MC_TryEntregarAJugador(APlayerCocina* Player)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (Estado != EEstadoEstacion::Listo) return;
    if (!IngredienteActual) return;
    if (!Player) return;

    AIngrediente* Ing = IngredienteActual;

    // Libera ownership MC del ingrediente y asigna Holder al jugador.
    // La replicacion de Holder hara que el cliente del Player ejecute AttachToHolder.
    UFusionOnlineSubsystem::SetWantsOwner(Ing, false);
    Ing->Holder = Player;

    IngredienteActual = nullptr;
    StartTimestamp = -1.0;
    Estado = EEstadoEstacion::Libre;

    OnRep_IngredienteActual();
    OnRep_Estado();

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: ENTREGADO a %s"),
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
    // Hook visual opcional (efectos de "humo", luces, etc.). De momento, no-op.
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
