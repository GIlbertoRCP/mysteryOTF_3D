#include "NoirMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NoirCaseContent.h"
#include "NoirPlayerController.h"
#include "NoirSave.h"
#include "NoirSettings.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Cream(0.86f, 0.8f, 0.68f);
	const FLinearColor Amber(1.f, 0.62f, 0.22f);
	const FLinearColor Dim(0.45f, 0.42f, 0.37f);

	const TCHAR* const DifficultyKeys[] = {TEXT("Novice"), TEXT("Detective"), TEXT("Mastermind")};

	FSlateFontInfo Font(const char* Weight, float Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	const FButtonStyle& PlainButton()
	{
		static FButtonStyle Style = []
		{
			FButtonStyle S;
			S.SetNormal(FSlateNoResource());
			S.SetHovered(FSlateColorBrush(FLinearColor(1.f, 0.62f, 0.22f, 0.06f)));
			S.SetPressed(FSlateColorBrush(FLinearColor(1.f, 0.62f, 0.22f, 0.12f)));
			S.SetDisabled(FSlateNoResource());
			S.SetNormalPadding(FMargin(0.f));
			S.SetPressedPadding(FMargin(0.f));
			return S;
		}();
		return Style;
	}

	const FSlateBrush* Shade(float Alpha)
	{
		static TMap<int32, TSharedPtr<FSlateColorBrush>> Brushes;
		const int32 Key = FMath::RoundToInt(Alpha * 100.f);
		TSharedPtr<FSlateColorBrush>& Brush = Brushes.FindOrAdd(Key);
		if (!Brush) Brush = MakeShared<FSlateColorBrush>(FLinearColor(0.004f, 0.003f, 0.002f, Alpha));
		return Brush.Get();
	}

	FText DifficultyName(int32 i)
	{
		switch (i)
		{
		case 0: return NoirTr(TEXT("Novice"), TEXT("Novato"));
		case 2: return NoirTr(TEXT("Mastermind"), TEXT("Cerebro"));
		default: return NoirTr(TEXT("Detective"), TEXT("Detective"));
		}
	}

	FText DifficultyNote(int32 i)
	{
		switch (i)
		{
		case 0: return NoirTr(TEXT("Fewer suspects, no twists. A good first case."), TEXT("Menos sospechosos, sin giros. Un buen primer caso."));
		case 2: return NoirTr(TEXT("Twists, lies and an accomplice who covers for the killer."), TEXT("Giros, mentiras y un cómplice que encubre al asesino."));
		default: return NoirTr(TEXT("Five suspects, one complication, one lie."), TEXT("Cinco sospechosos, una complicación, una mentira."));
		}
	}

	UGameUserSettings* Display() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
}

void SNoirMenu::Construct(const FArguments& InArgs)
{
	bClosed = InArgs._bClosed;
	bPause = InArgs._bPause || bClosed;
	Verdict = InArgs._Verdict;
	Owner = InArgs._Owner;
	if (!bPause)
	{
		NoirSetSpanish(UNoirSettings::Get()->bSpanish);
	}
	for (int32 i = 0; i < UE_ARRAY_COUNT(DifficultyKeys); ++i)
	{
		if (UNoirSettings::Get()->Difficulty == DifficultyKeys[i]) Difficulty = i;
	}
	bTrain = UNoirSettings::Get()->Theme == TEXT("express");
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	ChildSlot
	[
		SNew(SOverlay)
		// The scene stays visible; the menu darkens it from the left.
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(Shade(bPause ? 0.55f : 0.f)).Padding(0.f)
		]
		+ SOverlay::Slot().HAlign(HAlign_Left)
		[
			SAssignNew(Fade, SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(600.f)
				[
					SNew(SBorder).BorderImage(Shade(0.72f)).Padding(FMargin(90.f, 0.f, 30.f, 0.f)).VAlign(VAlign_Center)
					[
						SAssignNew(Pages, SWidgetSwitcher)
						+ SWidgetSwitcher::Slot()[MakeMainPage()]
						+ SWidgetSwitcher::Slot()[MakeCasePage()]
						+ SWidgetSwitcher::Slot()[MakeSettingsPage()]
					]
				]
			]
		]
	];
	// The panel's edge fades into the scene instead of stopping.
	constexpr int32 Steps = 16;
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float Alpha = 0.72f * FMath::Square(1.f - float(i) / (Steps + 1));
		Fade->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(14.f)[SNew(SBorder).BorderImage(Shade(Alpha)).Padding(0.f)]];
	}
}

