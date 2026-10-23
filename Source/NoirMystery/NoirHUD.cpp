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
#include "NoirPlayerController.h"
#include "NoirEvidence.h"
#include "NoirInteractable.h"
#include "NoirSuspect.h"
#include "NoirStyle.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace NoirHUDLocal
{
	// The shared palette (NoirStyle.h): the HUD matches the menus.
	using NoirStyle::Cream;
	using NoirStyle::Amber;
	using NoirStyle::Dim;
	using NoirStyle::Panel;
	using NoirStyle::Oxblood;

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
using namespace NoirHUDLocal;

FSlateFontInfo ANoirHUD::HudFont(float Scale) const
{
	// Body size at Scale 1, in screen points for this screen. Canvas text needs a font asset (the
	// Slate default style has only the raw family): the engine's runtime Roboto, the same typeface.
	static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	FSlateFontInfo Info = NoirStyle::Font("Regular", NoirStyle::Body * Scale * S);
	if (Roboto)
	{
		Info.FontObject = Roboto;
		Info.TypefaceFontName = TEXT("Regular");
	}
	return Info;
}

float ANoirHUD::TextWidth(const FString& Text, float Scale) const
{
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return Measure->Measure(Text, HudFont(Scale)).X / S;
}

void ANoirHUD::Txt(const FString& Text, const FLinearColor& Color, float X, float Y, UFont*, float Scale)
{
	if (Text.IsEmpty() || !Canvas) return;
	FCanvasTextItem Item(FVector2D(X * S, Y * S), FText::FromString(Text), HudFont(Scale), Color);
	// A soft drop shadow: text holds up over lamps, fog and pale walls.
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f * Color.A), FVector2D(1.5f * S, 1.5f * S));
	Canvas->DrawItem(Item);
}

void ANoirHUD::Box(const FLinearColor& Color, float X, float Y, float W, float H)
{
	DrawRect(Color, X * S, Y * S, W * S, H * S);
}

float ANoirHUD::KeyCap(const FString& Key, float X, float Y, float Scale)
{
	// A small dark key with an amber legend; returns its width so the label can follow.
	const float W = FMath::Max(TextWidth(Key, Scale * 0.85f) + 14.f, 26.f * Scale);
	const float H = 24.f * Scale;
	Box(NoirStyle::PanelEdge, X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
	Box(NoirStyle::KeyCap, X, Y, W, H);
	Txt(Key, Amber, X + (W - TextWidth(Key, Scale * 0.85f)) * 0.5f, Y + 2.f * Scale, nullptr, Scale * 0.85f);
	return W;
}

void ANoirHUD::Option(const FString& Key, const FString& Label, float X, float Y, const FLinearColor& Color, float Scale)
{
	const float W = KeyCap(Key, X, Y - 1.f, Scale);
	Txt(Label, Color, X + W + 12.f, Y, nullptr, Scale);
}

void ANoirHUD::Backdrop(float CenterX, float Top, float Width, float Height, float Alpha)
{
	// A dark band with soft ends, behind subtitles and messages.
	const float Pad = 28.f;
	const int32 Steps = 6;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float Inset = Pad * i / Steps;
		Box(FLinearColor(0.f, 0.f, 0.f, Alpha / Steps), CenterX - Width * 0.5f - Pad + Inset, Top, Width + 2.f * (Pad - Inset), Height);
	}
}

void ANoirHUD::Hints(const TArray<TPair<FString, FString>>& Keys, float Y, const FLinearColor& Color)
{
	// Measure, centre, then draw: keycap, label, a gap.
	const float Scale = 0.88f, Gap = 30.f;
	float Total = 0.f;
	for (const TPair<FString, FString>& K : Keys)
	{
		Total += FMath::Max(TextWidth(K.Key, Scale * 0.85f) + 14.f, 26.f * Scale) + 10.f + TextWidth(K.Value, Scale) + Gap;
	}
	float X = (VW - (Total - Gap)) * 0.5f;
	Backdrop(VW * 0.5f, Y - 10.f, Total, 44.f, 0.45f);
	for (const TPair<FString, FString>& K : Keys)
	{
		const float W = KeyCap(K.Key, X, Y - 1.f, Scale);
		Txt(K.Value, Color, X + W + 10.f, Y, nullptr, Scale);
		X += W + 10.f + TextWidth(K.Value, Scale) + Gap;
	}
}

