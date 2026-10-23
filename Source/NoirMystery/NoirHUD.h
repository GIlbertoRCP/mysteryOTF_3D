#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NoirHUD.generated.h"

/**
 * The in-world HUD, drawn with canvas in the game's shared style (NoirStyle.h): crosshair,
 * prompts, messages, the interview, the accusation and dossier, the board's hints.
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

	// Drawing in a 1080-tall design space (VW x VH), scaled by S to the screen.
	struct FSlateFontInfo HudFont(float Scale) const;
	float TextWidth(const FString& Text, float Scale = 1.f) const;
	void Txt(const FString& Text, const FLinearColor& Color, float X, float Y, class UFont* Unused = nullptr, float Scale = 1.f);
	void Box(const FLinearColor& Color, float X, float Y, float W, float H);
	/** A key legend in a little dark cap; returns its width. */
	float KeyCap(const FString& Key, float X, float Y, float Scale = 1.f);
	/** A keycap and its label: "[1] Where were you...". */
	void Option(const FString& Key, const FString& Label, float X, float Y, const FLinearColor& Color, float Scale = 1.f);
	/** A centred row of key hints: [Key] label   [Key] label ... */
	void Hints(const TArray<TPair<FString, FString>>& Keys, float Y, const FLinearColor& Color = FLinearColor(0.88f, 0.82f, 0.70f));
	/** A soft-edged dark band behind text over the world. */
	void Backdrop(float CenterX, float Top, float Width, float Height, float Alpha);
	float S = 1.f, VW = 1920.f, VH = 1080.f;
	void DrawInterview(const class ANoirCharacter& Detective);
	void DrawAccusation(const class ANoirCharacter& Detective);
	void DrawDossier(const class ANoirCharacter& Detective);
	void DrawBoard(const class ANoirCharacter& Detective);

	/** A card at the top right as each new piece of evidence is found; the breath bar. */
	void DrawNewEvidence(const class ANoirCharacter& Detective);
	void DrawBreath(const class ANoirCharacter& Detective, float CX, float CY);
	/** The case's tally in the Mind Place's corner. */
	void DrawMindTally(const class ANoirCharacter& Detective);

	TWeakObjectPtr<class ANoirCaseDirector> Director;
	/** Evidence known last frame (-1 before the first), and the card for the newest. */
	int32 KnownCount = -1;
	FString ToastTitle;
	int32 ToastMore = 0;
	float ToastAt = -100.f;
	float BreathAlpha = 0.f;
};
