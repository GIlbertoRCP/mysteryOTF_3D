#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirInteractable.h"
#include "NoirCase.generated.h"

class ANoirSuspect;
class UBoxComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FNoirClue;

/** A card pinned to the case board: a clue, or a suspect's photograph. */
USTRUCT()
struct FNoirBoardCard
{
	GENERATED_BODY()

	UPROPERTY()
	FString Id;
	UPROPERTY()
	FText Title;
	UPROPERTY()
	FText Text;
	UPROPERTY()
	FText TimeLabel;
	int32 Minute = INDEX_NONE;
	bool bPortrait = false;
	/** Moved by the detective: automatic layout leaves it alone. */
	bool bMoved = false;
	/** Centre on the board, in centimetres from the board's middle (X right, Y up). */
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Paper;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pin;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;
};

/** Red string tied between two cards' pins. */
USTRUCT()
struct FNoirBoardString
{
	GENERATED_BODY()

	UPROPERTY()
	FString A;
	UPROPERTY()
	FString B;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Cord;
};

/** One beat of what really happened, shown in the dossier. */
USTRUCT(BlueprintType)
struct FNoirTimelineEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	FText Time;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case", meta = (MultiLine = true))
	FText Event;
};

/** The parts of an accusation, in the order they are asked. Accomplice only when the case has one. */
UENUM(BlueprintType)
enum class ENoirAccusationPart : uint8
{
	Killer,
	Motive,
	Weapon,
	BrokenAlibi,
	Accomplice,
	Count UMETA(Hidden)
};

/**
 * The case: its solution, the options the accusation offers, and the true story. Lives on the
 * case board, which is where the detective makes the accusation.
 *
 * For now the level fills this in by hand; milestone 3's generator will build it from a seed.
 */
UCLASS()
class NOIRMYSTERY_API ANoirCase : public AActor, public INoirInteractable
{
	GENERATED_BODY()

public:
	ANoirCase();

	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	/** The question asked for each part, e.g. "Who killed Harold Thorne?". */
	FText GetQuestion(ENoirAccusationPart Part) const;
	TArray<FText> GetOptions(ENoirAccusationPart Part) const;
	int32 GetSolution(ENoirAccusationPart Part) const;

	/** What plays during the reveal for these picks (the true story only if the killer is right). */
	TArray<FText> GetRevealLines(const TArray<int32>& Picks) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	FText VictimName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	TArray<TObjectPtr<ANoirSuspect>> Suspects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	TArray<FText> Motives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	TArray<FText> Weapons;

	/** Alibi statements; one of them is the lie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case")
	TArray<FText> Alibis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution")
	int32 Killer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution")
	int32 Motive = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution")
	int32 Weapon = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution")
	int32 BrokenAlibi = 0;

	/** Someone lied to cover for the killer and must be named too (Mastermind twist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution")
	bool bHasAccomplice = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Solution", meta = (EditCondition = "bHasAccomplice"))
	int32 Accomplice = 0;

	/** How many questions the accusation asks: four, or five with an accomplice. */
	int32 NumParts() const { return bHasAccomplice ? 5 : 4; }

	/** Played line by line when the right killer is named. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Reveal", meta = (MultiLine = true))
	TArray<FText> RevealLines;

	/** Played after naming the wrong person. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Reveal", meta = (MultiLine = true))
	FText WrongAccusationLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Case|Reveal")
	TArray<FNoirTimelineEntry> Timeline;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds;

	// ─── The physical board: cards, pins and string (the board's front faces local +X) ───

	/** The view while working the board. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> BoardView;

	/** Area cards can be pinned in: width along the board, height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	FVector2D BoardSize = FVector2D(230.f, 100.f);

	/** Stands the board view back far enough to see the whole board. */
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Pins new cards for whatever the detective now knows; cards already up stay where they are. */
	void SyncCards(const TArray<FNoirClue>& Clues);

	/** Where a world-space ray meets the board's face, in board centimetres. */
	bool RayToBoard(const FVector& Origin, const FVector& Direction, FVector2D& OutPosition) const;

	/** The topmost card at a board position, or empty. */
	FString CardAt(const FVector2D& Position) const;

	const FNoirBoardCard* FindCard(const FString& Id) const;

	UFUNCTION(BlueprintCallable, Category = "Board")
	void MoveCard(const FString& Id, FVector2D Position);

	/** How the detective arranged the board (moved cards, tied strings), for a saved case, and back. */
	void GetArrangement(TArray<FString>& Moved, TArray<FVector2D>& To, TArray<FString>& A, TArray<FString>& B) const;
	void Arrange(const TArray<FString>& Moved, const TArray<FVector2D>& To, const TArray<FString>& A, const TArray<FString>& B);

	/** Ties string between two cards, or unties it if they are already tied. */
	UFUNCTION(BlueprintCallable, Category = "Board")
	void ToggleString(const FString& A, const FString& B);

	UFUNCTION(BlueprintCallable, Category = "Board")
	TArray<FString> GetCardIds() const;

	UFUNCTION(BlueprintCallable, Category = "Board")
	int32 NumStrings() const { return Strings.Num(); }

	/** Lifts the hovered card a little and lights the one picked for tying. */
	void SetHighlight(const FString& Hovered, const FString& Picked);

private:
	void UpdateBoardView();
	FNoirBoardCard* FindCardMutable(const FString& Id);
	void AddCard(FNoirBoardCard Card);
	void LayOut();
	void PlaceCard(FNoirBoardCard& Card);
	void PlaceString(FNoirBoardString& String);
	FVector BoardToLocal(const FVector2D& Position, float Lift) const;
	FVector2D CardSize() const;

	UPROPERTY(Transient)
	TArray<FNoirBoardCard> Cards;

	UPROPERTY(Transient)
	TArray<FNoirBoardString> Strings;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TimelineCord;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PinMaterial;

	FString HoveredId;
	FString PickedId;
};
