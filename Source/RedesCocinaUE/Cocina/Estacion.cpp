#include "Cocina/Estacion.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/GameStateCocina.h"
#include "FusionActorComponent.h"
#include "FusionOnlineSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/EngineTypes.h"
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
    DOREPLIFETIME(AEstacion, NumOcupados);
    DOREPLIFETIME(AEstacion, NumListos);
    DOREPLIFETIME(AEstacion, ProximoListo);
}

void AEstacion::BeginPlay()
{
    Super::BeginPlay();
    SlotsMC.Init(nullptr, FMath::Max(1, MaxIngredientes));
    UpdateColorByEstado();
}

void AEstacion::Tick(float Dt)
{
    Super::Tick(Dt);

    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    const double Now = Fusion->NetworkTime();
    bool bChanged = false;

    for (TObjectPtr<AIngrediente>& Slot : SlotsMC)
    {
        AIngrediente* Ing = Slot;
        if (!Ing) continue;
        if (Ing->Estado == EEstadoIngrediente::Procesado) continue;
        if (Ing->ProcInicio < 0.0 || Ing->ProcDuracion <= 0.f) continue;

        if (Now - Ing->ProcInicio >= Ing->ProcDuracion)
        {
            Ing->SetProcesadoMC();
            if (AGameStateCocina* GS = GetWorld()->GetGameState<AGameStateCocina>())
            {
                GS->SumarPuntosMC(PuntosOtorgados);
            }
            bChanged = true;
            UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: ingrediente LISTO (+%d)"),
                   *GetNameSafe(this), PuntosOtorgados);
        }
    }

    if (bChanged) RecalcularConteoMC();
}

// ---------------- Cliente local: peticiones ----------------

void AEstacion::RequestDepositar(APlayerCocina* Player, AIngrediente* Ing)
{
    if (!Player || !Ing) return;

    if (Ing->Holder != Player)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: deposito rechazado: no soy holder de %s"),
               *GetNameSafe(this), *GetNameSafe(Ing));
        return;
    }
    if (Ing->Tipo != TipoEsperado)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: deposito rechazado: tipo %d != esperado %d"),
               *GetNameSafe(this), (int32)Ing->Tipo, (int32)TipoEsperado);
        return;
    }
    if (NumOcupados >= MaxIngredientes)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: deposito rechazado: estacion llena (%d)"),
               *GetNameSafe(this), NumOcupados);
        return;
    }

    // Suelta el ingrediente (libera Holder/ownership). El MC lo colocara en su hueco.
    Ing->RequestDrop(Player);

    // Comunica la intencion al MC por el canal replicado del pawn.
    Player->SubmitDepositarEstacion(this, Ing);

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: deposito enviado al MC (Ing=%s)"),
           *GetNameSafe(this), *GetNameSafe(Ing));
}

void AEstacion::RequestRecoger(APlayerCocina* Player)
{
    if (!Player) return;
    if (NumListos <= 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: recoger sin nada listo"), *GetNameSafe(this));
        return;
    }

    // NO hacemos pickup local optimista: el ingrediente lo posee el MC, asi que el
    // cliente no puede escribir su Holder (no replicaria). Es el MC quien entrega el
    // ingrediente de forma autoritativa y la replicacion de Holder lo engancha a la
    // mano en TODOS los clientes (incluido el MC). El cliente que recoge reclama
    // ownership en AIngrediente::OnRep_Holder al recibirlo.
    Player->SubmitRecogerEstacion(this);
    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: recoger enviado al MC"), *GetNameSafe(this));
}

bool AEstacion::PuedeDepositar(ETipoIngrediente Tipo) const
{
    return Tipo == TipoEsperado && NumOcupados < MaxIngredientes;
}

// ---------------- Logica autoritativa (solo MC) ----------------

void AEstacion::MC_HandleDepositar(AIngrediente* Ing)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;

    if (!Ing)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Estacion] MC_HandleDepositar rechazado: Ing=null"));
        return;
    }
    if (Ing->Tipo != TipoEsperado) return;
    if (Ing->Estado == EEstadoIngrediente::Procesado) return;

    // Busca el primer hueco libre.
    int32 Slot = INDEX_NONE;
    for (int32 i = 0; i < SlotsMC.Num(); ++i)
    {
        if (SlotsMC[i] == nullptr) { Slot = i; break; }
    }
    if (Slot == INDEX_NONE)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] MC_HandleDepositar rechazado: llena"));
        return;
    }

    // El MC toma ownership para colocar/cronometrar el ingrediente de forma autoritativa.
    UFusionOnlineSubsystem::SetWantsOwner(Ing, true);

    SlotsMC[Slot] = Ing;
    Ing->SetEnEstacionMC(SlotWorldLocation(Slot), GetActorRotation(), TiempoProcesado, Fusion->NetworkTime());

    RecalcularConteoMC();

    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: ingrediente en hueco %d (ocupados=%d)"),
           *GetNameSafe(this), Slot, NumOcupados);
}

