#pragma once

#include "CoreMinimal.h"
#include "NoirClue.generated.h"

class ANoirEvidence;

/**
 * A small hands-on puzzle standing between the detective and what a clue really says (the web
 * game's PuzzleView): which hand left the prints, which sole made the cast, what the UV lamp shows,
 * which blood type clumps, the torn letter's order, the cipher's shift. Type is None once solved.
 */
USTRUCT(BlueprintType)
struct FNoirPuzzle
{
	GENERATED_BODY()

	/** "hand", "sole", "uv", "blood", "torn", "cipher"; None: nothing (left) to work out. */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	FName Type;

	/** hand: "left"/"right"; sole: "boots"/"heels"/"oxfords"/"rubber"; blood: "A"/"B"/"AB"/"O". */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	FString Answer;

	/** uv: what the lamp shows ("cigar", "lavender", "ink", "cat", "mint", "planted"). */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	FString Key;

	/** cipher: the letter shift. */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	int32 Shift = 0;

	/** torn: the scraps, in their true order. */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TArray<FString> Pieces;

	/** What the clue says once worked out (the cipher's plain text, the letter in full). */
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	FText Reveal;

	bool IsOpen() const { return !Type.IsNone(); }
};

/**
 * Something the detective knows and can show a suspect: a physical item they examined, or a
 * page of the case file (a statement, an alibi record, a lab result).
 */
USTRUCT(BlueprintType)
struct FNoirClue
{
	GENERATED_BODY()

	/** The case's evidence id for generated cases; the actor's name for hand-made ones. */
	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	TWeakObjectPtr<ANoirEvidence> Actor;

	/** When it happened (minutes after midnight), for the case board's timeline; INDEX_NONE if timeless. */
	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	int32 Minute = INDEX_NONE;

	/** Minute as the case writes it ("10:42 PM" / "22:42"). */
	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	FText TimeLabel;

	/** Still to be worked out before Text says everything (see FNoirPuzzle). */
	UPROPERTY(BlueprintReadOnly, Category = "Clue")
	FNoirPuzzle Puzzle;
};
