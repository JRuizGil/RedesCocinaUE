#include "Cocina/PlayerCocina.h"
#include "Cocina/CocinaGameInstance.h"
#include "FusionActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

APlayerCocina::APlayerCocina()
{
    FusionComp = CreateDefaultSubobject<UFusionActorComponent>(TEXT("FusionComp"));
    FusionComp->Ownership = EFusionObjectOwnerFlags::Transaction;
    // El owner natural es el jugador que controla este Pawn. Fusion lo gestiona via PlayerController.

    NameplateComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
    NameplateComp->SetupAttachment(RootComponent);
    NameplateComp->SetRelativeLocation(FVector(0, 0, 110));
    NameplateComp->SetWidgetSpace(EWidgetSpace::Screen);
    NameplateComp->SetDrawSize(FVector2D(180, 36));

    bReplicates = true;
}

void APlayerCocina::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APlayerCocina, PlayerName);
}

void APlayerCocina::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocallyControlled())
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
                {
                    if (MappingContexts.Num() == 0)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[PlayerCocina] MappingContexts vacio en BP. Asignar al menos IMC_Default e IMC_MouseLook."));
                    }
                    for (UInputMappingContext* IMC : MappingContexts)
                    {
                        if (IMC)
                        {
                            Subsystem->AddMappingContext(IMC, MappingPriority);
                        }
                    }
                }
            }

            // Asegura modo Game para que el input llegue al pawn (el menu lo dejaba en UI Only).
            FInputModeGameOnly GameOnly;
            PC->SetInputMode(GameOnly);
            PC->bShowMouseCursor = false;
        }
    }

    RefreshNameplate();
}

void APlayerCocina::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    // Solo el cliente local del jugador empuja su nombre al estado de red.
    if (IsLocallyControlled())
    {
        SetMyNameFromGameInstance();
    }
}

void APlayerCocina::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (IsLocallyControlled())
    {
        SetMyNameFromGameInstance();
    }
}

void APlayerCocina::SetMyNameFromGameInstance()
{
    UCocinaGameInstance* GI = Cast<UCocinaGameInstance>(GetGameInstance());
    if (!GI) return;
    if (PlayerName == GI->PlayerDisplayName) return;
    PlayerName = GI->PlayerDisplayName;
    OnRep_PlayerName(); // refresca local; los remotos lo recibir�n por replicaci�n
}

void APlayerCocina::OnRep_PlayerName()
{
    RefreshNameplate();
    OnPlayerNameUpdated(PlayerName);
}


void APlayerCocina::RefreshNameplate()
{
    // Implementaci�n BP: el widget tiene una variable Text "DisplayedName".
    // Aqu� podr�amos hacer el cast a UUserWidget; lo dejamos al Blueprint derivado para simplicidad.
    if (NameplateComp)
    {
        NameplateComp->SetVisibility(true);
    }
}