TSharedRef<SWidget> SNoirMenu::MakeHeading(TAttribute<FText> Text)
{
	return SNew(STextBlock).Text(Text).Font(Font("Light", 15, 400)).ColorAndOpacity(Amber);
}

TSharedRef<SWidget> SNoirMenu::MakeButton(TAttribute<FText> Label, TFunction<void()> OnClick, TSharedPtr<SButton>* OutButton, float Size)
{
	TSharedRef<TWeakPtr<SButton>> Self = MakeShared<TWeakPtr<SButton>>();
	TSharedRef<SButton> Button = SNew(SButton)
		.ButtonStyle(&PlainButton())
		.ContentPadding(FMargin(14.f, 7.f))
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Font("Light", Size, 60))
			.ColorAndOpacity_Lambda([Self]()
			{
				const TSharedPtr<SButton> B = Self->Pin();
				return FSlateColor(B && (B->IsHovered() || B->HasKeyboardFocus()) ? Amber : Cream);
			})
		];
	*Self = Button;
	if (OutButton) *OutButton = Button;
	return Button;
}

TSharedRef<SWidget> SNoirMenu::MakeChoice(TAttribute<FText> Label, TAttribute<FText> Value, TFunction<void(int32)> Step, TSharedPtr<SButton>* OutButton)
{
	auto Arrow = [this, Step](const TCHAR* Glyph, int32 Dir, TSharedPtr<SButton>* Out)
	{
		return MakeButton(FText::FromString(Glyph), [Step, Dir]() { Step(Dir); }, Out, 18.f);
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(Label).Font(Font("Regular", 15, 40)).ColorAndOpacity(Dim)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT("<"), -1, OutButton)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(190.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Value).Font(Font("Regular", 17, 20)).ColorAndOpacity(Cream)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT(">"), 1, nullptr)];
}

