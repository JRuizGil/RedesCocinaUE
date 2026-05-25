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
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] IniciarPartidaMC() llamado"));
    if (!EnsureMC(TEXT("IniciarPartidaMC"))) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] IniciarPartidaMC() FALLIDO: No soy MasterClient"));
        return;
    }
    if (EstadoPartida != EEstadoPartida::Esperando) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] IniciarPartidaMC() FALLIDO: EstadoPartida es %d (debe ser 0=Esperando)"), static_cast<int32>(EstadoPartida));
        return;
    }

    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    StartTimestamp = Fusion->NetworkTime();
    EstadoPartida = EEstadoPartida::EnCurso;
    Puntuacion = 0;
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] IniciarPartidaMC() ÉXITO: StartTimestamp=%.2f, EstadoPartida cambiado a 1=EnCurso"), StartTimestamp);
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
    if (EstadoPartida == EEstadoPartida::Esperando) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] GetTiempoRestante() -> EstadoPartida=ESPERANDO, devolviendo 120"));
        return DuracionPartida;
    }
    if (EstadoPartida == EEstadoPartida::Finalizada) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] GetTiempoRestante() -> EstadoPartida=FINALIZADA, devolviendo 0"));
        return 0.f;
    }
    UFusionOnlineSubsystem* Fusion = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>() : nullptr;
    if (!Fusion || StartTimestamp < 0) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] GetTiempoRestante() -> Fusion=%p, StartTimestamp=%.2f -> FALLBACK 120"), Fusion, StartTimestamp);
        return DuracionPartida;
    }
    const double Elapsed = Fusion->NetworkTime() - StartTimestamp;
    const float Restante = FMath::Max(0.f, DuracionPartida - static_cast<float>(Elapsed));
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] GetTiempoRestante() -> NetworkTime=%.2f, StartTS=%.2f, Elapsed=%.2f, Restante=%.2f"), Fusion->NetworkTime(), StartTimestamp, Elapsed, Restante);
    return Restante;
}

void AGameStateCocina::OnRep_StartTimestamp() 
{
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] OnRep_StartTimestamp() -> StartTimestamp=%.2f"), StartTimestamp);
}

void AGameStateCocina::OnRep_EstadoPartida()
{
    UE_LOG(LogTemp, Warning, TEXT("[GameStateCocina] OnRep_EstadoPartida() -> EstadoPartida=%d (0=Esperando, 1=EnCurso, 2=Finalizada)"), static_cast<int32>(EstadoPartida));
    K2_OnEstadoPartidaCambio(EstadoPartida);
}

void AGameStateCocina::OnRep_Puntuacion()
{
    K2_OnPuntuacionCambio(Puntuacion);
}

void AGameStateCocina::OnRep_RecetaActiva()
{
    K2_OnRecetaCambio(RecetaActivaIndex);
}
