#include "NoirCaseNotebook.h"
#include "NoirStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "NoirCase.h"
#include "NoirCaseContent.h"
#include "NoirCaseDirector.h"
#include "NoirCharacter.h"
#include "NoirEvidence.h"
#include "NoirPlayerController.h"
#include "NoirSuspect.h"
#include "NoirRoom.h"

namespace NoirNotebookLocal
{
	// ─── Knives Out / Alan Wake 2 / Silent Hill Aesthetic Palette ───
	// Atmospheric noir immersion: heavy obsidian folio backing, aged brass accents,
	// elevated card plates, candlelit amber, typewriter oxblood ink, and warm parchment cream.
	const FLinearColor Vignette(0.002f, 0.0015f, 0.0015f, 0.96f);
	const FLinearColor FolioOuter(0.010f, 0.008f, 0.006f, 0.99f);
	const FLinearColor FolioSheet(0.016f, 0.013f, 0.010f, 0.99f);
	const FLinearColor CardBg(0.024f, 0.020f, 0.016f, 0.95f);
	const FLinearColor CardBorder(0.24f, 0.18f, 0.11f, 0.50f);
	const FLinearColor HairlineBrass(0.50f, 0.38f, 0.20f, 0.70f);
	const FLinearColor HairlineRule(0.28f, 0.22f, 0.14f, 0.40f);
	const FLinearColor Amber = NoirStyle::Amber;
	const FLinearColor Gold(0.88f, 0.72f, 0.38f);
	const FLinearColor Cream = NoirStyle::Cream;
	const FLinearColor Dim = NoirStyle::Dim;
	const FLinearColor DimMuted(0.38f, 0.34f, 0.28f);
	const FLinearColor Oxblood(0.72f, 0.13f, 0.11f);
	const FLinearColor OxbloodBg(0.08f, 0.02f, 0.02f, 0.95f);
	const FLinearColor AmberMemoBg(0.038f, 0.026f, 0.014f, 0.92f);

	FSlateFontInfo Font(const char* Weight, float Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo Mono(float Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("Mono", Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	const FSlateBrush* SolidBrush(const FLinearColor& Color)
	{
		static TMap<uint32, TSharedPtr<FSlateColorBrush>> Brushes;
		const uint32 Key = GetTypeHash(Color.ToFColor(false));
		TSharedPtr<FSlateColorBrush>& B = Brushes.FindOrAdd(Key);
		if (!B) B = MakeShared<FSlateColorBrush>(Color);
		return B.Get();
	}

	const FButtonStyle& PlainButton()
	{
		static FButtonStyle Style = []
		{
			FButtonStyle S;
			S.SetNormal(FSlateNoResource());
			S.SetHovered(FSlateColorBrush(FLinearColor(1.f, 0.64f, 0.22f, 0.08f)));
			S.SetPressed(FSlateColorBrush(FLinearColor(1.f, 0.64f, 0.22f, 0.16f)));
			S.SetDisabled(FSlateNoResource());
			S.SetNormalPadding(FMargin(0.f));
			S.SetPressedPadding(FMargin(0.f));
			return S;
		}();
		return Style;
	}

	// Archival pill badge for metadata tags (clean, robust against clipping)
	TSharedRef<SWidget> MakeTagBadge(const FText& Label, const FText& Value, const FLinearColor& AccentColor)
	{
		return SNew(SBorder)
			.BorderImage(SolidBrush(CardBorder))
			.Padding(FMargin(1.f))
			[
				SNew(SBorder)
				.BorderImage(SolidBrush(CardBg))
				.Padding(FMargin(8.f, 3.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)
					[
						SNew(STextBlock).Text(Label).Font(Mono(8.5f, 60)).ColorAndOpacity(Dim)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Value).Font(Mono(9.f, 60)).ColorAndOpacity(AccentColor)
					]
				]
			];
	}

	// Keycap action prompt for the bottom bar
	TSharedRef<SWidget> MakeKeycapPrompt(const FText& KeyName, const FText& ActionDesc, const FLinearColor& KeyColor = Amber)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(SolidBrush(HairlineRule))
				.Padding(FMargin(1.f))
				[
					SNew(SBorder)
					.BorderImage(SolidBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.95f)))
					.Padding(FMargin(7.f, 2.5f))
					[
						SNew(STextBlock).Text(KeyName).Font(Mono(10.f, 80)).ColorAndOpacity(KeyColor)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(ActionDesc).Font(Mono(10.5f)).ColorAndOpacity(Cream)
			];
	}
}
using namespace NoirNotebookLocal;

