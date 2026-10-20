#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NoirSave.generated.h"

/** A suspect's side of the interviews so far. */
USTRUCT()
struct FNoirSuspectSave
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() float Stress = 0.f;
	UPROPERTY() bool bCracked = false;
	UPROPERTY() TArray<int32> AskedTopics;
	UPROPERTY() TArray<FString> ShownClues;
	/** Questions opened up by cracking them (beyond the case's own topics). */
	UPROPERTY() TArray<FString> FollowUpQuestions;
	UPROPERTY() TArray<FString> FollowUpAnswers;
};

/**
 * A case in progress. The case itself is regenerated from its seed (it always comes out the same),
 * so this keeps only what the detective has done: where they are, what they found and learned,
 * which puzzles they worked out, the interviews, and how they arranged the board. One slot.
 */
UCLASS()
class NOIRMYSTERY_API UNoirSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* Slot = TEXT("NoirCase");

	static bool Exists();
	static UNoirSaveGame* Load();
	static void Delete();
	bool Write();

	// The case.
	UPROPERTY() FString Seed;
	UPROPERTY() FString Theme;
	UPROPERTY() FString Difficulty;
	UPROPERTY() bool bSpanish = false;
	UPROPERTY() FString CaseTitle;
	UPROPERTY() FDateTime SavedAt;

	// The detective.
	UPROPERTY() FTransform Where;
	UPROPERTY() FRotator Looking = FRotator::ZeroRotator;
	UPROPERTY() bool bTorchOn = true;
	UPROPERTY() TArray<FString> KnownClues;
	UPROPERTY() TArray<FString> SolvedPuzzles;
	UPROPERTY() TArray<FString> Notes;
	/** Seconds until the late lead arrives; negative once it has (or if the case has none). */
	UPROPERTY() float LateLeadIn = -1.f;

	// The house.
	UPROPERTY() TArray<FString> Searched;

	UPROPERTY() TArray<FNoirSuspectSave> Suspects;

	// The board, as the detective arranged it.
	UPROPERTY() TArray<FString> MovedCards;
	UPROPERTY() TArray<FVector2D> MovedTo;
	UPROPERTY() TArray<FString> StringsA;
	UPROPERTY() TArray<FString> StringsB;
};