TSharedRef<SWidget> SNoirMenu::MakeMainPage()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](TSharedRef<SWidget> W, float Top = 0.f) { Box->AddSlot().AutoHeight().Padding(0.f, Top, 0.f, 0.f)[W]; };

	if (bClosed)
	{
		Add(MakeHeading(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("CASE CLOSED"), TEXT("CASO CERRADO")); })));
		Add(SNew(STextBlock).Text_Lambda([this]() { return Owner.IsValid() ? Owner->GetCaseTitle() : FText(); })
			.Font(Font("Light", 30, 20)).ColorAndOpacity(Cream).AutoWrapText(true), 12.f);
		Add(SNew(STextBlock).Text(Verdict).Font(Font("Regular", 16, 40)).ColorAndOpacity(Amber).AutoWrapText(true), 10.f);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("A new case"), TEXT("Un caso nuevo")); }),
			[this]() { if (Owner.IsValid()) Owner->StartCase(FString(), UNoirSettings::Get()->Difficulty, NoirIsSpanish(), Owner->GetCaseTheme()); }, &FirstOnPage[0], 26.f), 48.f);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("This case again"), TEXT("Este caso otra vez")); }), [this]() { if (Owner.IsValid()) Owner->RestartCase(); }));
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Keep looking around"), TEXT("Seguir mirando")); }), [this]() { if (Owner.IsValid()) Owner->CloseMenu(); }));
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Title screen"), TEXT("Pantalla de título")); }), [this]() { if (Owner.IsValid()) Owner->QuitToMainMenu(); }));
		return Box;
	}
	if (bPause)
	{
		Add(MakeHeading(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("PAUSED"), TEXT("EN PAUSA")); })));
		Add(SNew(STextBlock).Text_Lambda([this]() { return Owner.IsValid() ? Owner->GetCaseTitle() : FText(); })
			.Font(Font("Light", 30, 20)).ColorAndOpacity(Cream).AutoWrapText(true), 12.f);
		Add(SNew(STextBlock).Text_Lambda([this]()
			{
				return FText::Format(NoirTr(TEXT("Case seed  {0}"), TEXT("Semilla del caso  {0}")), FText::FromString(Owner.IsValid() ? Owner->GetCaseSeed() : FString()));
			})
			.Font(Font("Regular", 13, 80)).ColorAndOpacity(Dim), 8.f);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Resume"), TEXT("Continuar")); }), [this]() { if (Owner.IsValid()) Owner->CloseMenu(); }, &FirstOnPage[0]), 48.f);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Settings"), TEXT("Ajustes")); }), [this]() { ShowPage(TEXT("settings")); }));
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Restart this case"), TEXT("Reiniciar este caso")); }), [this]() { if (Owner.IsValid()) Owner->RestartCase(); }));
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Abandon the case"), TEXT("Abandonar el caso")); }), [this]() { if (Owner.IsValid()) Owner->QuitToMainMenu(); }));
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Quit to desktop"), TEXT("Salir al escritorio")); }), [this]() { if (Owner.IsValid()) Owner->QuitGame(); }));
		return Box;
	}

	Add(SNew(STextBlock).Text(FText::FromString(TEXT("NOIR"))).Font(Font("Light", 84, 600)).ColorAndOpacity(Cream));
	Add(SNew(STextBlock).Text_Lambda([]() { return NoirTr(TEXT("A MURDER MYSTERY"), TEXT("UN MISTERIO DE ASESINATO")); })
		.Font(Font("Light", 16, 700)).ColorAndOpacity(Amber), 2.f);
	// A case left halfway comes first.
	if (const UNoirSaveGame* Save = UNoirSaveGame::Exists() ? UNoirSaveGame::Load() : nullptr)
	{
		const FText Title = FText::FromString(Save->CaseTitle);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Continue"), TEXT("Continuar")); }),
			[this]() { if (Owner.IsValid()) Owner->ContinueCase(); }, &FirstOnPage[0], 26.f), 70.f);
		Add(SNew(SBox).Padding(FMargin(14.f, 0.f, 0.f, 6.f))
			[
				SNew(STextBlock).Text(Title).Font(Font("Italic", 13)).ColorAndOpacity(Dim)
			]);
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("New case"), TEXT("Nuevo caso")); }), [this]() { ShowPage(TEXT("case")); }, nullptr, 26.f));
	}
	else
	{
		Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("New case"), TEXT("Nuevo caso")); }), [this]() { ShowPage(TEXT("case")); }, &FirstOnPage[0], 26.f), 70.f);
	}
	Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Settings"), TEXT("Ajustes")); }), [this]() { ShowPage(TEXT("settings")); }, nullptr, 26.f));
	Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Quit"), TEXT("Salir")); }), [this]() { if (Owner.IsValid()) Owner->QuitGame(); }, nullptr, 26.f));
	return Box;
}

