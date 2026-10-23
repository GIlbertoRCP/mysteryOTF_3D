#include "NoirEvidenceModels.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MysteryCore/Blueprint.h"
#include "MysteryCore/Case.h"
#include "MysteryCore/Content.h"
#include "NoirCaseContent.h"

namespace
{
	// Model space: centimetres, the object's front facing +X.

	UStaticMesh* Shape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	struct FBuilder
	{
		UObject* Outer;
		TArray<FNoirEvidencePart> Parts;

		void Add(const TCHAR* Mesh, UMaterialInterface* Material, const FVector& Center, const FVector& SizeCm,
			const FRotator& Rotation = FRotator::ZeroRotator)
		{
			FNoirEvidencePart& Part = Parts.AddDefaulted_GetRef();
			Part.Mesh = Shape(Mesh);
			Part.Material = Material;
			Part.Transform = FTransform(Rotation, Center, SizeCm / 100.f);
		}
		void Box(UMaterialInterface* M, const FVector& C, const FVector& S, const FRotator& R = FRotator::ZeroRotator) { Add(TEXT("Cube"), M, C, S, R); }
		void Cylinder(UMaterialInterface* M, const FVector& C, const FVector& S, const FRotator& R = FRotator::ZeroRotator) { Add(TEXT("Cylinder"), M, C, S, R); }
		/** A cylinder lying along X: a face pointing at the viewer. */
		void Disc(UMaterialInterface* M, float X, float Diameter, float Thickness, float Y = 0.f, float Z = 0.f)
		{
			Add(TEXT("Cylinder"), M, FVector(X, Y, Z), FVector(Diameter, Diameter, Thickness), FRotator(90.f, 0.f, 0.f));
		}
	};

	struct FPalette
	{
		UMaterialInterface* Brass;
		UMaterialInterface* Paper;
		UMaterialInterface* Ink;
		UMaterialInterface* Steel;
		UMaterialInterface* DarkWood;
		UMaterialInterface* Leather;
		UMaterialInterface* Glass;
		UMaterialInterface* Cloth;
		UMaterialInterface* Cord;
		UMaterialInterface* WatchDial;
		UMaterialInterface* Letter;
		UMaterialInterface* Yarn;

		explicit FPalette(UObject* Outer)
		{
			Brass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Brass.MI_Brass"));
			if (!Brass) Brass = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.8f, 0.65f, 0.35f), 0.35f, 1.f);

			Paper = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Letter.MI_Letter"));
			if (!Paper) Paper = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.42f, 0.37f, 0.28f), 0.85f);

			Ink = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.012f, 0.012f, 0.015f), 0.6f);
			Steel = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.2f, 0.2f, 0.21f), 0.25f, 1.f);

			DarkWood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Wood_Dark.MI_Wood_Dark"));
			if (!DarkWood) DarkWood = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.035f, 0.018f, 0.01f), 0.45f);

			Leather = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Leather_Green.MI_Leather_Green"));
			if (!Leather) Leather = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.06f, 0.03f, 0.018f), 0.5f);

			Glass = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.03f, 0.06f, 0.045f), 0.1f);

			Cloth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Suspect_Margaret.MI_Suspect_Margaret"));
			if (!Cloth) Cloth = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.5f, 0.47f, 0.4f), 0.95f);

			Cord = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Yarn.MI_Yarn"));
			if (!Cord) Cord = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.12f, 0.09f, 0.05f), 0.9f);

			WatchDial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_WatchDial.MI_WatchDial"));
			Letter = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Letter.MI_Letter"));
			Yarn = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Yarn.MI_Yarn"));
			if (!WatchDial) WatchDial = Paper;
			if (!Letter) Letter = Paper;
			if (!Yarn) Yarn = Cord;
		}
	};

	FText Tr(mystery::Lang Lang, const TCHAR* En, const TCHAR* Es)
	{
		return FText::FromString(Lang == mystery::Lang::Es ? Es : En);
	}

	FNoirInspectDetail Detail(const FVector& Direction, const FText& Text, float Threshold = 0.85f)
	{
		FNoirInspectDetail D;
		D.LocalDirection = Direction;
		D.Text = Text;
		D.FacingThreshold = Threshold;
		return D;
	}

	/** Clock hand on a +X-facing dial; 0 degrees is twelve o'clock, clockwise as seen from +X. */
	void Hand(FBuilder& B, UMaterialInterface* M, float ClockDeg, float Length, float Width, float X)
	{
		const float T = FMath::DegreesToRadians(ClockDeg);
		const float Y = -FMath::Sin(T), Z = FMath::Cos(T); // a viewer at +X looking toward -X has -Y on their right
		B.Box(M, FVector(X, Y * Length / 2, Z * Length / 2), FVector(0.1f, Width, Length), FRotator(0.f, 0.f, -ClockDeg));
	}

	FLinearColor FromHex(const std::string& Hex)
	{
		return FLinearColor(FColor::FromHex(NoirStr(Hex)));
	}
}

