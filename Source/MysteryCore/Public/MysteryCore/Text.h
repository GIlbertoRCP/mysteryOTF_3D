// Bilingual text and the string helpers the case generator needs, matching the
// TypeScript engine's behaviour exactly (it is the reference). Strings are UTF-8.
#pragma once

#include "MysteryCore/Api.h"
#include <string>
#include <utility>
#include <vector>

namespace mystery
{

class Json;

enum class Lang { En, Es };

/** A piece of text in both languages. Placeholders look like {name}. */
struct MYSTERYCORE_API LText
{
	std::string en;
	std::string es;

	const std::string& Get(Lang lang) const { return lang == Lang::En ? en : es; }
	static LText FromJson(const Json& json);
};

/** Values for a template's placeholders: plain strings, or bilingual text resolved per language. */
class MYSTERYCORE_API Params
{
public:
	Params() = default;
	Params& Set(const std::string& key, std::string value);
	Params& Set(const std::string& key, const LText& value);
	/** Copies every entry of Other over this one (the TypeScript `{ ...a, ...b }`). */
	Params& Merge(const Params& other);
	const std::string* Find(const std::string& key, Lang lang) const;

private:
	struct Entry
	{
		std::string key;
		bool isText = false;
		std::string str;
		LText text;
	};
	std::vector<Entry> entries_;
	Entry& Slot(const std::string& key);
};

/** Fills {placeholders}, applies Spanish contractions (de el -> del), capitalizes sentence starts. */
MYSTERYCORE_API std::string Fill(const LText& text, Lang lang, const Params& params = Params());

/** "10:42 PM" / "22:42" */
MYSTERYCORE_API std::string FormatTime(int minutes, Lang lang);
/** "10:00 PM and 10:30 PM" / "las 22:00 y las 22:30" */
MYSTERYCORE_API std::string FormatWindow(int startMinutes, Lang lang);
/** "A, B and C" / "A, B y C" */
MYSTERYCORE_API std::string JoinNames(const std::vector<std::string>& names, Lang lang);
/** "A. B." from a name, skipping titles such as "Dr." or "Lady". */
MYSTERYCORE_API std::string InitialsOf(const std::string& name);
/** Lower-cases the first character. */
MYSTERYCORE_API std::string LowerFirst(const std::string& s);

/** JavaScript string length (UTF-16 code units). */
MYSTERYCORE_API size_t JsLength(const std::string& utf8);
/** UTF-16 code units of a UTF-8 string, as JavaScript's charCodeAt sees them. */
MYSTERYCORE_API std::u16string ToUtf16(const std::string& utf8);

/** "a b  c" split on single spaces, keeping empty pieces (String.prototype.split(' ')). */
MYSTERYCORE_API std::vector<std::string> SplitSpaces(const std::string& s);
MYSTERYCORE_API std::string Join(const std::vector<std::string>& parts, const std::string& sep);

} // namespace mystery
