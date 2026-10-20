#include "NoirPuzzle.h"

using namespace NoirDraw;

#include "Algo/Reverse.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "NoirCaseContent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Cream(0.86f, 0.8f, 0.68f);
	const FLinearColor Amber(1.f, 0.62f, 0.22f);
	const FLinearColor Dim(0.5f, 0.47f, 0.42f);
	const FLinearColor Blood(0.75f, 0.12f, 0.1f);
	const FLinearColor Solved(0.45f, 0.78f, 0.52f);

	FSlateFontInfo Font(const char* Weight, float Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	const FSlateBrush* Fill(const FLinearColor& Color)
	{
		static TMap<uint32, TSharedPtr<FSlateColorBrush>> Brushes;
		TSharedPtr<FSlateColorBrush>& B = Brushes.FindOrAdd(GetTypeHash(Color.ToFColor(false)));
		if (!B) B = MakeShared<FSlateColorBrush>(Color);
		return B.Get();
	}

	/** A shoe's sole and its tread, in an 84 x 176 box at Origin, scaled. */
	void Sole(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Origin, float S, const FString& Kind,
		const FLinearColor& Tone)
	{
		auto At = [&](float X, float Y) { return Origin + FVector2D(X, Y) * S; };
		Oval(G, Out, Layer, At(42, 88), FVector2D(60, 156) * S, Tone, true);
		if (Kind == TEXT("boots"))
		{
			for (int32 y = 20; y <= 100; y += 20)
				for (int32 x : {21, 41, 61}) Box(G, Out, Layer, At(x, y + 6), FVector2D(13, 11) * S, Tone);
			for (int32 y : {135, 153}) Box(G, Out, Layer, At(40, y), FVector2D(34, 9) * S, Tone);
		}
		else if (Kind == TEXT("heels"))
		{
			for (int32 y : {30, 48, 66}) Oval(G, Out, Layer, At(42, y), FVector2D(6, 6) * S, Tone);
			Oval(G, Out, Layer, At(40, 150), FVector2D(14, 14) * S, Tone);
		}
		else if (Kind == TEXT("oxfords"))
		{
			Oval(G, Out, Layer, At(42, 88), FVector2D(46, 134) * S, Tone * FLinearColor(1, 1, 1, 0.7f), true);
			Lines(G, Out, Layer, {At(20, 128), At(60, 128), At(60, 158), At(20, 158), At(20, 128)}, Tone, 2.f * S);
			for (int32 x : {30, 50})
				for (int32 y : {138, 150}) Oval(G, Out, Layer, At(x, y), FVector2D(4, 4) * S, Tone);
		}
		else // rubber: zigzags across the whole sole
		{
			for (int32 y = 24; y <= 150; y += 18)
			{
				TArray<FVector2D> Zig;
				for (int32 k = 0; k <= 6; ++k) Zig.Add(At(18 + k * 8, y - (k % 2) * 7));
				Lines(G, Out, Layer, Zig, Tone, 2.5f * S);
			}
		}
	}

	const FButtonStyle& PlainButton()
	{
		static FButtonStyle Style = []
		{
			FButtonStyle S;
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.04f), 4.f, FLinearColor(0.86f, 0.8f, 0.68f, 0.35f), 1.f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.62f, 0.22f, 0.12f), 4.f, FLinearColor(1.f, 0.62f, 0.22f, 0.8f), 1.f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.62f, 0.22f, 0.22f), 4.f, FLinearColor(1.f, 0.62f, 0.22f, 1.f), 1.f));
			S.SetDisabled(FSlateNoResource());
			S.SetNormalPadding(FMargin(0.f));
			S.SetPressedPadding(FMargin(0.f));
			return S;
		}();
		return Style;
	}

	const FButtonStyle& ScrapButton()
	{
		static FButtonStyle Style = []
		{
			FButtonStyle S;
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.62f, 0.56f, 0.44f), 2.f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.72f, 0.65f, 0.5f), 2.f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.7f, 0.55f), 2.f));
			S.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.62f, 0.56f, 0.44f, 0.25f), 2.f));
			S.SetNormalPadding(FMargin(0.f));
			S.SetPressedPadding(FMargin(0.f));
			return S;
		}();
		return Style;
	}

	TSharedRef<SWidget> Button(TAttribute<FText> Label, TFunction<void()> OnClick, float Size = 15.f)
	{
		return SNew(SButton)
			.ButtonStyle(&PlainButton())
			.ContentPadding(FMargin(14.f, 7.f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font("Regular", Size, 30)).ColorAndOpacity(Cream)
			];
	}

	FString ShiftText(const FString& Text, int32 By)
	{
		FString Out = Text;
		for (TCHAR& C : Out)
		{
			if (C >= 'A' && C <= 'Z') C = TCHAR('A' + ((C - 'A' + By) % 26 + 26) % 26);
			else if (C >= 'a' && C <= 'z') C = TCHAR('a' + ((C - 'a' + By) % 26 + 26) % 26);
		}
		return Out;
	}

	FLinearColor UvColor(const FString& Key)
	{
		if (Key == TEXT("cigar")) return FLinearColor(0.85f, 0.82f, 0.76f);
		if (Key == TEXT("ink")) return FLinearColor(0.37f, 0.7f, 1.f);
		if (Key == TEXT("mint")) return FLinearColor(0.49f, 1.f, 0.72f);
		if (Key == TEXT("cat") || Key == TEXT("planted")) return FLinearColor(0.96f, 0.97f, 1.f);
		return FLinearColor(0.79f, 0.64f, 1.f); // lavender
	}

	FText SoleName(const FString& Kind)
	{
		if (Kind == TEXT("boots")) return NoirTr(TEXT("Riding boots"), TEXT("Botas de montar"));
		if (Kind == TEXT("heels")) return NoirTr(TEXT("Heeled shoes"), TEXT("Zapatos de tacón"));
		if (Kind == TEXT("oxfords")) return NoirTr(TEXT("Polished oxfords"), TEXT("Zapatos de vestir"));
		return NoirTr(TEXT("Rubber soles"), TEXT("Suela de goma"));
	}
}

