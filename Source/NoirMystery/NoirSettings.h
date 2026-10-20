#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NoirSettings.generated.h"

/**
 * The player's own preferences that Unreal's UGameUserSettings does not cover (display mode,
 * resolution and quality live there). Saved to GameUserSettings.ini.
 */
UCLASS(Config = GameUserSettings)
class NOIRMYSTERY_API UNoirSettings : public UObject
{
	GENERATED_BODY()

public:
	static UNoirSettings* Get() { return GetMutableDefault<UNoirSettings>(); }
	void Save() { SaveConfig(); }

	/** Multiplies mouse and stick look. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls")
	float LookSensitivity = 1.f;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls")
	bool bInvertLook = false;

	/** The language new cases (and the menus) are played in. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Case")
	bool bSpanish = false;

	/** Last choices on the New case page. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Case")
	FString Difficulty = TEXT("Detective");

	/** "gothic" (the manor) or "express" (the train). */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Case")
	FString Theme = TEXT("gothic");
};