void SNoirCaseNotebook::Construct(const FArguments& InArgs)
{
	Detective = InArgs._Detective;
	Owner = InArgs._Owner;
	OnClose = InArgs._OnClose;

	ChildSlot
	[
		SNew(SOverlay)
		// Cinematic dark vignette obscuring the 3D world
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(SolidBrush(Vignette))
			.Padding(0.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
		]
		// Centered Homicide Case File Folio (Spacious 1360px dossier, perfectly scaled for 1080p)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(1360.f)
			.MaxDesiredHeight(890.f)
			.Padding(FMargin(24.f))
			[
				// Outer Archival Folio Casing with Double Brass Hairline
				SNew(SBorder)
				.BorderImage(SolidBrush(HairlineBrass))
				.Padding(FMargin(1.f))
				[
					SNew(SBorder)
					.BorderImage(SolidBrush(FolioOuter))
					.Padding(FMargin(2.f))
					[
						SNew(SBorder)
						.BorderImage(SolidBrush(HairlineRule))
						.Padding(FMargin(1.f))
						[
							// Deep obsidian archival document sheet
							SNew(SBorder)
							.BorderImage(SolidBrush(FolioSheet))
							.Padding(FMargin(36.f, 24.f, 36.f, 20.f))
							[
								SAssignNew(ContentContainer, SBox)
								[
									MakeOverviewContent()
								]
							]
						]
					]
				]
			]
		]
	];
}

void SNoirCaseNotebook::Refresh()
{
	if (ContentContainer.IsValid())
	{
		ContentContainer->SetContent(MakeOverviewContent());
	}
}