UMaterialInstanceDynamic* NoirEvidenceModels::MakeMaterial(UObject* Outer, const FLinearColor& Color, float Roughness,
	float Metallic, const FLinearColor& Emissive)
{
	static UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Master.M_Master"));
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Master, Outer);
	Mid->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Mid->SetVectorParameterValue(TEXT("Emissive"), Emissive);
	return Mid;
}

FNoirEvidenceSpec NoirEvidenceModels::Build(UObject* Outer, const mystery::Content& Content, const mystery::Blueprint& Bp,
	const mystery::PublicEvidence& E, mystery::Lang Lang)
{
	FNoirEvidenceSpec Spec;
	Spec.DisplayName = NoirText(E.title);
	Spec.Description = NoirText(E.summary);
	const FText Details = NoirText(E.details);
	const FPalette P(Outer);
	FBuilder B{Outer};
	const FVector Front(1, 0, 0), Back(-1, 0, 0);

	if (E.kind == "autopsy" && E.model)
	{
		// The victim's watch, broken in the fall: the hands give the time of death.
		const int32 Min = E.model->min;
		B.Disc(P.Brass, 0, 5.f, 1.2f);
		B.Disc(P.WatchDial, 0.62f, 4.4f, 0.2f);
		Hand(B, P.Ink, ((Min / 60) % 12 + (Min % 60) / 60.f) * 30.f, 1.2f, 0.2f, 0.75f);
		Hand(B, P.Ink, (Min % 60) * 6.f, 1.8f, 0.12f, 0.78f);
		B.Disc(P.Brass, 0.8f, 0.25f, 0.25f);
		B.Cylinder(P.Brass, FVector(0, 0, 2.8f), FVector(0.7f));
		B.Disc(P.Brass, 0, 1.6f, 0.25f, 0, 3.5f);
		B.Disc(P.DarkWood, -0.62f, 3.2f, 0.1f);
		Spec.DisplayName = Tr(Lang, TEXT("the victim's watch"), TEXT("el reloj de la víctima"));
		Spec.Description = Tr(Lang, TEXT("The victim's pocket watch, its glass cracked in the fall."),
			TEXT("El reloj de bolsillo de la víctima, con el cristal roto por la caída."));
		Spec.Details.Add(Detail(Front, FText::Format(Tr(Lang, TEXT("The hands have stopped at {0}."), TEXT("Las agujas se pararon a las {0}.")),
			NoirText(mystery::FormatTime(Min, Lang)))));
		Spec.Details.Add(Detail(Back, FText::Format(Tr(Lang, TEXT("Engraved on the case: {0}"), TEXT("Grabado en la tapa: {0}")),
			NoirText(E.model->engraving))));
	}
	else if (E.kind == "document")
	{
		// A sealed letter; the seal is the colour of the person it is about.
		UMaterialInterface* Wax = MakeMaterial(Outer, E.model ? FromHex(E.model->seal) * 0.35f : FLinearColor(0.25f, 0.01f, 0.01f), 0.35f);
		B.Box(P.Letter, FVector::ZeroVector, FVector(0.5f, 16, 10));
		const float Lines[][2] = {{7, 1.2f}, {8, 0}, {5, -1.2f}};
		for (const auto& L : Lines)
		{
			B.Box(P.Ink, FVector(0.28f, 1.5f, L[1]), FVector(0.1f, L[0], 0.35f));
		}
		B.Disc(Wax, -0.45f, 2.6f, 0.5f);
		Spec.Details.Add(Detail(Front, Details));
		Spec.Details.Add(Detail(Back, Tr(Lang, TEXT("The wax seal on the back is the colour of someone's signet."),
			TEXT("El lacre del reverso tiene el color del sello de alguien."))));
	}
	else if (E.kind == "diary")
	{
		B.Box(P.Leather, FVector::ZeroVector, FVector(2.5f, 14, 20));
		B.Box(P.Paper, FVector(-0.2f, 0.4f, 0), FVector(2.2f, 13.6f, 19.2f));
		B.Box(P.Brass, FVector(1.3f, -5.5f, 0), FVector(0.2f, 1.2f, 3));
		Spec.Details.Add(Detail(Front, Details));
	}
	else if (E.kind == "planted" && E.model)
	{
		const FString Item = NoirStr(E.model->item);
		if (Item == TEXT("handkerchief"))
		{
			B.Box(P.Cloth, FVector::ZeroVector, FVector(0.3f, 22, 22));
			B.Box(P.Ink, FVector(0.2f, -7, -7), FVector(0.1f, 4, 2.5f));
		}
		else if (Item == TEXT("glove"))
		{
			B.Box(P.Leather, FVector::ZeroVector, FVector(2.5f, 9, 11));
			for (int32 i = 0; i < 4; ++i)
			{
				B.Box(P.Leather, FVector(0, -3.3f + i * 2.2f, 8.5f), FVector(2.2f, 1.8f, 6.5f));
			}
			B.Box(P.Leather, FVector(0, 5.5f, 2), FVector(2.2f, 1.8f, 5.5f), FRotator(0, 0, -35));
			B.Box(P.Ink, FVector(1.3f, 0, -2), FVector(0.1f, 3, 1.5f));
		}
		else if (Item == TEXT("cufflink"))
		{
			B.Disc(P.Brass, 0, 1.8f, 0.3f);
			B.Box(P.Ink, FVector(0.17f, 0, 0), FVector(0.06f, 0.9f, 0.5f));
			B.Cylinder(P.Brass, FVector(-0.6f, 0, 0), FVector(0.25f, 0.25f, 0.9f), FRotator(90, 0, 0));
			B.Box(P.Steel, FVector(-1.1f, 0, 0), FVector(0.25f, 1.4f, 0.3f));
		}
		else // cigarette case
		{
			B.Box(P.Brass, FVector::ZeroVector, FVector(1.4f, 8.5f, 6));
			B.Box(P.Ink, FVector(0.75f, 0, 0), FVector(0.1f, 3, 1.5f));
		}
		Spec.Details.Add(Detail(Front, FText::Format(Tr(Lang, TEXT("A monogram: {0}"), TEXT("Un monograma: {0}")), NoirText(E.model->initials))));
		Spec.Details.Add(Detail(Back, Details));
	}
	else if (E.kind == "weapon")
	{
		// Shaped by how it kills; which one it is lives in the text.
		const mystery::Theme& Th = Content.themes.at(Bp.theme);
		int32 WeaponIdx = INDEX_NONE;
		for (const mystery::BlueprintEvidence& Be : Bp.evidence)
		{
			if (Be.id == E.id) WeaponIdx = Be.ref ? *Be.ref : INDEX_NONE;
		}
		const FString Cause = WeaponIdx >= 0 && WeaponIdx < static_cast<int32>(Th.weapons.size()) ? NoirStr(Th.weapons[WeaponIdx].cause) : TEXT("blunt");
		// Where a stain would show on each shape.
		FVector StainAt(0.25f, 0.3f, 9.f), StainSize(0.1f, 1.5f, 5.f);
		if (Cause == TEXT("sharp"))
		{
			B.Box(P.Steel, FVector(0, 0, 6), FVector(0.4f, 2.2f, 16));
			B.Box(P.Brass, FVector(0, 0, -2.5f), FVector(1.2f, 5, 1));
			B.Cylinder(P.DarkWood, FVector(0, 0, -7.5f), FVector(2, 2, 9));
		}
		else if (Cause == TEXT("poison"))
		{
			B.Cylinder(P.Glass, FVector::ZeroVector, FVector(6, 6, 12));
			B.Cylinder(P.Glass, FVector(0, 0, 8), FVector(2.4f, 2.4f, 5));
			B.Cylinder(P.DarkWood, FVector(0, 0, 11.5f), FVector(2.2f, 2.2f, 2));
			B.Box(P.Paper, FVector(3.05f, 0, 0), FVector(0.1f, 4, 5));
			StainAt = FVector(-3.05f, 0, -3); StainSize = FVector(0.1f, 2.5f, 3);
		}
		else if (Cause == TEXT("strangle"))
		{
			for (int32 i = 0; i < 3; ++i)
			{
				B.Cylinder(P.Cord, FVector(0, 0, -1.f + i * 1.f), FVector(14 - i * 2.f, 14 - i * 2.f, 0.9f));
			}
			B.Cylinder(P.Cord, FVector(0, 9, -1), FVector(1, 1, 10), FRotator(0, 0, 60));
			StainAt = FVector(6.6f, 0, -1); StainSize = FVector(0.3f, 2.5f, 1.2f);
		}
		else // blunt
		{
			B.Cylinder(P.Brass, FVector(0, 0, -11), FVector(9, 9, 2));
			B.Cylinder(P.Brass, FVector(0, 0, 0), FVector(3, 3, 22));
			B.Cylinder(P.Brass, FVector(0, 0, 11.5f), FVector(5, 5, 2));
			StainAt = FVector(4.55f, 0, -10.5f); StainSize = FVector(0.1f, 3.5f, 1.5f);
		}
		const bool bResidue = E.weapon && E.weapon->residue;
		// Real traces are old and dark; the tampered decoy's are too red, too fresh, and too neat.
		const bool bTampered = Bp.tampered && Bp.tampered->weaponIdx == WeaponIdx;
		if (bTampered)
		{
			UMaterialInterface* Fake = NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.2f, 0.004f, 0.006f), 0.1f);
			B.Box(Fake, StainAt, StainSize * FVector(1, 0.8f, 0.6f));
			// A neat run down from it, as if poured.
			B.Box(Fake, StainAt - FVector(0, StainSize.Y * 0.15f, StainSize.Z * 0.55f), FVector(StainSize.X, StainSize.Y * 0.18f, StainSize.Z * 0.6f));
		}
		else if (bResidue)
		{
			B.Box(NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.045f, 0.008f, 0.005f), 0.55f), StainAt, StainSize);
		}
		Spec.Details.Add(Detail(Front, Details));
		Spec.Details.Add(Detail(Back, bTampered
			? Tr(Lang, TEXT("Under the lamp: the red is too bright and too neat, run on, not spattered."), TEXT("Bajo la lámpara: el rojo es demasiado vivo y limpio, vertido, no salpicado."))
			: bResidue
			? Tr(Lang, TEXT("Under the lamp: dark traces in the grooves, wiped in a hurry."), TEXT("Bajo la lámpara: restos oscuros en las ranuras, limpiados con prisa."))
			: Tr(Lang, TEXT("Under the lamp: clean. Nothing on it."), TEXT("Bajo la lámpara: limpio. No tiene nada."))));
	}

	// Lab items, laid out on the Mind Place's table to be worked on.
	if (B.Parts.IsEmpty() && E.kind == "smudge")
	{
		// A swatch of lapel cloth with grey smudges on it.
		B.Box(P.Leather, FVector::ZeroVector, FVector(0.4f, 16, 20));
		for (int32 i = 0; i < 4; ++i)
		{
			B.Cylinder(P.Steel, FVector(0.25f, 4.f, -5.f + i * 3.5f), FVector(2.2f, 2.2f, 0.1f), FRotator(0, 0, 90));
		}
		Spec.Details.Add(Detail(Front, Details));
	}
	else if (B.Parts.IsEmpty() && E.kind == "shoeprint")
	{
		// A plaster cast of the print.
		B.Box(P.Cloth, FVector::ZeroVector, FVector(3.f, 13, 28));
		B.Box(P.Paper, FVector(1.55f, 0, 2), FVector(0.1f, 8, 18));
		Spec.Details.Add(Detail(Front, Details));
	}
	else if (B.Parts.IsEmpty() && E.kind == "trace")
	{
		// A folded collar in an evidence bag.
		B.Box(P.Cloth, FVector::ZeroVector, FVector(1.2f, 22, 7));
		B.Box(P.Glass, FVector::ZeroVector, FVector(1.6f, 26, 12));
		Spec.Details.Add(Detail(Front, Details));
	}
	else if (B.Parts.IsEmpty() && E.kind == "scrapings")
	{
		// A glass slide and a stoppered vial.
		B.Box(P.Glass, FVector::ZeroVector, FVector(0.3f, 7.5f, 2.5f));
		B.Box(NoirEvidenceModels::MakeMaterial(Outer, FLinearColor(0.2f, 0.01f, 0.01f), 0.3f), FVector(0.2f, 0, 0), FVector(0.1f, 2, 1.2f));
		B.Cylinder(P.Glass, FVector(0, 0, -6), FVector(1.6f, 1.6f, 6));
		B.Cylinder(P.Yarn, FVector(0, 0, -2.5f), FVector(1.4f, 1.4f, 1.2f));
		Spec.Details.Add(Detail(Front, Details));
	}

	if (B.Parts.IsEmpty())
	{
		// Anything else that ends up physical: a page of notes.
		B.Box(P.Paper, FVector::ZeroVector, FVector(0.3f, 15, 21));
		Spec.Details.Add(Detail(Front, Details));
	}
	Spec.Parts = MoveTemp(B.Parts);
	return Spec;
}
