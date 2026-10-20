#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NoirHUD.generated.h"

/**
 * Placeholder HUD drawn with canvas: crosshair dot, interaction prompt, search messages,
 * and the clue list (Tab). To be replaced by UMG and the physical case board.
 */
UCLASS()
class NOIRMYSTERY_API ANoirHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "HUD")
	float MessageSeconds = 4.f;

private:
	void DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale = 1.f);
	void DrawInterview(const class ANoirCharacter& Detective);
	void DrawAccusation(const class ANoirCharacter& Detective);
	void DrawDossier(const class ANoirCharacter& Detective);
	void DrawBoard(const class ANoirCharacter& Detective);

	TWeakObjectPtr<class ANoirCaseDirector> Director;
};
