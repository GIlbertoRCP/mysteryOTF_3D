#include "NoirMap.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "NoirCaseContent.h"
#include "NoirCaseDirector.h"
#include "NoirCharacter.h"
#include "NoirRoom.h"
#include "NoirSearchable.h"
#include "NoirSketch.h"
#include "NoirSuspect.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace NoirDraw;

namespace
{
	const FVector2D Canvas(1000.f, 560.f);
	const FLinearColor Parchment(0.42f, 0.35f, 0.24f);
	const FLinearColor Ink(0.07f, 0.045f, 0.03f);
	const FLinearColor FadedInk(0.07f, 0.045f, 0.03f, 0.45f);
	const FLinearColor SceneWash(0.45f, 0.08f, 0.05f, 0.22f);
	const FLinearColor You(0.75f, 0.32f, 0.05f);

	const FSlateBrush* Fill(const FLinearColor& Color)
	{
		static TMap<uint32, TSharedPtr<FSlateColorBrush>> Brushes;
		TSharedPtr<FSlateColorBrush>& B = Brushes.FindOrAdd(GetTypeHash(Color.ToFColor(false)));
		if (!B) B = MakeShared<FSlateColorBrush>(Color);
		return B.Get();
	}

	FSlateFontInfo Mono(float Size) { return FCoreStyle::GetDefaultFontStyle("Mono", Size); }

	/** Text centred (or left-aligned) on a point of the sketch. */
	void Write(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FString& Text, const FVector2D& At,
		const FSlateFontInfo& Font, const FLinearColor& Color, bool bCentred = true)
	{
		const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		const FVector2D Corner = bCentred ? At - Size * 0.5 : At - FVector2D(0, Size.Y * 0.5);
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Corner))),
			Text, Font, ESlateDrawEffect::None, Color);
	}

	void Square(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& At, float Side, bool bFilled)
	{
		if (bFilled)
		{
			Box(G, Out, Layer, At, FVector2D(Side), Ink);
			return;
		}
		const float H = Side * 0.5f;
		Lines(G, Out, Layer, {At + FVector2D(-H, -H), At + FVector2D(H, -H), At + FVector2D(H, H), At + FVector2D(-H, H), At + FVector2D(-H, -H)}, Ink, 1.5f);
	}

	void Arrow(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& At, const FVector2D& Dir, float Size)
	{
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D Tip = At + Dir * Size, L = At - Dir * Size * 0.6 + Side * Size * 0.6, R = At - Dir * Size * 0.6 - Side * Size * 0.6;
		Lines(G, Out, Layer, {Tip, L, At - Dir * Size * 0.2, R, Tip}, You, 3.f);
	}
}

void SNoirMap::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;
	Gather(InArgs._Detective.Get());

	TSharedRef<SNoirSketch> Sketch = SNew(SNoirSketch).Size(Canvas);
	Sketch->Paint = [this](const FGeometry& G, FSlateWindowElementList& Out, int32 L)
	{
		for (const FRoomMark& R : Rooms)
		{
			// Rooms are drawn square to the page; a wash where the body was found.
			FVector2D Lo(FLT_MAX), Hi(-FLT_MAX);
			for (const FVector2D& C : R.Corners) { Lo = FVector2D::Min(Lo, C); Hi = FVector2D::Max(Hi, C); }
			if (R.bScene) Box(G, Out, L, (Lo + Hi) * 0.5, Hi - Lo, SceneWash);
			TArray<FVector2D> Outline = R.Corners;
			Outline.Add(R.Corners[0]);
			Lines(G, Out, L + 1, Outline, Ink, 2.5f);
		}
		for (const FThingMark& F : Furniture) Square(G, Out, L + 2, F.At, 9.f, F.bSearched);
		for (const FRoomMark& R : Rooms)
		{
			const FSlateFontInfo Font = Mono(11);
			if (R.Label.IsEmpty())
			{
				Write(G, Out, L + 3, TEXT("—"), R.Centre, Font, FadedInk);
				continue;
			}
			// Long names over two lines in narrow rooms.
			FString A = R.Label, B;
			if (R.Label.Len() > 13)
			{
				int32 Space = INDEX_NONE;
				R.Label.FindLastChar(TEXT(' '), Space);
				if (Space > 0) { A = R.Label.Left(Space); B = R.Label.Mid(Space + 1); }
			}
			Write(G, Out, L + 3, A, R.Centre - FVector2D(0, B.IsEmpty() ? 0 : 8), Font, Ink);
			if (!B.IsEmpty()) Write(G, Out, L + 3, B, R.Centre + FVector2D(0, 8), Font, Ink);
			if (R.bScene) Write(G, Out, L + 3, NoirTrS(TEXT("(the body)"), TEXT("(el cuerpo)")), R.Centre + FVector2D(0, B.IsEmpty() ? 16 : 24), Mono(9), FLinearColor(0.4f, 0.05f, 0.03f));
		}
		for (const FPersonMark& P : Suspects)
		{
			Oval(G, Out, L + 4, P.At, FVector2D(12, 12), P.Color);
			Oval(G, Out, L + 4, P.At, FVector2D(12, 12), Ink, true);
			Write(G, Out, L + 4, P.Name, P.At + FVector2D(10, 0), Mono(9), Ink, false);
		}
		Arrow(G, Out, L + 5, You, YouFacing, 13.f);

		// The key, bottom left.
		const FVector2D K(14, Canvas.Y - 16);
		Square(G, Out, L + 5, K, 9.f, true);
		Write(G, Out, L + 5, NoirTrS(TEXT("searched"), TEXT("registrado")), K + FVector2D(10, 0), Mono(9), Ink, false);
		Square(G, Out, L + 5, K + FVector2D(110, 0), 9.f, false);
		Write(G, Out, L + 5, NoirTrS(TEXT("not yet searched"), TEXT("sin registrar")), K + FVector2D(120, 0), Mono(9), Ink, false);
		Oval(G, Out, L + 5, K + FVector2D(270, 0), FVector2D(10, 10), FadedInk);
		Write(G, Out, L + 5, NoirTrS(TEXT("a suspect"), TEXT("un sospechoso")), K + FVector2D(280, 0), Mono(9), Ink, false);
		Arrow(G, Out, L + 5, K + FVector2D(380, 0), FVector2D(1, 0), 7.f);
		Write(G, Out, L + 5, NoirTrS(TEXT("you"), TEXT("tú")), K + FVector2D(392, 0), Mono(9), Ink, false);
	};

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Fill(FLinearColor(0.f, 0.f, 0.f, 0.55f))).Padding(0.f)]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(Fill(Parchment)).Padding(FMargin(34.f, 26.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Text(Place).Font(FCoreStyle::GetDefaultFontStyle("Light", 28)).ColorAndOpacity(Ink)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(NoirTr(TEXT("M / Esc   close"), TEXT("M / Esc   cerrar"))).Font(Mono(11)).ColorAndOpacity(Ink)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 14)
				[
					SNew(SBorder).BorderImage(Fill(Ink)).Padding(FMargin(0.f, 1.f, 0.f, 0.f))
				]
				+ SVerticalBox::Slot().AutoHeight()[Sketch]
			]
		]
	];
}

