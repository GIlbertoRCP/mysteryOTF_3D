#include "NoirHUD.h"

#include "NoirCaseContent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NoirCase.h"
#include "NoirCaseDirector.h"
#include "NoirCharacter.h"
#include "NoirEvidence.h"
#include "NoirInteractable.h"
#include "NoirSuspect.h"

namespace
{
	// Warm paper tones from the palette; never pure white.
	const FLinearColor Cream(0.85f, 0.78f, 0.62f, 0.9f);
	const FLinearColor Amber(0.95f, 0.65f, 0.3f, 1.f);
	const FLinearColor Dim(0.5f, 0.45f, 0.36f, 0.7f);
	const FLinearColor Panel(0.02f, 0.015f, 0.01f, 0.6f);
	const FLinearColor Oxblood(0.45f, 0.06f, 0.05f, 1.f);

	/** Greedy word wrap; also honours explicit newlines. */
	TArray<FString> Wrap(const FString& Text, int32 MaxChars)
	{
		TArray<FString> Lines;
		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		for (const FString& Paragraph : Paragraphs)
		{
			TArray<FString> Words;
			Paragraph.ParseIntoArrayWS(Words);
			FString Line;
			for (const FString& Word : Words)
			{
				if (!Line.IsEmpty() && Line.Len() + 1 + Word.Len() > MaxChars)
				{
					Lines.Add(Line);
					Line.Reset();
				}
				Line += Line.IsEmpty() ? Word : TEXT(" ") + Word;
			}
			Lines.Add(Line);
		}
		return Lines;
	}
}

void ANoirHUD::DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	float Width = 0.f, Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - Width) * 0.5f, Y, Font, Scale);
}

void ANoirHUD::DrawHUD()
{
	Super::DrawHUD();

	const ANoirCharacter* Detective = Cast<ANoirCharacter>(GetOwningPawn());
	if (!Detective || !Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	if (const ANoirEvidence* Evidence = Detective->GetInspected())
	{
		FString Title = Evidence->DisplayName.ToString();
		if (!Title.IsEmpty())
		{
			Title = Title.Left(1).ToUpper() + Title.Mid(1);
		}
		DrawCentered(Title, Canvas->ClipY * 0.08f, Amber, 1.3f);
		if (Evidence->NumDetails() > 0)
		{
			DrawCentered(FString::Format(*NoirTrS(TEXT("Details noticed  {0} / {1}"), TEXT("Detalles vistos  {0} / {1}")),
				{Evidence->NumDetailsFound(), Evidence->NumDetails()}),
				Canvas->ClipY * 0.08f + 36.f, Cream);
		}
		if (Detective->GetInspectedPuzzle())
		{
			DrawCentered(NoirTrS(TEXT("[R]  work on it"), TEXT("[R]  examinarlo a fondo")), Canvas->ClipY * 0.92f - 32.f, Amber, 1.f);
		}
		DrawCentered(NoirTrS(TEXT("Move the mouse to turn it over    Wheel  closer    [E]  put it down"),
			TEXT("Mueve el ratón para girarlo    Rueda  acercar    [E]  dejarlo")), Canvas->ClipY * 0.92f, Cream, 0.9f);
	}
	else if (Detective->IsInterviewing())
	{
		DrawInterview(*Detective);
	}
	else if (Detective->IsOnBoard())
	{
		DrawBoard(*Detective);
	}
	else if (Detective->IsAccusing())
	{
		DrawAccusation(*Detective);
	}
	else
	{
		DrawRect(Cream, CX - 2.f, CY - 2.f, 4.f, 4.f);
	}

	const bool bBusy = Detective->IsInspecting() || Detective->IsInterviewing() || Detective->IsAccusing() || Detective->IsOnBoard();
	if (AActor* Focused = bBusy ? nullptr : Detective->GetFocusedActor())
	{
		const FText Prompt = INoirInteractable::Execute_GetInteractPrompt(Focused);
		if (!Prompt.IsEmpty())
		{
			DrawCentered(FString::Printf(TEXT("[E]  %s"), *Prompt.ToString()), CY + 28.f, Cream);
		}
	}

	if (Detective->GetMessageAge() < MessageSeconds && !Detective->GetMessage().IsEmpty())
	{
		DrawCentered(Detective->GetMessage().ToString(), Canvas->ClipY * 0.78f, Amber, 1.1f);
	}

	// Where am I: the Mind Place, or the room's name in the generated case.
	if (!bBusy && Detective->IsInMindPlace())
	{
		DrawText(NoirTrS(TEXT("Mind Place"), TEXT("Palacio mental")), Amber, 40.f, Canvas->ClipY - 60.f, GEngine->GetMediumFont(), 1.f);
		DrawText(NoirTrS(TEXT("Tab  back to the case    N  notes"), TEXT("Tab  volver al caso    N  notas")),
			Dim, 40.f, Canvas->ClipY - 34.f, GEngine->GetMediumFont(), 0.8f);
	}
	else if (!bBusy)
	{
		if (!Director.IsValid())
		{
			Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(this, ANoirCaseDirector::StaticClass()));
		}
		if (const ANoirCaseDirector* Case = Director.Get())
		{
			const FText Room = Case->RoomLabelAt(Detective->GetActorLocation());
			if (!Room.IsEmpty())
			{
				DrawText(Room.ToString(), Dim, 40.f, Canvas->ClipY - 60.f, GEngine->GetMediumFont(), 0.95f);
			}
		}
	}

	const APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->IsInputKeyDown(EKeys::N))
	{
		UFont* Font = GEngine->GetMediumFont();
		DrawRect(Panel, 30.f, 30.f, FMath::Min(Canvas->ClipX - 60.f, 1250.f), Canvas->ClipY - 60.f);
		float Y = 50.f;
		DrawText(NoirTrS(TEXT("NOTES"), TEXT("NOTAS")), Amber, 60.f, Y, Font, 1.2f);
		Y += 38.f;
		if (Detective->GetClues().IsEmpty())
		{
			DrawText(NoirTrS(TEXT("Nothing yet."), TEXT("Nada todavía.")), Cream, 60.f, Y, Font);
		}
		for (const FText& Clue : Detective->GetClues())
		{
			const TArray<FString> Lines = Wrap(Clue.ToString(), 120);
			if (Y + 22.f * Lines.Num() > Canvas->ClipY - 60.f)
			{
				DrawText(TEXT("..."), Dim, 60.f, Y, Font, 0.85f);
				break;
			}
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				DrawText((i == 0 ? TEXT("- ") : TEXT("  ")) + Lines[i], Cream, 60.f, Y, Font, 0.85f);
				Y += 22.f;
			}
			Y += 6.f;
		}
	}
}

