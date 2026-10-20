#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NoirGameMode.generated.h"

UCLASS()
class NOIRMYSTERY_API ANoirGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoirGameMode();
};

/** The title screen: no detective, just the camera and the menu. */
UCLASS()
class NOIRMYSTERY_API ANoirMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoirMenuGameMode();
};
