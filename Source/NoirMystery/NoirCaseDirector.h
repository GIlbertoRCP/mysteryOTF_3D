#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirClue.h"
#include "NoirCaseDirector.generated.h"

class ANoirCase;
class ANoirComplication;
class ANoirCharacter;
class ANoirRoom;
class ANoirSuspect;

UENUM(BlueprintType)
enum class ENoirCaseLanguage : uint8
{
	English,
	Spanish,
};

/** The case as the detective's notebook tells it: the facts the case file opens with. */
struct FNoirCaseSummary
{
	FText Title;
	FText Synopsis;
	FText VictimName;
	int32 VictimAge = 0;
	FText VictimOccupation;
	/** Where the body was found, and when the death happened ("between 10:00 and 10:40 PM"). */
	FText Scene;
	FText TimeWindow;
	/** What makes tonight harder (a blackout, a storm), if anything. */
	FText ComplicationLabel;
	FText ComplicationNote;
	/** The house or the train, by name. */
	FText Place;
	int32 SuspectCount = 0;
	bool bAccomplice = false;
	/** The hints taken so far, in order. */
	TArray<FText> HintsTaken;
};

/** One of the accusation's questions and how much of its evidence has turned up (never which way
 *  it points: the board keeps count, the detective still has to think). */
struct FNoirQuestionProgress
{
	FText Question;
	int32 Found = 0;
	int32 Total = 0;
	bool IsComplete() const { return Total > 0 && Found >= Total; }
};

/** Where the investigation stands: for the Mind Place's board and its tally. */
struct FNoirCaseProgress
{
	TArray<FNoirQuestionProgress> Questions;
	int32 CluesFound = 0, CluesTotal = 0;
	int32 Searched = 0, SearchTotal = 0;
	int32 Questioned = 0, Cracked = 0, SuspectTotal = 0;
	int32 HintsUsed = 0;
	/** A suspect actor's name -> what the board stamps on their photograph. */
	TMap<FString, FText> Stamps;
};

/** What a suspect says when shown a clue, from the generated case's confront rules. */
struct FNoirConfrontResult
{
	bool bCracked = false;
	FText Line;
	FText FollowUpQuestion;
	FText FollowUpAnswer;
};

/**
 * Runs a generated case in a hand-built level. On play it builds the case from a seed
 * (MysteryCore, identical to the web game's), stages it on the level's rooms, furniture and
 * stand points, then spawns the suspects, the body and the physical evidence, fills in the
 * searchables and the case board, and hands the detective the case file.
 *
 * The seed comes from, in order: the map URL option ?Seed=..., the -CaseSeed= command line,
 * the Seed property, or a random one. ?Lang=es plays the case in Spanish.
 */
UCLASS()
class NOIRMYSTERY_API ANoirCaseDirector : public AActor
{
	GENERATED_BODY()

public:
	ANoirCaseDirector();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FString Seed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FString Theme = TEXT("gothic");

	/** Novice or Detective; Mastermind's accomplice is not part of the accusation yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	FString Difficulty = TEXT("Detective");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	ENoirCaseLanguage Language = ENoirCaseLanguage::English;

	/** The case board the accusation is made at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	TObjectPtr<ANoirCase> Board;

	/** When the held-back clue (if the case has one) arrives as a new lead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Case")
	float LateLeadSeconds = 150.f;

	/** The seed actually played (after URL and command-line overrides). */
	UFUNCTION(BlueprintCallable, Category = "Case")
	FString GetPlayedSeed() const { return PlayedSeed; }

	/** The case's title, once built. */
	FText GetCaseTitle() const { return Briefing; }

	/** One line per placement decision, for debugging and playtests. */
	UFUNCTION(BlueprintCallable, Category = "Case")
	FString DescribeStaging() const;

	FNoirConfrontResult Confront(const FString& SuspectId, const FString& EvidenceId) const;

	/** Evaluates whether presenting EvidenceId contradicts StatementId for SuspectId. */
	UFUNCTION(BlueprintCallable, Category = "Case")
	bool EvaluateContradiction(FName SuspectId, FName StatementId, FName EvidenceId) const;

	/** The case's name for the room containing Location, or empty. */
	FText RoomLabelAt(const FVector& Location) const;

	/** The case's complication made physical (blackout, storm), if it has one. */
	UFUNCTION(BlueprintPure, Category = "Case")
	ANoirComplication* GetComplication() const { return Complication; }

	/** Writes the case in progress to the save slot (pause menu, Mind Place, quitting). */
	UFUNCTION(BlueprintCallable, Category = "Case")
	void SaveCase();

	/** Picking up a saved case (?Continue=1) rather than starting it fresh. */
	UFUNCTION(BlueprintPure, Category = "Case")
	bool IsResuming() const { return bResume; }

	/** The case's next hint (four, each narrowing it further, none naming the killer), filed in the
	 *  detective's notes. Empty once they are all used. */
	UFUNCTION(BlueprintCallable, Category = "Case")
	FText TakeHint();
	UFUNCTION(BlueprintPure, Category = "Case")
	int32 GetHintsUsed() const { return HintsUsed; }
	UFUNCTION(BlueprintPure, Category = "Case")
	int32 GetHintsLeft() const { return Hints.Num() - HintsUsed; }

	const TArray<TObjectPtr<ANoirSuspect>>& GetSuspects() const { return Suspects; }
	const TArray<FNoirClue>& GetCaseFile() const { return CaseFile; }
	const FText& GetBriefing() const { return Briefing; }

	/** The case's opening facts for the notebook; empty until the case is built. */
	FNoirCaseSummary GetCaseSummary() const;

	/** How far the detective has got: evidence found per question, places searched, people
	 *  questioned. Empty until the case is built. */
	FNoirCaseProgress GetCaseProgress(const ANoirCharacter* Detective) const;
	const TArray<FText>& GetHints() const { return Hints; }

	/** Stops the case's clock (the late lead's countdown) while the detective is in the Mind Place. */
	void SetCaseClockPaused(bool bPaused);

	/** A lab item (cast, slide, swatch...) for a case-file clue with a puzzle, to lie on the Mind
	 *  Place's table and be worked on there. Null if the clue has nothing physical to make. */
	class ANoirEvidence* MakeLabItem(const FNoirClue& Clue, const FTransform& Where);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FState;
	TSharedPtr<FState> State;

	void ReadOverrides();
	bool BuildCase();
	void Stage();
	void BriefDetective();
	void DeliverLateLead();
	void ResumeCase();
	/** A clue of this case by id: from the case file, or the physical item in the level. */
	bool FindClue(const FString& Id, FNoirClue& Out) const;
	bool bResume = false;
	bool bBriefed = false;
	bool bLateLeadDelivered = false;

	FString PlayedSeed;
	FTimerHandle BriefTimer;
	FTimerHandle LateLeadTimer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANoirRoom>> Rooms;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANoirSuspect>> Suspects;

	UPROPERTY(Transient)
	TObjectPtr<ANoirComplication> Complication;

	TArray<FNoirClue> CaseFile;
	TOptional<FNoirClue> LateLead;
	FText Briefing;
	TArray<FText> Hints;
	int32 HintsUsed = 0;
};
