#include "Cocina/CocinaGameMode.h"
#include "Cocina/PlayerCocina.h"
#include "Cocina/CocinaGameInstance.h"
#include "Cocina/GameStateCocina.h"
#include "FusionOnlineSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

ACocinaGameMode::ACocinaGameMode()
{
    DefaultPawnClass = APlayerCocina::StaticClass();
    GameStateClass = AGameStateCocina::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

AActor* ACocinaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Starts.Add(*It);
    if (Starts.Num() == 0) return Super::ChoosePlayerStart_Implementation(Player);

    // TActorIterator no garantiza el mismo orden en cada maquina: ordena de forma
    // determinista (por nombre) para que todos los clientes vean la misma lista.
    Starts.Sort([](const APlayerStart& A, const APlayerStart& B)
    {
        return A.GetName() < B.GetName();
    });

    // En Fusion shared mode el GameMode corre en CADA cliente y cada uno spawnea su
    // propio pawn. Un contador local (NextStartIndex) arranca en 0 en todos -> todos
    // elegirian el mismo PlayerStart. Indexamos por el numero de jugador de Photon,
    // que es unico y estable (master = 1, siguiente = 2, ...).
    int32 Index = NextStartIndex++ % Starts.Num(); // fallback offline/PIE
    if (UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>())
    {
        const int32 PlayerNumber = Fusion->GetLocalPlayerId(); // 1, 2, 3, ...
        if (PlayerNumber > 0)
        {
            Index = (PlayerNumber - 1) % Starts.Num();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[CocinaGameMode] ChoosePlayerStart -> %s (index %d de %d)"),
           *GetNameSafe(Starts[Index]), Index, Starts.Num());
    return Starts[Index];
}

void ACocinaGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;

    AGameStateCocina* GS = GetGameState<AGameStateCocina>();
    if (!GS || GS->EstadoPartida != EEstadoPartida::EnCurso) return;
    
    if (GS->GetTiempoRestante() <= 0.f)
    {
        GS->FinalizarPartidaMC();
    }
}

void ACocinaGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UFusionOnlineSubsystem* Fusion = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>();
    if (!Fusion || !Fusion->IsMasterClient()) return;

    AGameStateCocina* GS = GetGameState<AGameStateCocina>();
    if (!GS) return;

    if (GS->EstadoPartida == EEstadoPartida::Esperando && Fusion->PlayerCount() >= 1)
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([this]
        {
            if (UFusionOnlineSubsystem* F = GetGameInstance()->GetSubsystem<UFusionOnlineSubsystem>())
            {
                if (!F->IsMasterClient()) return;
                if (AGameStateCocina* G = GetGameState<AGameStateCocina>())
                {
                    G->IniciarPartidaMC();
                    G->SetRecetaActivaMC(0); // arranca con la primera receta del catalogo
                }
            }
        }), 3.0f, false);
    }
}
