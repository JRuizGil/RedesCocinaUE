#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CocinaConnectTest.generated.h"

UCLASS()
class REDESCOCINAUE_API UCocinaConnectTest : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Cocina|Test", meta = (WorldContext = "WorldContextObject"))
    static void TryConnectToPhoton(UObject* WorldContextObject);
};