// Console command for trying the case generator in the game:
//   Noir.GenerateCase [seed] [theme] [difficulty] [en|es]
// Logs the case as the players would see it, then the solution.

#include "HAL/IConsoleManager.h"
#include "MysteryCore/Blueprint.h"
#include "MysteryCore/Case.h"
#include "MysteryCore/Content.h"
#include "NoirCaseContent.h"

namespace
{
	void GenerateCase(const TArray<FString>& Args)
	{
		const mystery::Content* Content = NoirLoadCaseContent();
		if (!Content)
		{
			return;
		}
		const FString Seed = Args.IsValidIndex(0) ? Args[0] : TEXT("RAVEN-1947");
		const FString Theme = Args.IsValidIndex(1) ? Args[1] : TEXT("gothic");
		const FString Difficulty = Args.IsValidIndex(2) ? Args[2] : TEXT("Detective");
		const mystery::Lang Lang = Args.IsValidIndex(3) && Args[3] == TEXT("es") ? mystery::Lang::Es : mystery::Lang::En;

		const mystery::Blueprint Bp = mystery::BuildBlueprint(*Content, NoirUtf8(Difficulty), NoirUtf8(Theme), NoirUtf8(Seed));
		const mystery::PublicMystery Case = mystery::Localize(*Content, Bp, Lang);

		UE_LOG(LogNoirCase, Display, TEXT("=== %s  (%s, %s, seed %s) ==="), *NoirStr(Case.title), *NoirStr(Case.theme), *NoirStr(Case.difficulty), *NoirStr(Case.seed));
		UE_LOG(LogNoirCase, Display, TEXT("%s"), *NoirStr(Case.synopsis));
		if (Case.modifier)
		{
			UE_LOG(LogNoirCase, Display, TEXT("[%s] %s"), *NoirStr(Case.modifier->label), *NoirStr(Case.modifier->note));
		}
		for (const mystery::PublicSuspect& Suspect : Case.suspects)
		{
			UE_LOG(LogNoirCase, Display, TEXT("  Suspect: %s (%s), %s"), *NoirStr(Suspect.name), *NoirStr(Suspect.role), *NoirStr(Suspect.alibiStatement));
		}
		for (const mystery::PublicEvidence& Evidence : Case.evidence)
		{
			UE_LOG(LogNoirCase, Display, TEXT("  [%s] %s: %s"), *NoirStr(Evidence.role), *NoirStr(Evidence.title), *NoirStr(Evidence.details));
		}
		UE_LOG(LogNoirCase, Display, TEXT("  Solution: %s"), *NoirStr(mystery::ExplainSolution(*Content, Bp, Lang)));
	}

	FAutoConsoleCommand GenerateCaseCommand(
		TEXT("Noir.GenerateCase"),
		TEXT("Generates a case with the ported web-game generator and logs it. Args: [seed] [theme] [difficulty] [en|es]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&GenerateCase));
}
