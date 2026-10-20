#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

/** A small canvas the puzzles draw on: Paint draws it, OnMove hears the mouse over it. */
class SNoirSketch : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SNoirSketch) : _Size(FVector2D(320.f, 220.f)) {}
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Size = InArgs._Size; }

	TFunction<void(const FGeometry&, FSlateWindowElementList&, int32)> Paint;
	TFunction<void(const FVector2D&)> OnMove;

	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (Paint) Paint(Geometry, Out, LayerId);
		return LayerId + 1;
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!OnMove) return FReply::Unhandled();
		OnMove(FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
		return FReply::Handled();
	}

private:
	FVector2D Size;
};

/** Drawing on a sketch: centres and sizes in the sketch's own units (puzzles, the map). */
namespace NoirDraw
{


inline void Box(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Centre, const FVector2D& Size,
	const FLinearColor& Color)
{
	static const FSlateColorBrush White(FLinearColor::White);
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Centre - Size * 0.5))),
		&White, ESlateDrawEffect::None, Color);
}

/** An ellipse (rounded box) filled, or its outline. */
inline void Oval(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Centre, const FVector2D& Size,
	const FLinearColor& Color, bool bOutline = false)
{
	if (bOutline)
	{
		// A traced ellipse: rounded-box brushes paint their inside with the tint as well.
		TArray<FVector2f> P;
		for (int32 i = 0; i <= 48; ++i)
		{
			const float A = 2.f * PI * i / 48.f;
			P.Add(FVector2f(Centre + FVector2D(FMath::Cos(A) * Size.X * 0.5, FMath::Sin(A) * Size.Y * 0.5)));
		}
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, 2.5f);
		return;
	}
	// The corner radius has to fit the shape, so brushes are kept per radius.
	static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Brushes;
	const int32 Radius = FMath::Max(1, FMath::RoundToInt(FMath::Min(Size.X, Size.Y) * 0.5f));
	TSharedPtr<FSlateRoundedBoxBrush>& Brush = Brushes.FindOrAdd(bOutline ? -Radius : Radius);
	if (!Brush)
	{
		Brush = bOutline ? MakeShared<FSlateRoundedBoxBrush>(FLinearColor::Transparent, float(Radius), FLinearColor::White, 3.f)
		                 : MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, float(Radius));
	}
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Centre - Size * 0.5))),
		Brush.Get(), ESlateDrawEffect::None, Color);
}

inline void Lines(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const TArray<FVector2D>& Points, const FLinearColor& Color,
	float Thickness)
{
	TArray<FVector2f> P;
	for (const FVector2D& V : Points) P.Add(FVector2f(V));
	FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, Thickness);
}
}