void AEstacion::MC_HandleRecoger(APlayerCocina* Player)
{
    UGameInstance* GI = GetGameInstance();
    UFusionOnlineSubsystem* Fusion = GI ? GI->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || !Fusion->IsMasterClient()) return;
    if (!Player) return;

    // Entrega el ingrediente recogible (ProximoListo) al jugador. Guard: si ya tiene
    // Holder, otra peticion concurrente lo entrego primero -> ignora (anti doble-recoger).
    AIngrediente* Ing = ProximoListo;
    if (!Ing || Ing->Holder != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: recoger ignorado (nada listo o ya entregado)"),
               *GetNameSafe(this));
        return;
    }

    // Libera el hueco que ocupaba en la lista MC.
    for (int32 i = 0; i < SlotsMC.Num(); ++i)
    {
        if (SlotsMC[i] == Ing)
        {
            SlotsMC[i] = nullptr;
            break;
        }
    }

    // Entrega autoritativa: el MC (owner) fija Holder -> replica a todos los clientes.
    Ing->EntregarAJugadorMC(Player);

    RecalcularConteoMC();
    UE_LOG(LogTemp, Log, TEXT("[Estacion] %s: %s entregado a %s (ocupados=%d)"),
           *GetNameSafe(this), *GetNameSafe(Ing), *GetNameSafe(Player), NumOcupados);
}

void AEstacion::RecalcularConteoMC()
{
    int32 Ocupados = 0;
    int32 Listos = 0;
    AIngrediente* PrimerListo = nullptr;

    for (TObjectPtr<AIngrediente>& Slot : SlotsMC)
    {
        AIngrediente* Ing = Slot;
        if (!Ing) continue;
        ++Ocupados;
        if (Ing->Estado == EEstadoIngrediente::Procesado)
        {
            ++Listos;
            if (!PrimerListo) PrimerListo = Ing;
        }
    }

    NumOcupados = Ocupados;
    NumListos = Listos;
    ProximoListo = PrimerListo;
    OnRep_Conteo();
}

FVector AEstacion::SlotWorldLocation(int32 SlotIndex) const
{
    // Rejilla 2x2 sobre la cara superior. El cubo base mide TamCuboBase; la escala del
    // actor (p.ej. 1,1,0.5) define la altura y el ancho reales.
    const FVector S = GetActorScale3D();
    const float TopZ  = 0.5f * TamCuboBase * S.Z;   // cara superior relativa al origen
    const float HalfX = 0.5f * TamCuboBase * S.X;
    const float HalfY = 0.5f * TamCuboBase * S.Y;
    const float Gx = HalfX * 0.5f;                  // centros de la rejilla a +-1/4 del lado
    const float Gy = HalfY * 0.5f;

    const FVector Local[4] = {
        FVector(+Gx, +Gy, TopZ),
        FVector(+Gx, -Gy, TopZ),
        FVector(-Gx, +Gy, TopZ),
        FVector(-Gx, -Gy, TopZ),
    };
    const int32 Idx = FMath::Clamp(SlotIndex, 0, 3);

    // Respeta la rotacion del actor; la escala ya esta incorporada en los offsets.
    return GetActorLocation()
        + GetActorRotation().RotateVector(Local[Idx])
        + FVector(0, 0, AlturaIngrediente);
}

void AEstacion::OnRep_Conteo()
{
    // Estado solo-color derivado de los conteos.
    if (NumOcupados == 0)        Estado = EEstadoEstacion::Libre;
    else if (NumListos > 0)      Estado = EEstadoEstacion::Listo;
    else                         Estado = EEstadoEstacion::Procesando;

    UpdateColorByEstado();
}

void AEstacion::UpdateColorByEstado()
{
    if (!Mesh) return;
    UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;

    FLinearColor C = FLinearColor::Green;
    switch (Estado)
    {
        case EEstadoEstacion::Libre:      C = FLinearColor(0.2f, 0.9f, 0.2f);   break; // verde
        case EEstadoEstacion::Procesando: C = FLinearColor(1.0f, 0.85f, 0.0f);  break; // amarillo
        case EEstadoEstacion::Listo:      C = FLinearColor(0.9f, 0.15f, 0.15f); break; // rojo
    }
    MID->SetVectorParameterValue(TEXT("BaseColor"), C);
}