void ANoirHUD::DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	Txt(Text, Color, (VW - TextWidth(Text, Scale)) * 0.5f, Y, nullptr, Scale);
}

void ANoirHUD::DrawHUD()
{
	Super::DrawHUD();

	const ANoirCharacter* Detective = Cast<ANoirCharacter>(GetOwningPawn());
	if (!Detective || !Canvas)
	{
		return;
	}
	// Lay out at 1080 lines tall, whatever the screen: sizes stay in proportion at 1440p or 4K.
	S = FMath::Max(Canvas->ClipY / 1080.f, 0.5f);
	VH = 1080.f;
	VW = Canvas->ClipX / S;
	// Menus, the map, the case file and puzzles cover the screen: nothing of the HUD shows through.
	if (const ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetOwningPlayerController()))
	{
		if (PC->IsMenuOpen() || PC->IsMapOpen() || PC->IsCaseNotebookOpen() || PC->IsPuzzleOpen()) return;
	}

	const float CX = VW * 0.5f;
	const float CY = VH * 0.5f;

	if (const ANoirEvidence* Evidence = Detective->GetInspected())
	{
		FString Title = Evidence->DisplayName.ToString();
		if (!Title.IsEmpty())
		{
			Title = Title.Left(1).ToUpper() + Title.Mid(1);
		}
		DrawCentered(Title, VH * 0.08f, Amber, 1.3f);
		if (Evidence->NumDetails() > 0)
		{
			DrawCentered(FString::Format(*NoirTrS(TEXT("Details noticed  {0} / {1}"), TEXT("Detalles vistos  {0} / {1}")),
				{Evidence->NumDetailsFound(), Evidence->NumDetails()}),
				VH * 0.08f + 36.f, Cream);
		}
		if (Detective->GetInspectedPuzzle())
		{
			Hints({{TEXT("R"), NoirTrS(TEXT("work on it"), TEXT("examinarlo a fondo"))}}, VH * 0.92f - 44.f, Amber);
		}
		Hints({{NoirTrS(TEXT("Mouse"), TEXT("Ratón")), NoirTrS(TEXT("turn it over"), TEXT("girarlo"))},
			{NoirTrS(TEXT("Wheel"), TEXT("Rueda")), NoirTrS(TEXT("closer"), TEXT("acercar"))},
			{TEXT("E"), NoirTrS(TEXT("put it down"), TEXT("dejarlo"))}}, VH * 0.92f);
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
		Box(Cream, CX - 2.f, CY - 2.f, 4.f, 4.f);
	}

	const bool bBusy = Detective->IsInspecting() || Detective->IsInterviewing() || Detective->IsAccusing() || Detective->IsOnBoard();
	DrawNewEvidence(*Detective);
	if (!bBusy) DrawBreath(*Detective, CX, CY);
	if (AActor* Focused = bBusy ? nullptr : Detective->GetFocusedActor())
	{
		const FText Prompt = INoirInteractable::Execute_GetInteractPrompt(Focused);
		if (!Prompt.IsEmpty())
		{
			DrawCentered(FString::Printf(TEXT("[E]  %s"), *Prompt.ToString()), CY + 28.f, Cream);
		}
	}

	// Long messages (a hint) wrap, and stay up long enough to read.
	const FString Message = Detective->GetMessage().ToString();
	if (!Message.IsEmpty() && Detective->GetMessageAge() < MessageSeconds + Message.Len() / 30.f)
	{
		const TArray<FString> Lines = Wrap(Message, 90);
		// In an interview the suspect's subtitles own the bottom of the screen: the message sits above them.
		const float LineHeight = 30.f;
		float Y = VH * (Detective->IsInterviewing() ? 0.68f : 0.78f) - (Lines.Num() - 1) * LineHeight;
		float Widest = 0.f;
		for (const FString& Line : Lines) Widest = FMath::Max(Widest, TextWidth(Line, 1.1f));
		Backdrop(VW * 0.5f, Y - 10.f, Widest + 40.f, LineHeight * Lines.Num() + 14.f, 0.5f);
		for (const FString& Line : Lines)
		{
			DrawCentered(Line, Y, Amber, 1.1f);
			Y += LineHeight;
		}
	}

	// Where am I: the Mind Place, or the room's name in the generated case.
	if (!bBusy && Detective->IsInMindPlace())
	{
		DrawMindTally(*Detective);
		Txt(NoirTrS(TEXT("Mind Place"), TEXT("Palacio mental")), Amber, 40.f, VH - 68.f, GEngine->GetMediumFont(), 1.f);
		const float W1 = KeyCap(TEXT("Tab"), 40.f, VH - 36.f, 0.85f);
		Txt(NoirTrS(TEXT("back to the case"), TEXT("volver al caso")), Dim, 40.f + W1 + 10.f, VH - 35.f, nullptr, 0.85f);
		const float X2 = 40.f + W1 + 30.f + TextWidth(NoirTrS(TEXT("back to the case"), TEXT("volver al caso")), 0.85f);
		const float W2 = KeyCap(TEXT("N"), X2, VH - 36.f, 0.85f);
		Txt(NoirTrS(TEXT("case file"), TEXT("expediente")), Dim, X2 + W2 + 10.f, VH - 35.f, nullptr, 0.85f);
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
				Txt(Room.ToString(), Dim, 40.f, VH - 60.f, GEngine->GetMediumFont(), 0.95f);
			}
		}
	}
}