FVector2D SNoirMap::ToMap(const FVector& World) const
{
	// North (+Y) up the page.
	return Offset + FVector2D((World.X - WorldMin.X) * Scale.X, (WorldMax.Y - World.Y) * Scale.Y);
}

void SNoirMap::Gather(ANoirCharacter* Detective)
{
	UWorld* World = Detective ? Detective->GetWorld() : nullptr;
	if (!World) return;
	const ANoirCaseDirector* Director = nullptr;
	for (TActorIterator<ANoirCaseDirector> It(World); It; ++It) Director = *It;
	Place = Director && Director->Theme == TEXT("express") ? NoirTr(TEXT("The Midnight Express"), TEXT("El expreso de medianoche"))
		: NoirTr(TEXT("Blackwood Manor"), TEXT("Mansión Blackwood"));

	// Rooms: their corners in the world first, to find the extent of the place.
	struct FRaw { TArray<FVector> Corners; FVector Centre; FString Label; bool bScene; };
	TArray<FRaw> Raw;
	WorldMin = FVector2D(FLT_MAX);
	WorldMax = FVector2D(-FLT_MAX);
	for (TActorIterator<ANoirRoom> It(World); It; ++It)
	{
		const FTransform T = It->Bounds->GetComponentTransform();
		const FVector E = It->Bounds->GetUnscaledBoxExtent();
		FRaw& R = Raw.AddDefaulted_GetRef();
		for (const FVector2D S : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1)})
		{
			const FVector C = T.TransformPosition(FVector(S.X * E.X, S.Y * E.Y, 0.f));
			R.Corners.Add(C);
			WorldMin = FVector2D::Min(WorldMin, FVector2D(C));
			WorldMax = FVector2D::Max(WorldMax, FVector2D(C));
		}
		R.Centre = T.GetLocation();
		R.Label = It->Label.ToString();
		R.bScene = It->bPlaysScene;
	}
	if (Raw.IsEmpty()) return;

	// Fit the page; long, thin places (the train) are stretched across, at most 7:1.
	const FVector2D Margin(30.f, 30.f);
	const FVector2D Room = Canvas - Margin * 2 - FVector2D(0, 30); // keep the key clear
	const FVector2D Span = FVector2D::Max(WorldMax - WorldMin, FVector2D(1, 1));
	Scale = FVector2D(Room.X / Span.X, Room.Y / Span.Y);
	Scale.X = FMath::Min(Scale.X, Scale.Y * 7.f);
	Scale.Y = FMath::Min(Scale.Y, Scale.X * 7.f);
	Offset = Margin + (Room - Span * Scale) * 0.5;

	for (const FRaw& R : Raw)
	{
		FRoomMark& M = Rooms.AddDefaulted_GetRef();
		for (const FVector& C : R.Corners) M.Corners.Add(ToMap(C));
		M.Centre = ToMap(R.Centre);
		M.Label = R.Label;
		M.bScene = R.bScene;
	}
	for (TActorIterator<ANoirSearchable> It(World); It; ++It)
	{
		Furniture.Add({ToMap(It->GetActorLocation()), It->IsSearched()});
	}
	for (TActorIterator<ANoirSuspect> It(World); It; ++It)
	{
		Suspects.Add({ToMap(It->GetActorLocation()), It->SignatureColor, It->DisplayName.ToString()});
	}
	FRotator Looking;
	const FTransform Where = Detective->GetResumeTransform(Looking); // outside the Mind Place, if in it
	You = ToMap(Where.GetLocation());
	const FVector Dir = Looking.Vector();
	YouFacing = FVector2D(Dir.X * Scale.X, -Dir.Y * Scale.Y).GetSafeNormal();
	if (YouFacing.IsNearlyZero()) YouFacing = FVector2D(1, 0);
}

FReply SNoirMap::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::M || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (OnClose) OnClose();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Event);
}
