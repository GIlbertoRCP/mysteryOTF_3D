#include "NoirSave.h"

#include "Kismet/GameplayStatics.h"

bool UNoirSaveGame::Exists()
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

UNoirSaveGame* UNoirSaveGame::Load()
{
	return Cast<UNoirSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
}

void UNoirSaveGame::Delete()
{
	if (Exists())
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}
}

bool UNoirSaveGame::Write()
{
	SavedAt = FDateTime::Now();
	return UGameplayStatics::SaveGameToSlot(this, Slot, 0);
}