void ANoirHUD::DrawNewEvidence(const ANoirCharacter& Detective)
{
	const TArray<FNoirClue>& Known = Detective.GetKnownClues();
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (KnownCount < 0 || Known.Num() < KnownCount)
	{
		KnownCount = Known.Num(); // a case started or resumed: nothing is new
	}
	else if (Known.Num() > KnownCount)
	{
		// Found together (a body searched, a drawer of letters): one card, "and N more".
		ToastTitle = Known.Last().Title.ToString();
		ToastMore = Known.Num() - KnownCount - 1;
		ToastAt = Now;
		KnownCount = Known.Num();
	}
	const float Age = Now - ToastAt;
	const float Hold = 4.5f;
	if (Age > Hold + 0.6f || ToastTitle.IsEmpty()) return;

	// Slides in from the right, holds, fades.
	const float In = FMath::Clamp(Age / 0.35f, 0.f, 1.f);
	const float Ease = 1.f - FMath::Square(1.f - In);
	const float Alpha = Age < Hold ? 1.f : 1.f - (Age - Hold) / 0.6f;
	UFont* Font = GEngine->GetMediumFont();
	FString Title = ToastTitle.Len() > 52 ? ToastTitle.Left(50) + TEXT("…") : ToastTitle;
	const FString Pinned = NoirTrS(TEXT("Pinned to your Mind Place board  (Tab)"), TEXT("Clavada en el tablero de tu palacio mental  (Tab)"));
	const float W = FMath::Max3(380.f, TextWidth(Title) + 40.f, TextWidth(Pinned, 0.78f) + 40.f);
	const float H = ToastMore > 0 ? 112.f : 92.f;
	const float X = VW - (W + 36.f) * Ease, Y = 48.f;
	auto A = [Alpha](FLinearColor C) { C.A *= Alpha; return C; };
	Box(A(FLinearColor(0.02f, 0.015f, 0.01f, 0.78f)), X, Y, W, H);
	Box(A(Amber), X, Y, 3.f, H);
	Txt(NoirTrS(TEXT("NEW EVIDENCE"), TEXT("NUEVA PRUEBA")), A(Amber), X + 18.f, Y + 12.f, Font, 0.75f);
	Txt(Title, A(Cream), X + 18.f, Y + 32.f, Font, 1.f);
	float Line = Y + 60.f;
	if (ToastMore > 0)
	{
		Txt(FString::Format(*NoirTrS(TEXT("and {0} more"), TEXT("y {0} más")), {ToastMore}), A(Cream), X + 18.f, Line, Font, 0.8f);
		Line += 20.f;
	}
	Txt(Pinned, A(Dim), X + 18.f, Line, Font, 0.78f);
}

