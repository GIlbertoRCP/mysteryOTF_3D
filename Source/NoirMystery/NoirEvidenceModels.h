#pragma once

#include "CoreMinimal.h"
#include "NoirEvidence.h"

namespace mystery
{
	class Content;
	struct Blueprint;
	struct PublicEvidence;
	enum class Lang;
}

/** Everything a spawned ANoirEvidence needs for one clue of a generated case. */
struct FNoirEvidenceSpec
{
	FText DisplayName;
	FText Description;
	TArray<FNoirEvidencePart> Parts;
	TArray<FNoirInspectDetail> Details;
};

/**
 * Placeholder 3D models for generated evidence, built from engine shapes: the victim's watch
 * (hands at the time of death), sealed letters in the suspect's colour, the diary, planted items
 * and weapons shaped by how they kill. Real meshes replace these later.
 */
namespace NoirEvidenceModels
{
	/** Materials are dynamic instances of M_Master owned by Outer. */
	FNoirEvidenceSpec Build(UObject* Outer, const mystery::Content& Content, const mystery::Blueprint& Bp,
		const mystery::PublicEvidence& Evidence, mystery::Lang Lang);

	/** A tinted dynamic instance of M_Master. */
	UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, const FLinearColor& Color, float Roughness = 0.6f,
		float Metallic = 0.f, const FLinearColor& Emissive = FLinearColor::Black);
}
