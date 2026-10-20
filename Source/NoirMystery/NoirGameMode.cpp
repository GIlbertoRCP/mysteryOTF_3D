#include "NoirGameMode.h"

#include "NoirCharacter.h"
#include "NoirHUD.h"
#include "NoirPlayerController.h"

ANoirGameMode::ANoirGameMode()
{
	DefaultPawnClass = ANoirCharacter::StaticClass();
	HUDClass = ANoirHUD::StaticClass();
	PlayerControllerClass = ANoirPlayerController::StaticClass();
}

ANoirMenuGameMode::ANoirMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ANoirPlayerController::StaticClass();
}