void ANoirHUD::DrawBreath(const ANoirCharacter& Detective, float CX, float CY)
{
	// A hairline under the dot while running or getting breath back; gone when it's full.
	const float Stamina = Detective.GetStamina();
	const bool bShow = Detective.IsSprinting() || Stamina < 0.995f;
	BreathAlpha = FMath::FInterpTo(BreathAlpha, bShow ? 1.f : 0.f, GetWorld()->GetDeltaSeconds(), bShow ? 8.f : 2.f);
	if (BreathAlpha < 0.01f) return;
	const float W = 64.f;
	FLinearColor Track(0.1f, 0.08f, 0.06f, 0.55f * BreathAlpha);
	FLinearColor Fill = Stamina < 0.25f ? Oxblood : Cream;
	Fill.A *= 0.8f * BreathAlpha;
	Box(Track, CX - W * 0.5f, CY + 16.f, W, 2.f);
	Box(Fill, CX - W * 0.5f, CY + 16.f, W * Stamina, 2.f);
}

void ANoirHUD::DrawMindTally(const ANoirCharacter& Detective)
{
	if (!Director.IsValid())
	{
		Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(this, ANoirCaseDirector::StaticClass()));
	}
	const ANoirCaseDirector* Case = Director.Get();
	if (!Case) return;
	const FNoirCaseProgress P = Case->GetCaseProgress(&Detective);
	if (P.CluesTotal == 0) return;
	const FString Line = FString::Format(*NoirTrS(
		TEXT("Evidence  {0} of {1}      Searched  {2} of {3}      Questioned  {4} of {5}      Cracked  {6}      Hints  {7}"),
		TEXT("Pruebas  {0} de {1}      Registrado  {2} de {3}      Interrogados  {4} de {5}      Quebrados  {6}      Pistas  {7}")),
		{P.CluesFound, P.CluesTotal, P.Searched, P.SearchTotal, P.Questioned, P.SuspectTotal, P.Cracked, P.HintsUsed});
	Txt(Line, Dim, 40.f, VH - 112.f, GEngine->GetMediumFont(), 0.85f);
}