TSharedRef<SWidget> SNoirMenu::MakeCasePage()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](TSharedRef<SWidget> W, float Top = 0.f) { Box->AddSlot().AutoHeight().Padding(0.f, Top, 0.f, 0.f)[W]; };

	Add(MakeHeading(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("NEW CASE"), TEXT("NUEVO CASO")); })));
	Add(SNew(STextBlock).Text_Lambda([this]()
		{
			return bTrain ? NoirTr(TEXT("The Midnight Express"), TEXT("El expreso de medianoche")) : NoirTr(TEXT("Blackwood Manor"), TEXT("Mansión Blackwood"));
		})
		.Font(Font("Light", 34, 20)).ColorAndOpacity(Cream), 12.f);
	Add(SNew(STextBlock).Text_Lambda([this]()
		{
			return bTrain
				? NoirTr(TEXT("Six carriages stopped dead in a mountain pass, snowed in until morning."),
					TEXT("Seis vagones detenidos en un paso de montaña, atrapados por la nieve hasta el amanecer."))
				: NoirTr(TEXT("A Gothic manor, one autumn night, fog at the windows."),
					TEXT("Una mansión gótica, una noche de otoño, niebla en las ventanas."));
		})
		.Font(Font("Regular", 13)).ColorAndOpacity(Dim).AutoWrapText(true), 6.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Location"), TEXT("Lugar")); }),
		TAttribute<FText>::CreateLambda([this]() { return bTrain ? NoirTr(TEXT("The train"), TEXT("El tren")) : NoirTr(TEXT("The manor"), TEXT("La mansión")); }),
		[this](int32) { bTrain = !bTrain; }, &FirstOnPage[1]), 30.f);
	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Difficulty"), TEXT("Dificultad")); }),
		TAttribute<FText>::CreateLambda([this]() { return DifficultyName(Difficulty); }),
		[this](int32 Dir) { Difficulty = (Difficulty + Dir + 3) % 3; }), 12.f);
	Add(SNew(SBox).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text_Lambda([this]() { return DifficultyNote(Difficulty); }).Font(Font("Italic", 12)).ColorAndOpacity(Dim).AutoWrapText(true)
		], 2.f);
	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Language"), TEXT("Idioma")); }),
		TAttribute<FText>::CreateLambda([]() { return FText::FromString(NoirIsSpanish() ? TEXT("Español") : TEXT("English")); }),
		[](int32) { NoirSetSpanish(!NoirIsSpanish()); }), 12.f);

	// The seed: the same seed always gives the same case (also in the web game).
	Add(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text_Lambda([]() { return NoirTr(TEXT("Seed"), TEXT("Semilla")); }).Font(Font("Regular", 15, 40)).ColorAndOpacity(Dim)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(250.f)
			[
				SAssignNew(SeedBox, SEditableTextBox)
				.HintText_Lambda([]() { return NoirTr(TEXT("random"), TEXT("aleatoria")); })
				.Font(Font("Regular", 15, 60))
				.BackgroundColor(FLinearColor(0.02f, 0.016f, 0.012f, 0.9f))
				.ForegroundColor(Cream)
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit) { if (Commit == ETextCommit::OnEnter) Begin(); })
			]
		], 12.f);
	Add(SNew(SBox).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text_Lambda([]() { return NoirTr(TEXT("Share a seed and a friend gets the same case."), TEXT("Comparte una semilla y otra persona tendrá el mismo caso.")); })
			.Font(Font("Italic", 12)).ColorAndOpacity(Dim)
		], 4.f);

	Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Begin the case"), TEXT("Comenzar el caso")); }), [this]() { Begin(); }, nullptr, 26.f), 40.f);
	Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Back"), TEXT("Volver")); }), [this]() { Back(); }, nullptr, 18.f), 4.f);
	return Box;
}

