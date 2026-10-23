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

namespace NoirMapLocal
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
using namespace NoirMapLocal;

void SNoirMap::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;
	Detective = InArgs._Detective;
	// Start on the house indoors, on the grounds outside.
	if (ANoirCharacter* D = Detective.Get())
	{
		bool bIndoors = false;
		for (TActorIterator<ANoirRoom> It(D->GetWorld()); It; ++It)
		{
			bHasGrounds |= It->bOutdoors;
			// In the house proper; an outbuilding is out on the grounds.
			bIndoors |= !It->bOutdoors && !It->bOnlySuits && It->Contains(D->GetActorLocation());
		}
		bGrounds = bHasGrounds && !bIndoors;
	}
	Gather(Detective.Get());

	SAssignNew(Sketch, SNoirSketch).Size(Canvas);
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
			// Halls and corridors in a lighter hand, under the rooms.
			Lines(G, Out, R.bCast ? L + 1 : L, Outline, R.bCast ? Ink : FadedInk, R.bCast ? 2.5f : 1.5f);
		}
		if (!bGrounds)
		{
			for (const FThingMark& F : Furniture) Square(G, Out, L + 2, F.At, 9.f, F.bSearched);
		}
		else if (HouseLo.X < HouseHi.X)
		{
			// At this scale the house is one block, named once.
			Write(G, Out, L + 3, NoirTrS(TEXT("THE HOUSE"), TEXT("LA CASA")), FVector2D((HouseLo.X + HouseHi.X) * 0.5, HouseLo.Y - 10), Mono(10), Ink);
		}
		for (const FRoomMark& R : Rooms)
		{
			const FSlateFontInfo Font = Mono(11);
			if (bGrounds && !R.bOutdoors && !R.bOutbuilding) continue; // the house's rooms: outlines only
			if (!R.bCast)
			{
				Write(G, Out, L + 3, R.Label.ToUpper(), R.Centre, Mono(10), FadedInk);
				continue;
			}
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
			if (bGrounds) continue; // names would bury the house at this scale
			if (!P.LabelAt.Equals(P.At + FVector2D(10, 0))) Lines(G, Out, L + 4, {P.At + FVector2D(6, 0), P.LabelAt - FVector2D(2, 0)}, FadedInk, 1.f);
			Write(G, Out, L + 4, P.Name, P.LabelAt, Mono(9), Ink, false);
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
						SNew(STextBlock).Text_Lambda([this]()
						{
							return bGrounds ? FText::Format(NoirTr(TEXT("{0}: the grounds"), TEXT("{0}: los terrenos")), Place) : Place;
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Light", 28)).ColorAndOpacity(Ink)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 24, 0)
					[
						SNew(STextBlock).Visibility(bHasGrounds ? EVisibility::Visible : EVisibility::Collapsed)
						.Text_Lambda([this]()
						{
							return bGrounds ? NoirTr(TEXT("G   the house"), TEXT("G   la casa")) : NoirTr(TEXT("G   the grounds"), TEXT("G   los terrenos"));
						})
						.Font(Mono(11)).ColorAndOpacity(Ink)
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
				+ SVerticalBox::Slot().AutoHeight()[Sketch.ToSharedRef()]
			]
		]
	];
}

FVector2D SNoirMap::ToMap(const FVector& World) const
{
	// North (+Y) up the page.
	return Offset + FVector2D((World.X - WorldMin.X) * Scale.X, (WorldMax.Y - World.Y) * Scale.Y);
}

bool SNoirMap::OnPage(const FVector2D& P) const
{
	return P.X >= 0 && P.Y >= 0 && P.X <= Canvas.X && P.Y <= Canvas.Y - 30.f;
}