void ANoirHUD::DrawInterview(const ANoirCharacter& Detective)
{
	const ANoirSuspect* Suspect = Detective.GetInterviewee();
	UFont* Font = GEngine->GetMediumFont();
	const float Left = 60.f;
	float Y = Canvas->ClipY * 0.12f;

	// Who, and how close they are to cracking.
	DrawRect(Panel, Left - 20.f, Y - 16.f, 470.f, 120.f);
	DrawText(Suspect->DisplayName.ToString(), Amber, Left, Y, Font, 1.3f);
	DrawText(Suspect->Occupation.ToString(), Cream, Left, Y + 34.f, Font, 0.9f);
	const float BarW = 300.f;
	DrawText(Suspect->IsCracked() ? NoirTrS(TEXT("Cracked"), TEXT("Quebrado")) : NoirTrS(TEXT("Stress"), TEXT("Tensión")), Cream, Left, Y + 64.f, Font, 0.8f);
	DrawRect(FLinearColor(0.12f, 0.1f, 0.08f, 0.9f), Left + 90.f, Y + 70.f, BarW, 8.f);  // empty track
	DrawRect(Oxblood, Left + 90.f, Y + 70.f, BarW * Suspect->GetStress(), 8.f);

	// The menu.
	Y += 140.f;
	const TArray<FNoirInterviewOption> Options = Detective.GetInterviewOptions();
	DrawText(Detective.IsPresenting() ? NoirTrS(TEXT("Present evidence"), TEXT("Mostrar pruebas")) : NoirTrS(TEXT("Ask"), TEXT("Preguntar")), Amber, Left, Y, Font);
	Y += 32.f;
	for (int32 i = 0; i < Options.Num() && i < 9; ++i)
	{
		DrawText(FString::Printf(TEXT("%d   %s"), i + 1, *Options[i].Label), Options[i].bDim ? Dim : Cream, Left, Y, Font);
		Y += 28.f;
	}
	Y += 10.f;
	DrawText(Detective.IsPresenting() ? NoirTrS(TEXT("0   Back"), TEXT("0   Volver")) : NoirTrS(TEXT("P   Present evidence"), TEXT("P   Mostrar pruebas")), Cream, Left, Y, Font);
	DrawText(NoirTrS(TEXT("E   Leave"), TEXT("E   Salir")), Cream, Left, Y + 28.f, Font);

	// What they just said, as a subtitle.
	const FString Line = Detective.GetDialogueLine().ToString();
	if (!Line.IsEmpty())
	{
		const TArray<FString> Lines = Wrap(Line, 90);
		float SubY = Canvas->ClipY * 0.80f - 30.f * (Lines.Num() - 1);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			DrawCentered(i == 0 ? Suspect->DisplayName.ToString() + TEXT(":  ") + Lines[i] : Lines[i], SubY, Cream, 1.05f);
			SubY += 30.f;
		}
	}
}

