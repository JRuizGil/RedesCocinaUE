#include "Cocina/GameStateCocina.h"
#include "Cocina/RecetaAsset.h"
#include "FusionOnlineSubsystem.h"
#include "Net/UnrealNetwork.h"

AGameStateCocina::AGameStateCocina()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
}

void AGameStateCocina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGameStateCocina, StartTimestamp);
    DOREPLIFETIME(AGameStateCocina, DuracionPartida);
    DOREPLIFETIME(AGameStateCocina, EstadoPartida);
    DOREPLIFETIME(AGameStateCocina, Puntuacion);
    DOREPLIFETIME(AGameStateCocina, RecetaActivaIndex);
}

bool AGameStateCocina::EnsureMC(const TCHAR* Op) const
{
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (Fusion && Fusion->IsMasterClient()) return true;
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] %s ignorado: no soy MC"), Op);
    return false;
}

URecetaAsset* AGameStateCocina::GetRecetaActiva() const
{
    if (RecetaActivaIndex < 0 || RecetaActivaIndex >= Recetas.Num()) return nullptr;
    return Recetas[RecetaActivaIndex];
}

void AGameStateCocina::IniciarPartidaMC()
{
    if (!EnsureMC(TEXT("IniciarPartidaMC"))) return;
    if (EstadoPartida != EEstadoPartida::Esperando) return;

    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    StartTimestamp = Fusion->NetworkTime();
    EstadoPartida = EEstadoPartida::EnCurso;
    Puntuacion = 0;
    OnRep_StartTimestamp();
    OnRep_EstadoPartida();
    OnRep_Puntuacion();
}

void AGameStateCocina::SumarPuntosMC(int32 Delta)
{
    if (!EnsureMC(TEXT("SumarPuntosMC"))) return;
    if (EstadoPartida != EEstadoPartida::EnCurso) return;
    Puntuacion += Delta;
    OnRep_Puntuacion();
}

void AGameStateCocina::SetRecetaActivaMC(int32 NewIndex)
{
    if (!EnsureMC(TEXT("SetRecetaActivaMC"))) return;
    RecetaActivaIndex = NewIndex;
    OnRep_RecetaActiva();
}

void AGameStateCocina::FinalizarPartidaMC()
{
    if (!EnsureMC(TEXT("FinalizarPartidaMC"))) return;
    if (EstadoPartida != EEstadoPartida::EnCurso) return;
    EstadoPartida = EEstadoPartida::Finalizada;
    OnRep_EstadoPartida();
}

float AGameStateCocina::GetTiempoRestante() const
{
    if (EstadoPartida == EEstadoPartida::Esperando) return DuracionPartida;
    if (EstadoPartida == EEstadoPartida::Finalizada) return 0.f;
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || StartTimestamp < 0) return DuracionPartida;
    const double Elapsed = Fusion->NetworkTime() - StartTimestamp;
    return FMath::Max(0.f, DuracionPartida - static_cast<float>(Elapsed));
}

void AGameStateCocina::OnRep_StartTimestamp() {}
void AGameStateCocina::OnRep_EstadoPartida() {}
void AGameStateCocina::OnRep_Puntuacion() {}
void AGameStateCocina::OnRep_RecetaActiva() {}