void SNoirMap::Gather(ANoirCharacter* InDetective)
{
	Rooms.Reset();
	Furniture.Reset();
	Suspects.Reset();
	HouseLo = FVector2D(FLT_MAX);
	HouseHi = FVector2D(-FLT_MAX);
	UWorld* World = InDetective ? InDetective->GetWorld() : nullptr;
	if (!World) return;
	const ANoirCaseDirector* Director = nullptr;
	for (TActorIterator<ANoirCaseDirector> It(World); It; ++It) Director = *It;
	Place = Director && Director->Theme == TEXT("express") ? NoirTr(TEXT("The Midnight Express"), TEXT("El expreso de medianoche"))
		: NoirTr(TEXT("Blackwood Manor"), TEXT("Mansión Blackwood"));

	// Rooms: their corners in the world first, to find the extent of the place.
	struct FRaw { TArray<FVector> Corners; FVector Centre; FString Label; bool bScene; bool bCast; bool bOutdoors; bool bOutbuilding; };
	TArray<FRaw> Raw;
	WorldMin = FVector2D(FLT_MAX);
	WorldMax = FVector2D(-FLT_MAX);
	for (TActorIterator<ANoirRoom> It(World); It; ++It)
	{
		if ((It->bOutdoors || It->bOnlySuits) && !bGrounds) continue; // the house view is the house alone
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
		R.Label = It->GetDisplayLabel().ToString();
		R.bScene = It->bPlaysScene;
		R.bCast = It->bCast;
		R.bOutdoors = It->bOutdoors;
		R.bOutbuilding = It->bOnlySuits;
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
		M.bCast = R.bCast;
		M.bOutdoors = R.bOutdoors;
		M.bOutbuilding = R.bOutbuilding;
		if (!R.bOutdoors && !R.bOutbuilding)
		{
			for (const FVector2D& C : M.Corners) { HouseLo = FVector2D::Min(HouseLo, C); HouseHi = FVector2D::Max(HouseHi, C); }
		}
	}
	for (TActorIterator<ANoirSearchable> It(World); It; ++It)
	{
		const FVector2D At = ToMap(It->GetActorLocation());
		if (OnPage(At)) Furniture.Add({At, It->IsSearched()});
	}
	for (TActorIterator<ANoirSuspect> It(World); It; ++It)
	{
		const FVector2D At = ToMap(It->GetActorLocation());
		if (OnPage(At)) Suspects.Add({At, It->SignatureColor, It->DisplayName.ToString(), At + FVector2D(10, 0)});
	}
	// Names that would print over each other step down, top to bottom.
	Suspects.Sort([](const FPersonMark& A, const FPersonMark& B) { return A.At.Y < B.At.Y; });
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Mono", 9);
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (int32 i = 0; i < Suspects.Num(); ++i)
	{
		const float W = Measure->Measure(Suspects[i].Name, LabelFont).X;
		for (int32 Pass = 0; Pass < Suspects.Num(); ++Pass)
		{
			bool bMoved = false;
			for (int32 j = 0; j < i; ++j)
			{
				const float WJ = Measure->Measure(Suspects[j].Name, LabelFont).X;
				const FVector2D A = Suspects[i].LabelAt, B = Suspects[j].LabelAt;
				if (FMath::Abs(A.Y - B.Y) < 15.f && A.X < B.X + WJ + 6.f && B.X < A.X + W + 6.f)
				{
					Suspects[i].LabelAt.Y = B.Y + 15.f;
					bMoved = true;
				}
			}
			if (!bMoved) break;
		}
	}
	FRotator Looking;
	const FTransform Where = InDetective->GetResumeTransform(Looking); // outside the Mind Place, if in it
	// Off the page (out in the grounds, on the house view): pinned to its edge, still pointing.
	You = ToMap(Where.GetLocation());
	You = FVector2D(FMath::Clamp(You.X, 8.0, Canvas.X - 8.0), FMath::Clamp(You.Y, 8.0, Canvas.Y - 38.0));
	const FVector Dir = Looking.Vector();
	YouFacing = FVector2D(Dir.X * Scale.X, -Dir.Y * Scale.Y).GetSafeNormal();
	if (YouFacing.IsNearlyZero()) YouFacing = FVector2D(1, 0);
}

void SNoirMap::SetGrounds(bool bOn)
{
	bGrounds = bOn && bHasGrounds;
	Gather(Detective.Get());
	if (Sketch) Sketch->Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SNoirMap::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::M || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (OnClose) OnClose();
		return FReply::Handled();
	}
	if (bHasGrounds && (Key == EKeys::G || Key == EKeys::Gamepad_FaceButton_Top))
	{
		SetGrounds(!bGrounds);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, Event);
}
