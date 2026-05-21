#include "Cocina/PlayerCocina.h"
#include "Cocina/CocinaGameInstance.h"
#include "Cocina/CocinaInteractor.h"
#include "FusionActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
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

    Interactor = CreateDefaultSubobject<UCocinaInteractor>(TEXT("Interactor"));

    bReplicates = true;
}

void APlayerCocina::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (InteractAction)
        {
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCocina::OnInteractTriggered);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[PlayerCocina] InteractAction no asignado en BP. La tecla E no funcionara."));
        }
    }
}

void APlayerCocina::OnInteractTriggered()
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerCocina] IA_Interact disparado en %s (local=%d)"),
           *GetNameSafe(this), IsLocallyControlled() ? 1 : 0);
    if (Interactor)
    {
        Interactor->OnInteractPressed();
    }
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