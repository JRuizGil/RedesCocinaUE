#include "Cocina/CocinaGameInstance.h"

bool UCocinaGameInstance::SetPlayerName(const FString& Raw)
{
    FString Trimmed = Raw.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || Trimmed.Len() > 16) return false;
    PlayerDisplayName = Trimmed;
    return true;
}