// ─── SNoirPuzzle ───

void SNoirPuzzle::Construct(const FArguments& InArgs)
{
	Clue = InArgs._Clue;
	OnSolved = InArgs._OnSolved;
	OnClose = InArgs._OnClose;
	const FNoirPuzzle& P = Clue.Puzzle;

	FText Title, Body;
	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	if (P.Type == TEXT("hand"))
	{
		Title = NoirTr(TEXT("Which hand made these prints?"), TEXT("¿Qué mano dejó estas huellas?"));
		Body = NoirTr(TEXT("Flip the hand outline until the thumb and fingers sit on the smudges."),
			TEXT("Gira el contorno de la mano hasta que el pulgar y los dedos coincidan con las marcas."));
		Content = MakeHand();
	}
	else if (P.Type == TEXT("sole"))
	{
		Title = NoirTr(TEXT("Match the shoeprint"), TEXT("Identifica la pisada"));
		Body = NoirTr(TEXT("Compare the plaster cast with the reference soles and pick the one that matches."),
			TEXT("Compara el molde de yeso con las suelas de referencia y elige la que coincide."));
		Content = MakeSole();
	}
	else if (P.Type == TEXT("uv"))
	{
		Title = NoirTr(TEXT("Sweep the lamp over it"), TEXT("Pasa la lámpara por encima"));
		Body = NoirTr(TEXT("Ultraviolet light shows what the eye misses. Move the lamp slowly over the fabric."),
			TEXT("La luz ultravioleta muestra lo que el ojo no ve. Mueve la lámpara despacio sobre la tela."));
		Content = MakeUv();
	}
	else if (P.Type == TEXT("blood"))
	{
		Title = NoirTr(TEXT("Type the blood"), TEXT("Determina el grupo sanguíneo"));
		Body = NoirTr(TEXT("Add a drop of the sample to each serum. Clumping means the blood carries that antigen."),
			TEXT("Añade una gota de la muestra a cada suero. Si se aglutina, la sangre tiene ese antígeno."));
		Content = MakeBlood();
	}
	else if (P.Type == TEXT("torn"))
	{
		Title = NoirTr(TEXT("Piece it back together"), TEXT("Vuelve a unir los trozos"));
		Body = NoirTr(TEXT("Pick the scraps in the order they belong."), TEXT("Elige los trozos en el orden correcto."));
		Content = MakeTorn();
	}
	else if (P.Type == TEXT("cipher"))
	{
		Title = NoirTr(TEXT("The entry is in code"), TEXT("La entrada está en clave"));
		Body = NoirTr(TEXT("Turn the dial until the entry makes sense."), TEXT("Gira el dial hasta que la entrada tenga sentido."));
		Content = MakeCipher();
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Fill(FLinearColor(0.f, 0.f, 0.f, 0.45f))).Padding(0.f)]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(820.f)
			[
				SNew(SBorder).BorderImage(Fill(FLinearColor(0.012f, 0.01f, 0.008f, 0.94f))).Padding(FMargin(44.f, 34.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Clue.Title).Font(Font("Regular", 13, 200)).ColorAndOpacity(Amber)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(STextBlock).Text(Title).Font(Font("Light", 26, 10)).ColorAndOpacity(Cream)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
					[
						SNew(STextBlock).Text(Body).Font(Font("Regular", 13)).ColorAndOpacity(Dim).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)[Content]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(STextBlock).Text_Lambda([this]() { return Status; }).Font(Font("Regular", 13)).ColorAndOpacity(Blood)
						.Visibility_Lambda([this]() { return Status.IsEmpty() || bSolved ? EVisibility::Collapsed : EVisibility::Visible; })
					]
					// What it really says, once worked out.
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this]() { return bSolved ? EVisibility::Visible : EVisibility::Collapsed; })
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(NoirTr(TEXT("WORKED OUT"), TEXT("RESUELTO"))).Font(Font("Bold", 13, 300)).ColorAndOpacity(Solved)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
						[
							SNew(STextBlock).Text(Clue.Puzzle.Reveal).Font(Font("Regular", 15)).ColorAndOpacity(Cream).AutoWrapText(true)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).Visibility_Lambda([this]() { return bSolved ? EVisibility::Collapsed : EVisibility::Visible; })
							[
								Button(NoirTr(TEXT("Just show me"), TEXT("Muéstramelo sin más")), [this]() { Solve(); }, 13.f)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							Button(TAttribute<FText>::CreateLambda([this]()
							{
								return bSolved ? NoirTr(TEXT("Note it down"), TEXT("Anotarlo")) : NoirTr(TEXT("Put it aside   (Esc)"), TEXT("Dejarlo para luego   (Esc)"));
							}), [this]() { if (OnClose) OnClose(); }, 15.f)
						]
					]
				]
			]
		]
	];
}