void ANoirHUD::DrawAccusation(const ANoirCharacter& Detective)
{
	const ANoirCase* Case = Detective.GetCase();
	if (!Case)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// Letterbox bars ease in for the reveal and stay for the dossier.
	if (const float Bars = Detective.GetLetterbox(); Bars > 0.f)
	{
		const float BarH = H * 0.12f * Bars;
		DrawRect(FLinearColor::Black, 0.f, 0.f, W, BarH);
		DrawRect(FLinearColor::Black, 0.f, H - BarH, W, BarH);
	}

	switch (Detective.GetAccusePhase())
	{
	case ENoirAccusePhase::Choosing:
	{
		const int32 Step = Detective.GetAccuseStep();
		const int32 Parts = Case->NumParts();
		const float PanelW = 760.f;
		const float Left = (W - PanelW) * 0.5f;
		float Y = H * 0.2f;
		DrawRect(Panel, Left - 30.f, Y - 30.f, PanelW + 60.f, 560.f);
		DrawText(NoirTrS(TEXT("ACCUSATION"), TEXT("ACUSACIÓN")), Amber, Left, Y, Font, 1.4f);
		Y += 50.f;

		// What has been decided so far.
		for (int32 Part = 0; Part < Step && Part < Parts; ++Part)
		{
			const ENoirAccusationPart P = static_cast<ENoirAccusationPart>(Part);
			const TArray<FText> Options = Case->GetOptions(P);
			const int32 Pick = Detective.GetAccusePicks()[Part];
			DrawText(Case->GetQuestion(P).ToString() + TEXT("  ") + (Options.IsValidIndex(Pick) ? Options[Pick].ToString() : TEXT("")),
				Dim, Left, Y, Font, 0.9f);
			Y += 26.f;
		}
		Y += 14.f;

		if (Step < Parts)
		{
			const ENoirAccusationPart P = static_cast<ENoirAccusationPart>(Step);
			DrawText(Case->GetQuestion(P).ToString(), Amber, Left, Y, Font, 1.15f);
			Y += 40.f;
			const TArray<FText> Options = Case->GetOptions(P);
			for (int32 i = 0; i < Options.Num() && i < 9; ++i)
			{
				DrawText(FString::Printf(TEXT("%d   %s"), i + 1, *Options[i].ToString()), Cream, Left, Y, Font);
				Y += 30.f;
			}
		}
		else
		{
			DrawText(NoirTrS(TEXT("Is this your accusation?"), TEXT("¿Es esta tu acusación?")), Amber, Left, Y, Font, 1.15f);
			Y += 40.f;
			DrawText(NoirTrS(TEXT("1   Make the accusation"), TEXT("1   Acusar")), Cream, Left, Y, Font);
			DrawText(NoirTrS(TEXT("2   Start over"), TEXT("2   Empezar de nuevo")), Cream, Left, Y + 30.f, Font);
		}
		DrawText(Step > 0 ? NoirTrS(TEXT("0  Back      E  Leave the board"), TEXT("0  Volver      E  Dejar el tablero"))
		                 : NoirTrS(TEXT("E  Leave the board"), TEXT("E  Dejar el tablero")), Dim, Left, H * 0.2f + 500.f, Font, 0.9f);
		break;
	}
	case ENoirAccusePhase::Reveal:
	{
		const TArray<FString> Lines = Wrap(Detective.GetCurrentRevealLine().ToString(), 80);
		float Y = H * 0.74f - 32.f * (Lines.Num() - 1);
		if (!Lines.IsEmpty() && !Lines[0].IsEmpty())
		{
			// Soft dark band so the line reads over bright practicals behind the accused.
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, Y - 16.f, W, 32.f * Lines.Num() + 24.f);
		}
		for (const FString& Line : Lines)
		{
			DrawCentered(Line, Y, Cream, 1.2f);
			Y += 32.f;
		}
		DrawText(NoirTrS(TEXT("E  skip"), TEXT("E  saltar")), Dim, W - 140.f, H - 50.f, Font, 0.8f);
		break;
	}
	case ENoirAccusePhase::Dossier:
		DrawDossier(Detective);
		break;
	default:
		break;
	}
}

