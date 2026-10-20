#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirClue.h"
#include "NoirInteractable.h"
#include "NoirSuspect.generated.h"

class ANoirCharacter;
class UMaterialInterface;

/** How a suspect looks: what a witness could see (height, coat colour) and who they are. */
struct FNoirLook
{
	bool bFemale = false;
	/** 0.93 short, 1 average, 1.06 tall. */
	float Height = 1.f;
	FLinearColor Coat = FLinearColor(0.02f, 0.02f, 0.02f);
	/** The suspect's signature colour: scarf and hat band. */
	FLinearColor Accent = FLinearColor(0.3f, 0.05f, 0.05f);
	/** Picks skin and hair. */
	int32 Seed = 0;
};
struct FNoirSuspectSave;
class ANoirEvidence;
class UCapsuleComponent;
class UStaticMeshComponent;

/** A question the detective can ask, and the suspect's answer. */
USTRUCT(BlueprintType)
struct FNoirTopic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Question;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Answer;

	/** Follow-up that only opens once the suspect has cracked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bRequiresCracked = false;
};

/** How the suspect responds when shown a particular piece of evidence. */
USTRUCT(BlueprintType)
struct FNoirEvidenceReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<ANoirEvidence> Evidence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Response;

	/** Added to the stress meter (0..1) the first time this evidence is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float Stress = 0.f;
};

/**
 * Someone to question. Stands where their alibi puts them. Presenting the right evidence
 * raises their stress; at 1 they crack, give their confession line, and open follow-ups.
 */
UCLASS()
class NOIRMYSTERY_API ANoirSuspect : public AActor, public INoirInteractable
{
	GENERATED_BODY()

public:
	ANoirSuspect();

	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	/** Indices into Topics that can be asked right now (locked follow-ups are left out). */
	TArray<int32> GetAvailableTopics() const;
	bool WasAsked(int32 TopicIndex) const { return AskedTopics.Contains(TopicIndex); }

	/** Asks a topic; returns the line the suspect says. */
	FText Ask(int32 TopicIndex, ANoirCharacter* Detective);

	/** Shows the suspect a clue; returns the line they say. May crack them. */
	FText Present(const FNoirClue& Clue, ANoirCharacter* Detective);

	/** Hands this suspect's reactions over to a generated case (see ANoirCaseDirector). */
	void BindToCase(class ANoirCaseDirector* InDirector, const FString& InCaseSuspectId);

	/** The interviews so far, for a saved case, and back. */
	FNoirSuspectSave SaveState() const;
	void LoadState(const FNoirSuspectSave& Saved);
	const FString& GetCaseSuspectId() const { return CaseSuspectId; }

	float GetStress() const { return Stress; }
	bool IsCracked() const { return bCracked; }
	FVector GetFaceLocation() const;

	/** Builds the standing figure from a look (the case director calls it once on spawn). */
	void Dress(const FNoirLook& Look);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspect")
	FText DisplayName;

	/** Their colour on the case board's photo card (and their clothes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspect")
	FLinearColor SignatureColor = FLinearColor(0.2f, 0.2f, 0.2f);

	/** "the widow", "the butler" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspect")
	FText Occupation;

	/** Said when the interview starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Greeting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FNoirTopic> Topics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FNoirEvidenceReaction> Reactions;

	/** Said when shown evidence with no reaction of its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText DefaultReaction;

	/** Said once, when stress reaches 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText CrackedLine;

	/** Recorded in the detective's notes when the suspect cracks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText CrackedClue;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	/** Placeholder figure; its material carries the suspect's signature colour. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	/** The dressed figure, standing on the floor; turns to face the detective in an interview. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Figure;

	/** Head, hat and hair: turns toward the detective when they come close. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> HeadPivot;

private:
	float Stress = 0.f;
	bool bCracked = false;
	TSet<int32> AskedTopics;
	TSet<FString> ShownClues;
	/** Topics opened up by cracking them, by index into Topics. */
	TArray<int32> FollowUpTopics;

	UStaticMeshComponent* Part(USceneComponent* Parent, const TCHAR* Shape, const FVector& At, const FVector& SizeCm,
		UMaterialInterface* Material, const FRotator& Rotation = FRotator::ZeroRotator);
	bool bDressed = false;
	float Time = 0.f;
	float Phase = 0.f;
	FVector FigureScale = FVector::OneVector;

	/** Set for suspects of a generated case: presenting goes through the case's confront rules. */
	TWeakObjectPtr<class ANoirCaseDirector> Director;
	FString CaseSuspectId;
};
