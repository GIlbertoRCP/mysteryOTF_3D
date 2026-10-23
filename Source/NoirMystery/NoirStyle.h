#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

/**
 * The game's one look for text and panels: the menus, the HUD, the puzzles and the case file all
 * draw from here, so they read as the same game. Warm paper tones on near-black; amber for what
 * matters now; oxblood for danger. Roboto throughout (Light for titles, Regular for reading,
 * letter-spaced capitals for labels).
 */
namespace NoirStyle
{
	inline const FLinearColor Cream(0.88f, 0.82f, 0.70f);
	inline const FLinearColor Amber(1.f, 0.63f, 0.24f);
	/** Secondary text: still readable on a dark panel (the old dim tone was too faint). */
	inline const FLinearColor Dim(0.62f, 0.57f, 0.49f);
	inline const FLinearColor Faint(0.42f, 0.39f, 0.34f);
	inline const FLinearColor Oxblood(0.78f, 0.16f, 0.12f);
	inline const FLinearColor Good(0.52f, 0.78f, 0.55f);
	/** Panels: near-black, a touch warm, mostly opaque so text never sits on a bright wall. */
	inline const FLinearColor Panel(0.012f, 0.009f, 0.007f, 0.78f);
	inline const FLinearColor PanelEdge(0.55f, 0.40f, 0.20f, 0.55f);
	inline const FLinearColor KeyCap(0.06f, 0.045f, 0.03f, 0.95f);

	// Type sizes (points at 1080p; Slate and the HUD both scale them with the screen).
	inline constexpr float Caption = 13.f;
	inline constexpr float Body = 16.f;
	inline constexpr float Lead = 20.f;
	inline constexpr float Heading = 26.f;
	inline constexpr float Title = 34.f;

	inline FSlateFontInfo Font(const char* Weight, float Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	/** Spaced capitals for small labels ("NEW EVIDENCE", "COMPOSURE"). */
	inline FSlateFontInfo Label(float Size = Caption) { return Font("Regular", Size, 160); }
}