void ANoirHUD::DrawDossier(const ANoirCharacter& Detective)
{
	const ANoirCase* Case = Detective.GetCase();
	UFont* Font = GEngine->GetMediumFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor(0.015f, 0.012f, 0.01f, 0.92f), 0.f, 0.f, W, H);

	const float Left = W * 0.12f;
	float Y = H * 0.1f;
	DrawText(NoirTrS(TEXT("DOSSIER"), TEXT("EXPEDIENTE")), Amber, Left, Y, Font, 1.5f);
	DrawText(Case->Title.ToString(), Cream, Left, Y + 44.f, Font, 1.1f);
	Y += 110.f;

	// The verdict, part by part.
	int32 Correct = 0;
	const int32 Parts = Case->NumParts();
	for (int32 Part = 0; Part < Parts; ++Part)
	{
		const ENoirAccusationPart P = static_cast<ENoirAccusationPart>(Part);
		const TArray<FText> Options = Case->GetOptions(P);
		const int32 Pick = Detective.GetAccusePicks()[Part];
		const int32 Truth = Case->GetSolution(P);
		const bool bRight = Pick == Truth;
		Correct += bRight;
		const FString Said = Options.IsValidIndex(Pick) ? Options[Pick].ToString() : TEXT("-");
		const FString Real = Options.IsValidIndex(Truth) ? Options[Truth].ToString() : TEXT("-");
		DrawText(Case->GetQuestion(P).ToString(), Dim, Left, Y, Font, 0.9f);
		const FString Verdict = bRight
			? FString::Format(*NoirTrS(TEXT("RIGHT   {0}"), TEXT("BIEN   {0}")), {Said})
			: FString::Format(*NoirTrS(TEXT("WRONG   you said \"{0}\"  -  it was \"{1}\""), TEXT("MAL   dijiste «{0}»  -  era «{1}»")), {Said, Real});
		DrawText(Verdict, bRight ? Cream : Amber, Left + 260.f, Y, Font);
		Y += 32.f;
	}
	Y += 10.f;
	DrawText(FString::Format(*NoirTrS(TEXT("{0} of {1}"), TEXT("{0} de {1}")), {Correct, Parts}), Amber, Left + 260.f, Y, Font, 1.1f);
	Y += 60.f;

	// What really happened.
	DrawText(NoirTrS(TEXT("WHAT REALLY HAPPENED"), TEXT("LO QUE PASÓ DE VERDAD")), Amber, Left, Y, Font, 1.1f);
	Y += 40.f;
	for (const FNoirTimelineEntry& Entry : Case->Timeline)
	{
		DrawText(Entry.Time.ToString(), Amber, Left, Y, Font);
		for (const FString& Line : Wrap(Entry.Event.ToString(), 90))
		{
			DrawText(Line, Cream, Left + 120.f, Y, Font);
			Y += 28.f;
		}
		Y += 6.f;
	}
	DrawText(NoirTrS(TEXT("E  Close the case"), TEXT("E  Cerrar el caso")), Dim, Left, H - 70.f, Font, 0.9f);
}

void ANoirHUD::DrawBoard(const ANoirCharacter& Detective)
{
	const ANoirCase* Board = Detective.GetBoard();
	UFont* Font = GEngine->GetMediumFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// The hovered card, written out in full.
	if (const FNoirBoardCard* Card = Board ? Board->FindCard(Detective.GetHoveredCard()) : nullptr)
	{
		const TArray<FString> Lines = Wrap(Card->Text.ToString(), 70);
		const float PanelH = 56.f + 24.f * Lines.Num();
		const float Top = H - 110.f - PanelH;
		DrawRect(Panel, 30.f, Top, 760.f, PanelH);
		FString Heading = Card->Title.ToString();
		if (!Card->TimeLabel.IsEmpty())
		{
			Heading = Card->TimeLabel.ToString() + TEXT("   ") + Heading;
		}
		DrawText(Heading, Amber, 50.f, Top + 14.f, Font, 1.05f);
		float Y = Top + 46.f;
		for (const FString& Line : Lines)
		{
			DrawText(Line, Cream, 50.f, Y, Font, 0.9f);
			Y += 24.f;
		}
	}

	if (const FNoirBoardCard* Picked = Board ? Board->FindCard(Detective.GetPickedCard()) : nullptr)
	{
		DrawCentered(FString::Format(*NoirTrS(TEXT("Tying string from \"{0}\": right-click another card"),
			TEXT("Atando hilo desde «{0}»: clic derecho en otra tarjeta")), {Picked->Title.ToString()}), H * 0.08f, Amber);
	}

	DrawCentered(NoirTrS(
		TEXT("Drag cards to move them    Right-click two cards to tie them    A  make an accusation    E  step back"),
		TEXT("Arrastra las tarjetas    Clic derecho en dos tarjetas para unirlas    A  acusar    E  apartarse")),
		H - 60.f, Cream, 0.9f);
}
