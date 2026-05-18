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
}

AActor* ACocinaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Starts.Add(*It);
    if (Starts.Num() == 0) return Super::ChoosePlayerStart_Implementation(Player);

    APlayerStart* Pick = Starts[NextStartIndex % Starts.Num()];
    NextStartIndex++;
    return Pick;
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
                }
            }
        }), 3.0f, false);
    }
}