void ANoirHUD::DrawInterview(const ANoirCharacter& Detective)
{
	const ANoirSuspect* Suspect = Detective.GetInterviewee();
	const float Left = 64.f;
	float Y = VH * 0.12f;

	// Who, and how close they are to cracking.
	const float PanelW = 560.f;
	Box(Panel, Left - 24.f, Y - 20.f, PanelW, 128.f);
	Box(Suspect->SignatureColor, Left - 24.f, Y - 20.f, 4.f, 128.f);
	Txt(Suspect->DisplayName.ToString(), Cream, Left, Y - 6.f, nullptr, 1.6f);
	Txt(Suspect->Occupation.ToString(), Dim, Left, Y + 34.f, nullptr, 1.f);
	const float BarW = PanelW - 190.f;
	// Composure drains as they're pressed; empty, they crack.
	Txt(Suspect->IsCracked() ? NoirTrS(TEXT("CRACKED"), TEXT("SE QUEBRÓ")) : NoirTrS(TEXT("COMPOSURE"), TEXT("COMPOSTURA")),
		Suspect->IsCracked() ? Oxblood : Dim, Left, Y + 66.f, nullptr, 0.78f);
	Box(FLinearColor(0.16f, 0.13f, 0.1f, 0.9f), Left + 130.f, Y + 74.f, BarW, 6.f);  // empty track
	const float Composure = Suspect->IsCracked() ? 0.f : 1.f - Suspect->GetStress();
	Box(Composure < 0.35f ? Oxblood : Cream, Left + 130.f, Y + 74.f, BarW * Composure, 6.f);

	// The menu.
	Y += 150.f;
	const TArray<FNoirInterviewOption> Options = Detective.GetInterviewOptions();
	FString Heading = NoirTrS(TEXT("Ask"), TEXT("Preguntar"));
	if (Detective.IsChoosingStatement())
	{
		Heading = NoirTrS(TEXT("Which statement do you challenge?"), TEXT("¿Qué declaración rebates?"));
	}
	else if (Detective.IsPresenting() && Suspect->Topics.IsValidIndex(Detective.GetChallengedTopic()))
	{
		Heading = NoirTrS(TEXT("Evidence against: "), TEXT("Prueba contra: ")) + Suspect->Topics[Detective.GetChallengedTopic()].Question.ToString();
	}
	else if (Detective.IsPresenting())
	{
		Heading = NoirTrS(TEXT("Present evidence"), TEXT("Mostrar pruebas"));
	}
	const int32 Shown = FMath::Min(Options.Num(), 9);
	Box(Panel, Left - 24.f, Y - 18.f, PanelW + 140.f, 56.f + 34.f * Shown + 84.f);
	Txt(Heading, Amber, Left, Y, nullptr, 1.1f);
	Y += 42.f;
	for (int32 i = 0; i < Shown; ++i)
	{
		Option(FString::FromInt(i + 1), Options[i].Label, Left, Y, Options[i].bDim ? Dim : Cream);
		Y += 34.f;
	}
	Y += 12.f;
	Option(Detective.IsPresenting() ? TEXT("0") : TEXT("P"), Detective.IsPresenting() ? NoirTrS(TEXT("Back"), TEXT("Volver"))
		: Suspect->HasStatements() ? NoirTrS(TEXT("Challenge a statement"), TEXT("Rebatir una declaración"))
		: NoirTrS(TEXT("Present evidence"), TEXT("Mostrar pruebas")), Left, Y, Dim);
	Option(TEXT("E"), NoirTrS(TEXT("Leave"), TEXT("Salir")), Left + 300.f, Y, Dim);

	// What they just said, as a subtitle on a dark band: the speaker's name in amber.
	const FString Line = Detective.GetDialogueLine().ToString();
	if (!Line.IsEmpty())
	{
		const float Scale = 1.2f;
		const float LineH = 34.f;
		const FString Name = Suspect->DisplayName.ToString() + TEXT("   ");
		const TArray<FString> Lines = Wrap(Line, 78);
		float Widest = 0.f;
		for (int32 i = 0; i < Lines.Num(); ++i) Widest = FMath::Max(Widest, TextWidth(i == 0 ? Name + Lines[i] : Lines[i], Scale));
		float SubY = VH * 0.82f - LineH * (Lines.Num() - 1);
		Backdrop(VW * 0.5f, SubY - 12.f, Widest + 40.f, LineH * Lines.Num() + 18.f, 0.6f);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			const float LineW = TextWidth(i == 0 ? Name + Lines[i] : Lines[i], Scale);
			float X = (VW - LineW) * 0.5f;
			if (i == 0)
			{
				Txt(Name, Amber, X, SubY, nullptr, Scale);
				X += TextWidth(Name, Scale);
			}
			Txt(Lines[i], Cream, X, SubY, nullptr, Scale);
			SubY += LineH;
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
	const float W = VW;
	const float H = VH;

	// Letterbox bars ease in for the reveal and stay for the dossier.
	if (const float Bars = Detective.GetLetterbox(); Bars > 0.f)
	{
		const float BarH = H * 0.12f * Bars;
		Box(FLinearColor::Black, 0.f, 0.f, W, BarH);
		Box(FLinearColor::Black, 0.f, H - BarH, W, BarH);
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
		Box(Panel, Left - 30.f, Y - 30.f, PanelW + 60.f, 560.f);
		Txt(NoirTrS(TEXT("ACCUSATION"), TEXT("ACUSACIÓN")), Amber, Left, Y, Font, 1.4f);
		Y += 50.f;

		// What has been decided so far.
		for (int32 Part = 0; Part < Step && Part < Parts; ++Part)
		{
			const ENoirAccusationPart P = static_cast<ENoirAccusationPart>(Part);
			const TArray<FText> Options = Case->GetOptions(P);
			const int32 Pick = Detective.GetAccusePicks()[Part];
			Txt(Case->GetQuestion(P).ToString() + TEXT("  ") + (Options.IsValidIndex(Pick) ? Options[Pick].ToString() : TEXT("")),
				Dim, Left, Y, Font, 0.9f);
			Y += 26.f;
		}
		Y += 14.f;

		if (Step < Parts)
		{
			const ENoirAccusationPart P = static_cast<ENoirAccusationPart>(Step);
			Txt(Case->GetQuestion(P).ToString(), Amber, Left, Y, Font, 1.15f);
			Y += 40.f;
			const TArray<FText> Options = Case->GetOptions(P);
			for (int32 i = 0; i < Options.Num() && i < 9; ++i)
			{
				Option(FString::FromInt(i + 1), Options[i].ToString(), Left, Y, Cream);
				Y += 34.f;
			}
		}
		else
		{
			Txt(NoirTrS(TEXT("Is this your accusation?"), TEXT("¿Es esta tu acusación?")), Amber, Left, Y, Font, 1.15f);
			Y += 40.f;
			Option(TEXT("1"), NoirTrS(TEXT("Make the accusation"), TEXT("Acusar")), Left, Y, Cream);
			Option(TEXT("2"), NoirTrS(TEXT("Start over"), TEXT("Empezar de nuevo")), Left, Y + 34.f, Cream);
		}
		if (Step > 0) Option(TEXT("0"), NoirTrS(TEXT("Back"), TEXT("Volver")), Left, H * 0.2f + 496.f, Dim, 0.9f);
		Option(TEXT("E"), NoirTrS(TEXT("Leave the board"), TEXT("Dejar el tablero")), Left + (Step > 0 ? 160.f : 0.f), H * 0.2f + 496.f, Dim, 0.9f);
		break;
	}
	case ENoirAccusePhase::Reveal:
	{
		const TArray<FString> Lines = Wrap(Detective.GetCurrentRevealLine().ToString(), 80);
		float Y = H * 0.74f - 32.f * (Lines.Num() - 1);
		if (!Lines.IsEmpty() && !Lines[0].IsEmpty())
		{
			// Soft dark band so the line reads over bright practicals behind the accused.
			Box(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, Y - 16.f, W, 32.f * Lines.Num() + 24.f);
		}
		for (const FString& Line : Lines)
		{
			DrawCentered(Line, Y, Cream, 1.2f);
			Y += 32.f;
		}
		Option(TEXT("E"), NoirTrS(TEXT("skip"), TEXT("saltar")), W - 150.f, H - 56.f, Dim, 0.85f);
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
	const float W = VW;
	const float H = VH;
	Box(FLinearColor(0.015f, 0.012f, 0.01f, 0.92f), 0.f, 0.f, W, H);

	const float Left = W * 0.12f;
	float Y = H * 0.1f;
	Txt(NoirTrS(TEXT("DOSSIER"), TEXT("EXPEDIENTE")), Amber, Left, Y, Font, 1.5f);
	Txt(Case->Title.ToString(), Cream, Left, Y + 44.f, Font, 1.1f);
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
		Txt(Case->GetQuestion(P).ToString(), Dim, Left, Y, Font, 0.9f);
		const FString Verdict = bRight
			? FString::Format(*NoirTrS(TEXT("RIGHT   {0}"), TEXT("BIEN   {0}")), {Said})
			: FString::Format(*NoirTrS(TEXT("WRONG   you said \"{0}\"  -  it was \"{1}\""), TEXT("MAL   dijiste «{0}»  -  era «{1}»")), {Said, Real});
		Txt(Verdict, bRight ? Cream : Amber, Left + 260.f, Y, Font);
		Y += 32.f;
	}
	Y += 10.f;
	Txt(FString::Format(*NoirTrS(TEXT("{0} of {1}"), TEXT("{0} de {1}")), {Correct, Parts}), Amber, Left + 260.f, Y, Font, 1.1f);
	Y += 60.f;

	// What really happened.
	Txt(NoirTrS(TEXT("WHAT REALLY HAPPENED"), TEXT("LO QUE PASÓ DE VERDAD")), Amber, Left, Y, Font, 1.1f);
	Y += 40.f;
	for (const FNoirTimelineEntry& Entry : Case->Timeline)
	{
		Txt(Entry.Time.ToString(), Amber, Left, Y, Font);
		for (const FString& Line : Wrap(Entry.Event.ToString(), 90))
		{
			Txt(Line, Cream, Left + 120.f, Y, Font);
			Y += 28.f;
		}
		Y += 6.f;
	}
	Option(TEXT("E"), NoirTrS(TEXT("Close the case"), TEXT("Cerrar el caso")), Left, H - 74.f, Dim, 0.9f);
}

void ANoirHUD::DrawBoard(const ANoirCharacter& Detective)
{
	const ANoirCase* Board = Detective.GetBoard();
	UFont* Font = GEngine->GetMediumFont();
	const float W = VW;
	const float H = VH;

	// The hovered card, written out in full.
	if (const FNoirBoardCard* Card = Board ? Board->FindCard(Detective.GetHoveredCard()) : nullptr)
	{
		const TArray<FString> Lines = Wrap(Card->Text.ToString(), 70);
		const float PanelH = 56.f + 24.f * Lines.Num();
		const float Top = H - 110.f - PanelH;
		Box(Panel, 30.f, Top, 760.f, PanelH);
		FString Heading = Card->Title.ToString();
		if (!Card->TimeLabel.IsEmpty())
		{
			Heading = Card->TimeLabel.ToString() + TEXT("   ") + Heading;
		}
		Txt(Heading, Amber, 50.f, Top + 14.f, Font, 1.05f);
		float Y = Top + 46.f;
		for (const FString& Line : Lines)
		{
			Txt(Line, Cream, 50.f, Y, Font, 0.9f);
			Y += 24.f;
		}
	}

	if (const FNoirBoardCard* Picked = Board ? Board->FindCard(Detective.GetPickedCard()) : nullptr)
	{
		DrawCentered(FString::Format(*NoirTrS(TEXT("Tying string from \"{0}\": right-click another card"),
			TEXT("Atando hilo desde «{0}»: clic derecho en otra tarjeta")), {Picked->Title.ToString()}), H * 0.08f, Amber);
	}

	Hints({{NoirTrS(TEXT("Wheel"), TEXT("Rueda")), NoirTrS(TEXT("look closer"), TEXT("acercarse"))},
		{TEXT("WASD"), NoirTrS(TEXT("move along"), TEXT("desplazarse"))},
		{NoirTrS(TEXT("Drag"), TEXT("Arrastrar")), NoirTrS(TEXT("move a card"), TEXT("mover"))},
		{NoirTrS(TEXT("Right-click"), TEXT("Clic der.")), NoirTrS(TEXT("tie two cards"), TEXT("unir dos"))},
		{TEXT("A"), NoirTrS(TEXT("accuse"), TEXT("acusar"))},
		{TEXT("E"), NoirTrS(TEXT("step back"), TEXT("apartarse"))}}, H - 62.f);
}