FReply SNoirPuzzle::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::R)
	{
		if (OnClose) OnClose();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Event);
}

void SNoirPuzzle::Solve()
{
	if (bSolved) return;
	bSolved = true;
	Status = FText();
	if (OnSolved) OnSolved();
}

void SNoirPuzzle::Wrong(const FText& Why)
{
	Status = Why;
}

void SNoirPuzzle::SolveForTest()
{
	const FNoirPuzzle& P = Clue.Puzzle;
	if (P.Type == TEXT("cipher")) Dial = P.Shift;
	if (P.Type == TEXT("hand")) bRightHand = P.Answer == TEXT("right");
	Solve();
}

// ─── Which hand? ───

TSharedRef<SWidget> SNoirPuzzle::MakeHand()
{
	const bool bAnswerRight = Clue.Puzzle.Answer == TEXT("right");
	bRightHand = !bAnswerRight; // start the wrong way round
	TSharedRef<SNoirSketch> Sketch = SNew(SNoirSketch).Size(FVector2D(480, 330));
	Sketch->Paint = [this, bAnswerRight](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
	{
		const float S = 1.5f;
		// For a right hand the thumb sits on the left of the lapel, the fingers on the right.
		auto ThumbX = [](bool bRight) { return bRight ? 90.f : 230.f; };
		auto FingerX = [](bool bRight) { return bRight ? 230.f : 90.f; };
		Box(G, Out, L, FVector2D(240, 165), FVector2D(480, 330), FLinearColor(0.025f, 0.028f, 0.033f));
		Box(G, Out, L, FVector2D(240, 165), FVector2D(200, 330), FLinearColor(0.045f, 0.05f, 0.06f));
		Lines(G, Out, L, {FVector2D(240, 0), FVector2D(240, 330)}, FLinearColor(0.015f, 0.017f, 0.02f), 2.f);
		const FLinearColor Smudge(0.2f, 0.23f, 0.27f, 0.8f);
		Oval(G, Out, L + 1, FVector2D(ThumbX(bAnswerRight), 120) * S, FVector2D(30, 44) * S, Smudge);
		for (float Y : {60.f, 95.f, 130.f, 162.f}) Oval(G, Out, L + 1, FVector2D(FingerX(bAnswerRight), Y) * S, FVector2D(26, 22) * S, Smudge);
		const FLinearColor Outline(1.f, 0.82f, 0.48f, 0.65f);
		Oval(G, Out, L + 2, FVector2D(160, 120) * S, FVector2D(90, 100) * S, Outline, true);
		Oval(G, Out, L + 2, FVector2D(ThumbX(bRightHand), 121) * S, FVector2D(32, 52) * S, Outline, true);
		for (float Y : {60.f, 95.f, 130.f, 162.f}) Oval(G, Out, L + 2, FVector2D(FingerX(bRightHand), Y) * S, FVector2D(32, 24) * S, Outline, true);
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[Sketch]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Button(NoirTr(TEXT("Flip hand"), TEXT("Girar mano")), [this]() { bRightHand = !bRightHand; Status = FText(); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0)
			[
				SNew(SBox).WidthOverride(160.f)
				[
					SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Cream).Text_Lambda([this]()
					{
						return bRightHand ? NoirTr(TEXT("Right hand"), TEXT("Mano derecha")) : NoirTr(TEXT("Left hand"), TEXT("Mano izquierda"));
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Button(NoirTr(TEXT("It fits"), TEXT("Encaja")), [this, bAnswerRight]()
			{
				if (bRightHand == bAnswerRight) Solve();
				else Wrong(NoirTr(TEXT("The thumb doesn't sit on the smudge. Look again."), TEXT("El pulgar no coincide con la marca. Vuelve a mirar.")));
			})]
		];
}

// ─── Match the sole ───

TSharedRef<SWidget> SNoirPuzzle::MakeSole()
{
	const FString Answer = Clue.Puzzle.Answer;
	TSharedRef<SNoirSketch> Cast = SNew(SNoirSketch).Size(FVector2D(84, 176) * 1.2);
	Cast->Paint = [Answer](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
	{
		Box(G, Out, L, FVector2D(84, 176) * 0.6, FVector2D(84, 176) * 1.2, FLinearColor(0.25f, 0.2f, 0.14f));
		Sole(G, Out, L + 1, FVector2D::ZeroVector, 1.2f, Answer, FLinearColor(0.93f, 0.9f, 0.82f));
	};
	TSharedRef<SHorizontalBox> Refs = SNew(SHorizontalBox);
	for (const TCHAR* Kind : {TEXT("boots"), TEXT("heels"), TEXT("oxfords"), TEXT("rubber")})
	{
		const FString K = Kind;
		TSharedRef<SNoirSketch> Ref = SNew(SNoirSketch).Size(FVector2D(84, 176) * 0.8);
		Ref->Paint = [K](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
		{
			Box(G, Out, L, FVector2D(84, 176) * 0.4, FVector2D(84, 176) * 0.8, FLinearColor(0.55f, 0.5f, 0.4f));
			Sole(G, Out, L + 1, FVector2D::ZeroVector, 0.8f, K, FLinearColor(0.08f, 0.06f, 0.04f));
		};
		Refs->AddSlot().AutoWidth().Padding(6, 0)
		[
			SNew(SButton).ButtonStyle(&PlainButton()).ContentPadding(FMargin(8.f))
			.OnClicked_Lambda([this, K, Answer]()
			{
				if (K == Answer) Solve();
				else Wrong(NoirTr(TEXT("No match. Look at the tread again."), TEXT("No coincide. Vuelve a mirar el dibujo.")));
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Ref]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
				[
					SNew(STextBlock).Text(SoleName(K)).Font(Font("Regular", 11)).ColorAndOpacity(Cream)
				]
			]
		];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Cast]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
			[
				SNew(STextBlock).Text(NoirTr(TEXT("Cast from the scene"), TEXT("Molde de la escena"))).Font(Font("Bold", 11)).ColorAndOpacity(Dim)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(28, 0, 0, 0).VAlign(VAlign_Bottom)[Refs];
}

// ─── UV lamp ───

TSharedRef<SWidget> SNoirPuzzle::MakeUv()
{
	const FVector2D Size(560, 260);
	const uint32 H = GetTypeHash(Clue.Id);
	Spot = FVector2D(0.2f + (H % 60) / 100.f, 0.25f + ((H >> 6) % 50) / 100.f) * Size;
	const FLinearColor Color = UvColor(Clue.Puzzle.Key);
	const bool bThreads = Clue.Puzzle.Key == TEXT("cat");
	TSharedRef<SNoirSketch> Sketch = SNew(SNoirSketch).Size(Size);
	Sketch->Paint = [this, Size, Color, bThreads](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
	{
		Box(G, Out, L, Size * 0.5, Size, FLinearColor(0.02f, 0.016f, 0.03f));
		for (int32 i = 0; i < 26; ++i)
		{
			const FVector2D P = Spot + FVector2D(FMath::Sin(i * 7.1f) * 40.f, FMath::Cos(i * 3.3f) * 26.f);
			const float Near = 1.f - FMath::Clamp((FVector2D::Distance(P, Lamp) - 40.f) / 40.f, 0.f, 1.f);
			const float A = bFound ? 1.f : Near;
			if (A <= 0.01f) continue;
			const FLinearColor C = Color * FLinearColor(1, 1, 1, A);
			if (bThreads) Lines(G, Out, L + 1, {P, P + FVector2D(9, -6), P + FVector2D(18, 2)}, C, 1.4f);
			else Oval(G, Out, L + 1, P, FVector2D(3.f + (i % 4) * 2.f), C);
		}
		if (!bFound && Lamp.X > -500.f)
		{
			Oval(G, Out, L + 2, Lamp, FVector2D(150, 150), FLinearColor(0.55f, 0.35f, 1.f, 0.35f), true);
			Oval(G, Out, L + 2, Lamp, FVector2D(120, 120), FLinearColor(0.55f, 0.35f, 1.f, 0.08f));
		}
	};
	Sketch->OnMove = [this](const FVector2D& P)
	{
		Lamp = P;
		if (bFound || FVector2D::Distance(P, Spot) > 55.f) return;
		if (++Dwell > 18)
		{
			bFound = true;
			RegisterActiveTimer(0.9f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
			{
				Solve();
				return EActiveTimerReturnType::Stop;
			}));
		}
	};
	return Sketch;
}

// ─── Blood typing ───

TSharedRef<SWidget> SNoirPuzzle::MakeBlood()
{
	const FString Answer = Clue.Puzzle.Answer;
	auto Well = [this, Answer](bool bA) -> TSharedRef<SWidget>
	{
		const bool bClumps = Answer.Contains(bA ? TEXT("A") : TEXT("B"));
		TSharedRef<SNoirSketch> Sketch = SNew(SNoirSketch).Size(FVector2D(120, 120));
		Sketch->Paint = [this, bA, bClumps](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
		{
			Oval(G, Out, L, FVector2D(60, 60), FVector2D(116, 116), FLinearColor(0.85f, 0.83f, 0.78f, 0.85f));
			Oval(G, Out, L + 1, FVector2D(60, 60), FVector2D(116, 116), bA ? FLinearColor(0.25f, 0.5f, 0.85f) : FLinearColor(0.85f, 0.7f, 0.25f), true);
			if (!(bA ? bAddedA : bAddedB)) return;
			if (bClumps)
				for (int32 i = 0; i < 14; ++i)
					Oval(G, Out, L + 2, FVector2D(24 + (i * 37) % 72, 24 + (i * 53) % 72), FVector2D(8.f + (i % 3) * 4.f), FLinearColor(0.5f, 0.06f, 0.05f));
			else
				Oval(G, Out, L + 2, FVector2D(60, 60), FVector2D(96, 96), FLinearColor(0.7f, 0.12f, 0.1f, 0.75f));
		};
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Sketch]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
			[
				SNew(STextBlock).Text(bA ? NoirTr(TEXT("Anti-A serum"), TEXT("Suero anti-A")) : NoirTr(TEXT("Anti-B serum"), TEXT("Suero anti-B")))
				.Font(Font("Bold", 12)).ColorAndOpacity(Cream)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
			[
				SNew(SBox).Visibility_Lambda([this, bA]() { return (bA ? bAddedA : bAddedB) ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					Button(NoirTr(TEXT("Add sample"), TEXT("Añadir muestra")), [this, bA]() { (bA ? bAddedA : bAddedB) = true; }, 12.f)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
			[
				SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Dim)
				.Text(bClumps ? NoirTr(TEXT("Clumped"), TEXT("Aglutinada")) : NoirTr(TEXT("Smooth"), TEXT("Uniforme")))
				.Visibility_Lambda([this, bA]() { return (bA ? bAddedA : bAddedB) ? EVisibility::Visible : EVisibility::Collapsed; })
			];
	};
	TSharedRef<SHorizontalBox> Types = SNew(SHorizontalBox);
	for (const TCHAR* Type : {TEXT("A"), TEXT("B"), TEXT("AB"), TEXT("O")})
	{
		const FString T = Type;
		Types->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[Button(FText::FromString(T), [this, T, Answer]()
		{
			if (T == Answer) Solve();
			else Wrong(NoirTr(TEXT("That doesn't fit how the sample reacted."), TEXT("No cuadra con cómo reaccionó la muestra.")));
		}, 15.f)];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Well(true)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(30, 0, 0, 0)[Well(false)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]() { return bAddedA && bAddedB ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(NoirTr(TEXT("Which blood type is it?"), TEXT("¿Qué grupo sanguíneo es?"))).Font(Font("Bold", 13)).ColorAndOpacity(Cream)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[Types]
		];
}

// ─── Torn letter ───

TSharedRef<SWidget> SNoirPuzzle::MakeTorn()
{
	const int32 N = Clue.Puzzle.Pieces.Num();
	for (int32 i = 0; i < N; ++i) Shuffled.Add(i);
	const uint32 H = GetTypeHash(Clue.Id);
	for (int32 i = N - 1; i > 0; --i) Shuffled.Swap(i, (H >> i) % (i + 1));
	bool bInOrder = true;
	for (int32 i = 0; i < N; ++i) bInOrder &= Shuffled[i] == i;
	if (bInOrder) Algo::Reverse(Shuffled);

	auto Scrap = [this](int32 i) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock).Text(FText::FromString(Clue.Puzzle.Pieces[i])).Font(FCoreStyle::GetDefaultFontStyle("Mono", 12))
			.ColorAndOpacity(FLinearColor(0.13f, 0.09f, 0.05f)).AutoWrapText(true);
	};
	TSharedRef<SWrapBox> Pool = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
	for (const int32 i : Shuffled)
	{
		Pool->AddSlot()
		[
			SNew(SBox).WidthOverride(236.f)
			[
				SNew(SButton).ButtonStyle(&ScrapButton()).ContentPadding(FMargin(12.f, 10.f))
				.IsEnabled_Lambda([this, i]() { return !Placed.Contains(i) && !bSolved; })
				.OnClicked_Lambda([this, i, N]()
				{
					Placed.Add(i);
					Status = FText();
					RebuildTorn();
					if (Placed.Num() == N)
					{
						bool bRight = true;
						for (int32 k = 0; k < N; ++k) bRight &= Placed[k] == k;
						if (bRight) Solve();
						else
						{
							Wrong(NoirTr(TEXT("That doesn't read right. Start again."), TEXT("Así no tiene sentido. Empieza de nuevo.")));
							Placed.Reset();
							RebuildTorn();
						}
					}
					return FReply::Handled();
				})
				[Scrap(i)]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(Fill(FLinearColor(1, 1, 1, 0.03f))).Padding(10.f)
			[
				SNew(SBox).MinDesiredHeight(60.f)[SAssignNew(PlacedBox, SVerticalBox)]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)[Pool];
}

void SNoirPuzzle::RebuildTorn()
{
	if (!PlacedBox) return;
	PlacedBox->ClearChildren();
	for (const int32 i : Placed)
	{
		PlacedBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(STextBlock).Text(FText::FromString(Clue.Puzzle.Pieces[i])).Font(FCoreStyle::GetDefaultFontStyle("Mono", 12))
			.ColorAndOpacity(Cream).AutoWrapText(true)
		];
	}
}

// ─── Cipher dial ───

TSharedRef<SWidget> SNoirPuzzle::MakeCipher()
{
	Encoded = ShiftText(Clue.Puzzle.Reveal.ToString(), Clue.Puzzle.Shift);
	auto Turn = [this](int32 To)
	{
		Dial = (To + 26) % 26;
		if (Dial == Clue.Puzzle.Shift && !bSolved)
		{
			RegisterActiveTimer(0.7f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
			{
				if (Dial == Clue.Puzzle.Shift) Solve();
				return EActiveTimerReturnType::Stop;
			}));
		}
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 14)).AutoWrapText(true)
			.Text_Lambda([this]() { return FText::FromString(ShiftText(Encoded, -Dial)); })
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(Dial == Clue.Puzzle.Shift ? Solved : Cream); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(NoirTr(TEXT("Shift"), TEXT("Desplazamiento"))).Font(Font("Bold", 12)).ColorAndOpacity(Dim)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 6, 0)[Button(FText::FromString(TEXT("<")), [this, Turn]() { Turn(Dial - 1); })]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).MinValue(0.f).MaxValue(25.f).StepSize(1.f).MouseUsesStep(true)
				.Value_Lambda([this]() { return float(Dial); })
				.OnValueChanged_Lambda([Turn](float V) { Turn(FMath::RoundToInt(V)); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 12, 0)[Button(FText::FromString(TEXT(">")), [this, Turn]() { Turn(Dial + 1); })]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(30.f)
				[
					SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 16)).ColorAndOpacity(Cream)
					.Text_Lambda([this]() { return FText::AsNumber(Dial); })
				]
			]
		];
}