TSharedRef<SWidget> SNoirCaseNotebook::MakeOverviewContent()
{
	ANoirCharacter* Det = Detective.Get();
	ANoirCaseDirector* Director = Det ? Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(Det, ANoirCaseDirector::StaticClass())) : nullptr;
	const ANoirCase* Board = Director ? Director->Board.Get() : (Det ? Det->GetCase() : nullptr);

	// The case's own facts (a generated case); a hand-made case falls back on the board and notes.
	const FNoirCaseSummary Summary = Director ? Director->GetCaseSummary() : FNoirCaseSummary();
	FText Title = !Summary.Title.IsEmpty() ? Summary.Title : (Board ? Board->Title : FText());
	if (Title.IsEmpty())
	{
		Title = NoirTr(TEXT("The Open Investigation"), TEXT("La investigación abierta"));
	}

	FText VictimName = !Summary.VictimName.IsEmpty() ? Summary.VictimName : (Board ? Board->VictimName : FText());
	FText VictimLine;
	if (!Summary.VictimOccupation.IsEmpty())
	{
		VictimLine = Summary.VictimAge > 0
			? FText::Format(NoirTr(TEXT("{0}, {1}"), TEXT("{0}, {1} años")), Summary.VictimOccupation, Summary.VictimAge)
			: Summary.VictimOccupation;
	}

	FText CrimeScene = Summary.Scene;
	if (CrimeScene.IsEmpty() && Det && Det->GetWorld())
	{
		for (TActorIterator<ANoirRoom> It(Det->GetWorld()); It; ++It)
		{
			if (It->bPlaysScene)
			{
				CrimeScene = It->Label;
				break;
			}
		}
	}
	const FText Unknown = NoirTr(TEXT("Unknown"), TEXT("Desconocido"));
	if (CrimeScene.IsEmpty()) CrimeScene = Unknown;
	const FText TimeOfDeath = Summary.TimeWindow.IsEmpty() ? Unknown : Summary.TimeWindow;

	FText SynopsisText = Summary.Synopsis;
	if (SynopsisText.IsEmpty() && Det && Det->GetClues().Num() > 0)
	{
		SynopsisText = Det->GetClues()[0];
	}

	// Tonight's complication (a blackout, a storm), if the case has one.
	const FText CompTitle = FText::Format(INVTEXT("✦  {0}"), FText::FromString(Summary.ComplicationLabel.ToString().ToUpper()));
	const FText CleanCompNote = Summary.ComplicationNote;
	const int32 SuspectCount = Summary.SuspectCount > 0 ? Summary.SuspectCount : (Board ? Board->Suspects.Num() : 0);
	const FText Place = Summary.Place.IsEmpty() ? NoirTr(TEXT("Blackwood Manor"), TEXT("Mansión Blackwood")) : Summary.Place;

	// Suspects list (Knives Out Cast)
	TArray<ANoirSuspect*> SuspectsList;
	if (Board && Board->Suspects.Num() > 0)
	{
		for (ANoirSuspect* S : Board->Suspects) if (S) SuspectsList.Add(S);
	}
	else if (Director && Director->GetSuspects().Num() > 0)
	{
		for (ANoirSuspect* S : Director->GetSuspects()) if (S) SuspectsList.Add(S);
	}
	else if (Det && Det->GetWorld())
	{
		for (TActorIterator<ANoirSuspect> It(Det->GetWorld()); It; ++It) SuspectsList.Add(*It);
	}

	FString Seed = Director ? Director->GetPlayedSeed() : TEXT("-");
	FString Difficulty = Director ? Director->Difficulty : TEXT("Detective");

	// Close Button on top right
	TSharedRef<TWeakPtr<SButton>> CloseBtnRef = MakeShared<TWeakPtr<SButton>>();
	TSharedRef<SButton> CloseBtn = SNew(SButton)
		.ButtonStyle(&PlainButton())
		.ContentPadding(FMargin(12.f, 6.f))
		.OnClicked_Lambda([this]()
		{
			if (OnClose) OnClose();
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(SolidBrush(HairlineRule))
			.Padding(FMargin(1.f))
			[
				SNew(SBorder)
				.BorderImage(SolidBrush(FLinearColor(0.035f, 0.025f, 0.018f, 0.95f)))
				.Padding(FMargin(10.f, 4.f))
				[
					SNew(STextBlock)
					.Text(NoirTr(TEXT("[ N / ESC ]   CLOSE DOSSIER"), TEXT("[ N / ESC ]   CERRAR EXPEDIENTE")))
					.Font(Mono(10.5f, 50))
					.ColorAndOpacity_Lambda([CloseBtnRef]()
					{
						TSharedPtr<SButton> B = CloseBtnRef->Pin();
						return FSlateColor(B && (B->IsHovered() || B->HasKeyboardFocus()) ? Amber : Cream);
					})
				]
			]
		];
	*CloseBtnRef = CloseBtn;

	// Suspects list builder (Knives Out Cast list with dedicated dossier plates)
	TSharedRef<SVerticalBox> SuspectsBox = SNew(SVerticalBox);
	if (SuspectsList.IsEmpty())
	{
		SuspectsBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NoirTr(TEXT("No persons of interest catalogued yet."), TEXT("Sin personas de interés catalogadas todavía.")))
			.Font(Font("Italic", 13.f))
			.ColorAndOpacity(Dim)
		];
	}
	else
	{
		for (int32 i = 0; i < SuspectsList.Num(); ++i)
		{
			ANoirSuspect* Suspect = SuspectsList[i];
			const bool bCracked = Suspect->IsCracked();

			// Detect if detective has interviewed this suspect
			bool bQuestioned = false;
			for (int32 t = 0; t < Suspect->Topics.Num(); ++t)
			{
				if (Suspect->WasAsked(t)) { bQuestioned = true; break; }
			}

			TSharedRef<SWidget> StatusBadge = SNew(SBox);
			if (bCracked)
			{
				StatusBadge = SNew(SBorder)
					.BorderImage(SolidBrush(OxbloodBg))
					.Padding(FMargin(7.f, 2.5f))
					[
						SNew(STextBlock)
						.Text(NoirTr(TEXT("CONFESSED"), TEXT("CONFESIÓN")))
						.Font(Mono(9.f, 60))
						.ColorAndOpacity(Oxblood)
					];
			}
			else if (bQuestioned)
			{
				StatusBadge = SNew(SBorder)
					.BorderImage(SolidBrush(FLinearColor(0.015f, 0.035f, 0.02f, 0.9f)))
					.Padding(FMargin(7.f, 2.5f))
					[
						SNew(STextBlock)
						.Text(NoirTr(TEXT("INTERVIEWED"), TEXT("INTERROGADO")))
						.Font(Mono(8.5f, 50))
						.ColorAndOpacity(FLinearColor(0.50f, 0.72f, 0.50f))
					];
			}
			else
			{
				StatusBadge = SNew(SBorder)
					.BorderImage(SolidBrush(FLinearColor(0.02f, 0.018f, 0.015f, 0.8f)))
					.Padding(FMargin(7.f, 2.5f))
					[
						SNew(STextBlock)
						.Text(NoirTr(TEXT("UNQUESTIONED"), TEXT("SIN INTERROGAR")))
						.Font(Mono(8.5f, 40))
						.ColorAndOpacity(DimMuted)
					];
			}

			SuspectsBox->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.5f)
			[
				SNew(SBorder)
				.BorderImage(SolidBrush(CardBg))
				.Padding(FMargin(10.f, 7.f))
				[
					SNew(SHorizontalBox)
					// Suspect signature color vertical line
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox)
						.WidthOverride(4.f)
						[
							SNew(SBorder)
							.BorderImage(SolidBrush(Suspect->SignatureColor))
							.Padding(0.f)
						]
					]
					// Suspect Name & Role
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Suspect->DisplayName)
							.Font(Font("Medium", 14.5f, 10))
							.ColorAndOpacity(Cream)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text(FText::Format(INVTEXT("—  {0}"), Suspect->Occupation))
							.Font(Font("Italic", 12.5f))
							.ColorAndOpacity(Dim)
						]
					]
					// Broken / Interview status badge
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						StatusBadge
					]
				]
			];
		}
	}

	// Complication widget if active
	TSharedRef<SWidget> ComplicationWidget = SNew(SBox);
	if (!CleanCompNote.IsEmpty())
	{
		ComplicationWidget = SNew(SBorder)
			.BorderImage(SolidBrush(AmberMemoBg))
			.Padding(FMargin(12.f, 8.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(CompTitle)
					.Font(Mono(9.5f, 80))
					.ColorAndOpacity(Amber)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(CleanCompNote)
					.Font(Font("Italic", 13.5f))
					.ColorAndOpacity(Cream)
					.AutoWrapText(true)
				]
			];
	}

	// Helper lambda for creating styled Alan Wake 2 Mind Board Inquiry Tiles
	auto MakeInquiryTile = [](const FText& RomanNum, const FText& InqTitle, const FText& InqDesc) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(SolidBrush(CardBg))
			.Padding(FMargin(12.f, 7.f))
			[
				SNew(SHorizontalBox)
				// Roman Numeral Tag
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(SolidBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.9f)))
					.Padding(FMargin(6.f, 2.5f))
					[
						SNew(STextBlock).Text(RomanNum).Font(Mono(10.f, 60)).ColorAndOpacity(Amber)
					]
				]
				// Inquiry Title & Objective
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(InqTitle).Font(Font("Medium", 13.5f, 20)).ColorAndOpacity(Cream)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock).Text(InqDesc).Font(Font("Italic", 12.f)).ColorAndOpacity(Dim).AutoWrapText(true)
					]
				]
				// Pending tag
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(NoirTr(TEXT("EVIDENCE NEEDED"), TEXT("REQUIERE PRUEBAS")))
					.Font(Mono(8.5f, 40))
					.ColorAndOpacity(DimMuted)
				]
			];
	};

	// Optional Accomplice Article V
	TSharedRef<SWidget> AccompliceWidget = SNew(SBox);
	if ((Board && Board->bHasAccomplice) || Summary.bAccomplice)
	{
		AccompliceWidget = MakeInquiryTile(
			INVTEXT("V"),
			NoirTr(TEXT("THE ACCOMPLICE"), TEXT("EL CÓMPLICE")),
			NoirTr(TEXT("Identify who conspired to conceal the crime."), TEXT("Identificar quién conspiró para encubrir el crimen.")));
	}

	// The hints taken so far, or how to take one.
	TSharedRef<SVerticalBox> HintsBox = SNew(SVerticalBox);
	if (Summary.HintsTaken.IsEmpty())
	{
		HintsBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NoirTr(TEXT("No hunches yet. Press A when you are stuck."), TEXT("Ninguna corazonada aún. Pulsa A si te atascas.")))
			.Font(Font("Italic", 12.5f))
			.ColorAndOpacity(DimMuted)
		];
	}
	for (int32 h = 0; h < Summary.HintsTaken.Num(); ++h)
	{
		HintsBox->AddSlot().AutoHeight().Padding(0.f, 2.5f)
		[
			SNew(SBorder)
			.BorderImage(SolidBrush(AmberMemoBg))
			.Padding(FMargin(12.f, 7.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock).Text(FText::AsNumber(h + 1)).Font(Mono(10.f, 60)).ColorAndOpacity(Amber)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(Summary.HintsTaken[h]).Font(Font("Italic", 13.f)).ColorAndOpacity(Cream).AutoWrapText(true)
				]
			]
		];
	}

	// Hint count string
	int32 HintsLeft = Director ? Director->GetHintsLeft() : 0;
	FString IntuitionActionStr = FString::Printf(TEXT("DETECTIVE INTUITION  (%d REMAINING)"), HintsLeft);
	FString IntuitionActionStrEs = FString::Printf(TEXT("INTUICIÓN DETECTIVE  (%d DISPONIBLES)"), HintsLeft);

	return SNew(SVerticalBox)
		// ─── Header: Red Rubber Stamp, Case Title, Metadata Pills & Close Button ───
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				// Stamp & Title Row
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					// Red Typewriter Ink Stamp
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 14.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(SolidBrush(Oxblood))
						.Padding(FMargin(1.f))
						[
							SNew(SBorder)
							.BorderImage(SolidBrush(OxbloodBg))
							.Padding(FMargin(8.f, 3.f))
							[
								SNew(STextBlock)
								.Text(NoirTr(TEXT("CONFIDENTIAL // HOMICIDE DOSSIER"), TEXT("CONFIDENCIAL // EXPEDIENTE DE HOMICIDIO")))
								.Font(Mono(9.5f, 80))
								.ColorAndOpacity(Oxblood)
							]
						]
					]
					// Case Title in dramatic literary serif typography
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(Font("Light", 28.f, 20))
						.ColorAndOpacity(Cream)
					]
				]
				// Archival Tag Badges Row (Clean, responsive, never clips)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						MakeTagBadge(NoirTr(TEXT("FILE:"), TEXT("EXP: ")), FText::FromString(Seed.ToUpper()), Amber)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						MakeTagBadge(NoirTr(TEXT("DIFFICULTY:"), TEXT("DIFICULTAD:")), FText::FromString(Difficulty.ToUpper()), Cream)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						MakeTagBadge(NoirTr(TEXT("SETTING:"), TEXT("ENTORNO:")), FText::FromString(Place.ToString().ToUpper()), Cream)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				CloseBtn
			]
		]
		// Hairline etched rule under header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 14.f, 0.f, 14.f)
		[
			SNew(SBorder)
			.BorderImage(SolidBrush(HairlineRule))
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
		]
		// ─── Main Dossier Content (Scrollable so it never clips) ───
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Vertical)
			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 16.f, 0.f)
			[
				SNew(SVerticalBox)
				// ─── Row 1: The Incident Briefing (Left) & The Deceased / Crime Scene (Right) ───
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					// Left Panel: Case Narrative Synopsis & Estate Complication
					+ SHorizontalBox::Slot()
					.FillWidth(0.58f)
					.Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NoirTr(TEXT("✦  I N C I D E N T   B R I E F I N G"), TEXT("✦  I N F O R M E   D E L   I N C I D E N T E")))
							.Font(Font("Light", 11.5f, 300))
							.ColorAndOpacity(Amber)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(SolidBrush(CardBg))
							.Padding(FMargin(16.f, 12.f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(SynopsisText)
									.Font(Font("Regular", 14.5f))
									.ColorAndOpacity(Cream)
									.AutoWrapText(true)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									ComplicationWidget
								]
							]
						]
					]
					// Right Panel: The Victim Profile & Crime Scene Facts
					+ SHorizontalBox::Slot()
					.FillWidth(0.42f)
					.Padding(16.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NoirTr(TEXT("✦  T H E   D E C E A S E D   &   S C E N E"), TEXT("✦  E L   F A L L E C I D O   Y   E S C E N A")))
							.Font(Font("Light", 11.5f, 300))
							.ColorAndOpacity(Amber)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(SolidBrush(CardBg))
							.Padding(FMargin(16.f, 12.f))
							[
								SNew(SVerticalBox)
								// Victim Name & Status Pill
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(VictimName)
										.Font(Font("Light", 22.f, 15))
										.ColorAndOpacity(Cream)
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
									[
										SNew(SBorder)
										.BorderImage(SolidBrush(OxbloodBg))
										.Padding(FMargin(6.f, 2.f))
										[
											SNew(STextBlock)
											.Text(NoirTr(TEXT("HOMICIDE"), TEXT("HOMICIDIO")))
											.Font(Mono(8.5f, 60))
											.ColorAndOpacity(Oxblood)
										]
									]
								]
								// Hairline inside card
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 8.f)
								[
									SNew(SBorder).BorderImage(SolidBrush(HairlineRule)).Padding(FMargin(0.f, 1.f, 0.f, 0.f))
								]
								// Crime Scene Facts Grid
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NoirTr(TEXT("LOCATION OF BODY"), TEXT("LUGAR DEL CUERPO"))).Font(Mono(8.5f, 80)).ColorAndOpacity(Dim)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[SNew(STextBlock).Text(CrimeScene).Font(Font("Medium", 13.5f)).ColorAndOpacity(Cream)]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NoirTr(TEXT("ESTIMATED TIME"), TEXT("HORA ESTIMADA"))).Font(Mono(8.5f, 80)).ColorAndOpacity(Dim)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[SNew(STextBlock).Text(TimeOfDeath).Font(Font("Medium", 13.5f)).ColorAndOpacity(Cream)]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NoirTr(TEXT("INVESTIGATION"), TEXT("INVESTIGACIÓN"))).Font(Mono(8.5f, 80)).ColorAndOpacity(Dim)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[SNew(STextBlock).Text(NoirTr(TEXT("Active Murder Inquiry"), TEXT("Pesquisa abierta"))).Font(Font("Medium", 13.5f)).ColorAndOpacity(Amber)]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NoirTr(TEXT("THE VICTIM"), TEXT("LA VÍCTIMA"))).Font(Mono(8.5f, 80)).ColorAndOpacity(Dim)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[SNew(STextBlock).Text(VictimLine.IsEmpty() ? Unknown : VictimLine).Font(Font("Medium", 13.5f)).ColorAndOpacity(Cream).AutoWrapText(true)]
									]
								]
							]
						]
					]
				]
				// Hairline divider
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 16.f, 0.f, 14.f)
				[
					SNew(SBorder)
					.BorderImage(SolidBrush(HairlineRule))
					.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
				]
				// ─── Row 2: Suspects Gallery (Knives Out Cast) & The Case Inquiries (Alan Wake 2 Mind Board) ───
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					// Left Column: Persons of Interest Present
					+ SHorizontalBox::Slot()
					.FillWidth(0.50f)
					.Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NoirTr(TEXT("✦  P E R S O N S   O F   I N T E R E S T"), TEXT("✦  P E R S O N A S   D E   I N T E R É S")))
							.Font(Font("Light", 11.5f, 300))
							.ColorAndOpacity(Amber)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 8.f)
						[
							SNew(STextBlock)
							.Text(NoirTr(
								TEXT("Present when the crime occurred. Interview them to uncover lies and break alibis:"),
								TEXT("Presentes cuando ocurrió el crimen. Interrógales para destapar mentiras y romper coartadas:")))
							.Font(Font("Italic", 12.5f))
							.ColorAndOpacity(Dim)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SuspectsBox
						]
					]
					// Right Column: The Case Inquiries (Alan Wake 2 Mind Board)
					+ SHorizontalBox::Slot()
					.FillWidth(0.50f)
					.Padding(16.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NoirTr(TEXT("✦  T H E   C A S E   I N Q U I R I E S"), TEXT("✦  L A S   I N C Ó G N I T A S   D E L   C A S O")))
							.Font(Font("Light", 11.5f, 300))
							.ColorAndOpacity(Amber)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 8.f)
						[
							SNew(STextBlock)
							.Text(NoirTr(
								TEXT("To convict the killer at the Case Board (or in your Mind Place [Tab]), you must establish:"),
								TEXT("Para condenar al culpable en el tablero (o en tu palacio mental [Tab]), debes demostrar:")))
							.Font(Font("Italic", 12.5f))
							.ColorAndOpacity(Dim)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.5f)
						[
							MakeInquiryTile(
								INVTEXT("I"),
								NoirTr(TEXT("THE KILLER"), TEXT("EL ASESINO")),
								FText::Format(NoirTr(TEXT("Who among the {0} persons of interest committed the murder?"), TEXT("¿Quién de los {0} sospechosos cometió el asesinato?")), SuspectCount))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.5f)
						[
							MakeInquiryTile(
								INVTEXT("II"),
								NoirTr(TEXT("THE MOTIVE"), TEXT("EL MOTIVO")),
								NoirTr(TEXT("Why was the victim targeted and killed tonight?"), TEXT("¿Por qué murió la víctima esta noche?")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.5f)
						[
							MakeInquiryTile(
								INVTEXT("III"),
								NoirTr(TEXT("THE WEAPON"), TEXT("EL ARMA")),
								NoirTr(TEXT("What instrument was used to deliver the fatal blow?"), TEXT("¿Con qué instrumento se acabó con la víctima?")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.5f)
						[
							MakeInquiryTile(
								INVTEXT("IV"),
								NoirTr(TEXT("BROKEN ALIBI"), TEXT("COARTADA ROTA")),
								NoirTr(TEXT("Whose statement or alibi collapses under physical evidence?"), TEXT("¿Qué declaración o coartada se desmorona ante las pruebas?")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.5f)
						[
							AccompliceWidget
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)
						[
							SNew(STextBlock)
							.Text(NoirTr(TEXT("✦  D E T E C T I V E ' S   I N T U I T I O N"), TEXT("✦  I N T U I C I Ó N   D E L   D E T E C T I V E")))
							.Font(Font("Light", 11.5f, 300))
							.ColorAndOpacity(Amber)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							HintsBox
						]
					]
				]
			]
		]
		// Hairline etched rule above footer
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 14.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(SolidBrush(HairlineRule))
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
		]
		// ─── Footer: Minimal Cinematic Keycap Prompts ───
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 18.f, 0.f)
				[
					MakeKeycapPrompt(
						INVTEXT("TAB"),
						NoirTr(TEXT("Mind Place (3D Clues & Case Board)"), TEXT("Palacio mental (Pruebas 3D y tablero)")),
						Amber)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeKeycapPrompt(
						INVTEXT("A"),
						FText::FromString(NoirIsSpanish() ? IntuitionActionStrEs : IntuitionActionStr),
						Amber)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeKeycapPrompt(
					INVTEXT("N / ESC"),
					NoirTr(TEXT("Close Dossier"), TEXT("Cerrar expediente")),
					Cream)
			]
		];
}

FReply SNoirCaseNotebook::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();

	// Close on N, Escape, or Gamepad FaceButton_Right (B)
	if (Key == EKeys::Escape || Key == EKeys::N || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (OnClose) OnClose();
		return FReply::Handled();
	}

	// Press Tab while in notebook: Close notebook and jump straight into Mind Place
	if (Key == EKeys::Tab)
	{
		if (OnClose) OnClose();
		if (ANoirCharacter* Det = Detective.Get())
		{
			Det->ToggleMindPlace();
		}
		return FReply::Handled();
	}

	// Press A to consult intuition hint
	if (Key == EKeys::A || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (ANoirCharacter* Det = Detective.Get())
		{
			if (ANoirCaseDirector* Director = Cast<ANoirCaseDirector>(UGameplayStatics::GetActorOfClass(Det, ANoirCaseDirector::StaticClass())))
			{
				if (Director->GetHintsLeft() > 0)
				{
					Director->TakeHint();
					Refresh();
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnKeyDown(Geometry, Event);
}