TSharedRef<SWidget> SNoirMenu::MakeSettingsPage()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](TSharedRef<SWidget> W, float Top = 0.f) { Box->AddSlot().AutoHeight().Padding(0.f, Top, 0.f, 0.f)[W]; };
	auto Apply = []()
	{
		if (UGameUserSettings* S = Display())
		{
			S->ApplySettings(false);
			S->SaveSettings();
		}
	};

	Add(MakeHeading(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("SETTINGS"), TEXT("AJUSTES")); })));

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Language"), TEXT("Idioma")); }),
		TAttribute<FText>::CreateLambda([]() { return FText::FromString(NoirIsSpanish() ? TEXT("Español") : TEXT("English")); }),
		[this](int32)
		{
			// During a case the case keeps its language; this sets the next one and the menus.
			NoirSetSpanish(!NoirIsSpanish());
			UNoirSettings::Get()->bSpanish = NoirIsSpanish();
			UNoirSettings::Get()->Save();
		}, &FirstOnPage[2]), 24.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Display"), TEXT("Pantalla")); }),
		TAttribute<FText>::CreateLambda([]()
		{
			const UGameUserSettings* S = Display();
			switch (S ? S->GetFullscreenMode() : EWindowMode::Windowed)
			{
			case EWindowMode::Fullscreen: return NoirTr(TEXT("Fullscreen"), TEXT("Pantalla completa"));
			case EWindowMode::WindowedFullscreen: return NoirTr(TEXT("Borderless"), TEXT("Sin bordes"));
			default: return NoirTr(TEXT("Windowed"), TEXT("Ventana"));
			}
		}),
		[Apply](int32 Dir)
		{
			if (UGameUserSettings* S = Display())
			{
				const EWindowMode::Type Modes[] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
				int32 i = 0;
				while (i < 3 && Modes[i] != S->GetFullscreenMode()) ++i;
				S->SetFullscreenMode(Modes[((i % 3) + Dir + 3) % 3]);
				Apply();
			}
		}), 8.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Resolution"), TEXT("Resolución")); }),
		TAttribute<FText>::CreateLambda([]()
		{
			const UGameUserSettings* S = Display();
			const FIntPoint R = S ? S->GetScreenResolution() : FIntPoint::ZeroValue;
			return FText::FromString(FString::Printf(TEXT("%d x %d"), R.X, R.Y));
		}),
		[this, Apply](int32 Dir)
		{
			UGameUserSettings* S = Display();
			if (!S || Resolutions.IsEmpty()) return;
			int32 i = Resolutions.IndexOfByKey(S->GetScreenResolution());
			i = i == INDEX_NONE ? Resolutions.Num() - 1 : (i + Dir + Resolutions.Num()) % Resolutions.Num();
			S->SetScreenResolution(Resolutions[i]);
			Apply();
		}), 8.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Quality"), TEXT("Calidad")); }),
		TAttribute<FText>::CreateLambda([]()
		{
			const UGameUserSettings* S = Display();
			switch (S ? S->GetOverallScalabilityLevel() : -1)
			{
			case 0: return NoirTr(TEXT("Low"), TEXT("Baja"));
			case 1: return NoirTr(TEXT("Medium"), TEXT("Media"));
			case 2: return NoirTr(TEXT("High"), TEXT("Alta"));
			case 3: return NoirTr(TEXT("Epic"), TEXT("Épica"));
			case 4: return NoirTr(TEXT("Cinematic"), TEXT("Cinemática"));
			default: return NoirTr(TEXT("Custom"), TEXT("Personalizada"));
			}
		}),
		[Apply](int32 Dir)
		{
			if (UGameUserSettings* S = Display())
			{
				const int32 Level = S->GetOverallScalabilityLevel();
				S->SetOverallScalabilityLevel(Level < 0 ? 3 : FMath::Clamp(Level + Dir, 0, 4));
				Apply();
			}
		}), 8.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Look sensitivity"), TEXT("Sensibilidad")); }),
		TAttribute<FText>::CreateLambda([]() { return FText::FromString(FString::Printf(TEXT("%.2f"), UNoirSettings::Get()->LookSensitivity)); }),
		[](int32 Dir)
		{
			UNoirSettings* S = UNoirSettings::Get();
			S->LookSensitivity = FMath::Clamp(S->LookSensitivity + Dir * 0.25f, 0.25f, 3.f);
			S->Save();
		}), 8.f);

	Add(MakeChoice(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Invert look"), TEXT("Invertir vista")); }),
		TAttribute<FText>::CreateLambda([]() { return UNoirSettings::Get()->bInvertLook ? NoirTr(TEXT("On"), TEXT("Sí")) : NoirTr(TEXT("Off"), TEXT("No")); }),
		[](int32)
		{
			UNoirSettings* S = UNoirSettings::Get();
			S->bInvertLook = !S->bInvertLook;
			S->Save();
		}), 8.f);

	Add(MakeButton(TAttribute<FText>::CreateLambda([]() { return NoirTr(TEXT("Back"), TEXT("Volver")); }), [this]() { Back(); }, nullptr, 18.f), 36.f);
	return Box;
}

void SNoirMenu::ShowPage(const FString& Name)
{
	Page = Name == TEXT("case") && !bPause ? 1 : Name == TEXT("settings") ? 2 : 0;
	Pages->SetActiveWidgetIndex(Page);
	FocusFirst();
}

void SNoirMenu::FocusFirst()
{
	if (FirstOnPage[Page].IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(FirstOnPage[Page], EFocusCause::SetDirectly);
	}
}

void SNoirMenu::Back()
{
	if (Page != 0)
	{
		ShowPage(TEXT("main"));
	}
	else if (bPause && Owner.IsValid())
	{
		Owner->CloseMenu();
	}
}

void SNoirMenu::Begin()
{
	UNoirSettings* S = UNoirSettings::Get();
	S->Difficulty = DifficultyKeys[Difficulty];
	S->bSpanish = NoirIsSpanish();
	S->Theme = bTrain ? TEXT("express") : TEXT("gothic");
	S->Save();
	if (Owner.IsValid())
	{
		Owner->StartCase(SeedBox.IsValid() ? SeedBox->GetText().ToString() : FString(), S->Difficulty, S->bSpanish, S->Theme);
	}
}

FReply SNoirMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		Back();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}
