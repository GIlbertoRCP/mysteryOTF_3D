#include "NoirSearchable.h"

#include "NoirCaseContent.h"
#include "Components/BoxComponent.h"
#include "NoirCharacter.h"
#include "NoirEvidence.h"

#define LOCTEXT_NAMESPACE "NoirSearchable"

ANoirSearchable::ANoirSearchable()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(50.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Bounds->SetLineThickness(1.f);
	RootComponent = Bounds;

	DisplayName = NoirTr(TEXT("the furniture"), TEXT("el mueble"));
}

FText ANoirSearchable::GetInteractPrompt_Implementation() const
{
	return bSearched
		? FText::Format(NoirTr(TEXT("Search {0} again (searched)"), TEXT("Registrar {0} otra vez (registrado)")), DisplayName)
		: FText::Format(NoirTr(TEXT("Search {0}"), TEXT("Registrar {0}")), DisplayName);
}

void ANoirSearchable::RestoreSearched()
{
	bSearched = true;
	for (ANoirEvidence* Item : Items)
	{
		if (Item) Item->Reveal();
	}
}

void ANoirSearchable::Interact_Implementation(APawn* InstigatorPawn)
{
	ANoirCharacter* Detective = Cast<ANoirCharacter>(InstigatorPawn);
	if (!Detective)
	{
		return;
	}
	if (bSearched)
	{
		Detective->ShowMessage(FText::Format(NoirTr(TEXT("You have already searched {0}."), TEXT("Ya has registrado {0}.")), DisplayName));
		return;
	}

	bSearched = true;
	Items.RemoveAll([](const TObjectPtr<ANoirEvidence>& Item) { return Item == nullptr; });
	if (!Items.IsEmpty())
	{
		// Everything hidden here comes out; the first piece is picked up straight away.
		for (ANoirEvidence* Item : Items)
		{
			Item->Reveal();
		}
		Detective->BeginInspect(Items[0]);
		if (!ClueText.IsEmpty())
		{
			Detective->AddClue(ClueText);
		}
		if (!SearchText.IsEmpty())
		{
			Detective->ShowMessage(SearchText);
		}
		return;
	}
	if (!SearchText.IsEmpty())
	{
		Detective->ShowMessage(SearchText);
	}
	else if (ClueText.IsEmpty())
	{
		Detective->ShowMessage(FText::Format(NoirTr(TEXT("You search {0}. Nothing of interest."), TEXT("Registras {0}. Nada de interés.")), DisplayName));
	}
	else
	{
		Detective->AddClue(ClueText);
		Detective->ShowMessage(FText::Format(NoirTr(TEXT("Found: {0}"), TEXT("Encontrado: {0}")), ClueText));
	}
}

#undef LOCTEXT_NAMESPACE
