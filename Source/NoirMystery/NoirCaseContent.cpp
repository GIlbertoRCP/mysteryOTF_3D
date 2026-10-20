#include "NoirCaseContent.h"

#include "Misc/Paths.h"
#include "MysteryCore/Content.h"

DEFINE_LOG_CATEGORY(LogNoirCase);

namespace
{
	bool GNoirSpanish = false;
}

void NoirSetSpanish(bool bSpanish)
{
	GNoirSpanish = bSpanish;
}

bool NoirIsSpanish()
{
	return GNoirSpanish;
}

const mystery::Content* NoirLoadCaseContent()
{
	static mystery::Content Content;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Data/Mystery/content.json"));
		std::string Error;
		if (!Content.LoadFile(NoirUtf8(Path), &Error))
		{
			UE_LOG(LogNoirCase, Error, TEXT("Could not load case content from %s: %s"), *Path, *NoirStr(Error));
			return nullptr;
		}
		bLoaded = true;
	}
	return &Content;
}
