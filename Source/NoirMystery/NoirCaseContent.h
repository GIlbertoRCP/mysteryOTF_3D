#pragma once

#include "CoreMinimal.h"

#include <string>

namespace mystery
{
	class Content;
}

/** The case content (Content/Data/Mystery/content.json), loaded once; null if it is missing. */
const mystery::Content* NoirLoadCaseContent();

/** UTF-8 strings from MysteryCore to and from Unreal. */
inline FString NoirStr(const std::string& Utf8) { return FString(UTF8_TO_TCHAR(Utf8.c_str())); }
inline FText NoirText(const std::string& Utf8) { return FText::FromString(NoirStr(Utf8)); }
inline std::string NoirUtf8(const FString& Text) { return std::string(TCHAR_TO_UTF8(*Text)); }

DECLARE_LOG_CATEGORY_EXTERN(LogNoirCase, Log, All);

/**
 * The language of the case being played. The case director sets it; the case-facing interface
 * (prompts, menus, the dossier) follows it so a Spanish case reads in Spanish throughout.
 * A small stand-in for Unreal's localization pipeline, which can replace NoirTr later.
 */
void NoirSetSpanish(bool bSpanish);
bool NoirIsSpanish();

inline FText NoirTr(const TCHAR* En, const TCHAR* Es) { return FText::FromString(NoirIsSpanish() ? Es : En); }
inline FString NoirTrS(const TCHAR* En, const TCHAR* Es) { return NoirIsSpanish() ? Es : En; }
