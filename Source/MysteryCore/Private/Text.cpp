#include "MysteryCore/Text.h"

#include "MysteryCore/Json.h"

#include <cmath>
#include <cstdio>

namespace mystery
{

namespace
{
	// ─── UTF-8 <-> code points ───

	std::u32string Decode(const std::string& s)
	{
		std::u32string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size();)
		{
			const unsigned char c = static_cast<unsigned char>(s[i]);
			char32_t cp = c;
			int extra = 0;
			if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
			else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
			else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
			++i;
			for (int k = 0; k < extra && i < s.size(); ++k, ++i)
			{
				cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
			}
			out += cp;
		}
		return out;
	}

	std::string Encode(const std::u32string& s)
	{
		std::string out;
		out.reserve(s.size());
		for (const char32_t cp : s)
		{
			if (cp < 0x80) out += static_cast<char>(cp);
			else if (cp < 0x800)
			{
				out += static_cast<char>(0xC0 | (cp >> 6));
				out += static_cast<char>(0x80 | (cp & 0x3F));
			}
			else if (cp < 0x10000)
			{
				out += static_cast<char>(0xE0 | (cp >> 12));
				out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (cp & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xF0 | (cp >> 18));
				out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (cp & 0x3F));
			}
		}
		return out;
	}

	// ─── JavaScript regex character classes ───

	/** \w without the u flag */
	bool IsWord(char32_t c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
	}

	/** \s */
	bool IsSpace(char32_t c)
	{
		return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == 0xA0 || c == 0x1680 ||
			(c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000 ||
			c == 0xFEFF;
	}

	/** [a-záéíóúñ] */
	bool IsSentenceLower(char32_t c)
	{
		return (c >= 'a' && c <= 'z') || c == U'á' || c == U'é' || c == U'í' || c == U'ó' || c == U'ú' || c == U'ñ';
	}

	char32_t ToUpper(char32_t c)
	{
		if (c >= 'a' && c <= 'z') return c - 32;
		if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32; // Latin-1 lowercase
		return c;
	}

	char32_t ToLower(char32_t c)
	{
		if (c >= 'A' && c <= 'Z') return c + 32;
		if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32; // Latin-1 uppercase
		return c;
	}

	/** text.replace(/\bFROM\b/g, TO) for a pattern made of word characters and a space. */
	std::u32string ReplaceWord(const std::u32string& s, const std::u32string& from, const std::u32string& to)
	{
		std::u32string out;
		size_t i = 0;
		while (i < s.size())
		{
			if (s.compare(i, from.size(), from) == 0 &&
				(i == 0 || !IsWord(s[i - 1])) &&
				(i + from.size() == s.size() || !IsWord(s[i + from.size()])))
			{
				out += to;
				i += from.size();
			}
			else
			{
				out += s[i++];
			}
		}
		return out;
	}

	/** s.replace(/(^|[.!?]\s+|¿|¡)([a-záéíóúñ])/g, upper-case the letter) */
	std::u32string CapitalizeSentences(std::u32string s)
	{
		size_t i = 0;
		while (i < s.size())
		{
			// Try the alternatives at i in order; on a match, the letter after the prefix is upper-cased.
			size_t letter = std::u32string::npos;
			if (i == 0 && IsSentenceLower(s[0]))
			{
				letter = 0;
			}
			else if (s[i] == '.' || s[i] == '!' || s[i] == '?')
			{
				size_t j = i + 1;
				while (j < s.size() && IsSpace(s[j])) ++j;
				if (j > i + 1 && j < s.size() && IsSentenceLower(s[j])) letter = j;
			}
			else if ((s[i] == U'¿' || s[i] == U'¡') && i + 1 < s.size() && IsSentenceLower(s[i + 1]))
			{
				letter = i + 1;
			}
			if (letter != std::u32string::npos)
			{
				s[letter] = ToUpper(s[letter]);
				i = letter + 1;
			}
			else
			{
				++i;
			}
		}
		return s;
	}

	bool IsTitleWord(const std::string& word)
	{
		static const char* Titles[] = {"dr.", "don", "doña", "padre", "fra", "sister", "count", "colonel", "professor",
			"general", "maestro", "lord", "lady", "baron", "duchess", "captain", "ambassador", "signor", "signora",
			"madame", "contessa", "commander"};
		std::u32string lower = Decode(word);
		for (char32_t& c : lower) c = ToLower(c);
		const std::string folded = Encode(lower);
		for (const char* t : Titles)
		{
			if (folded == t) return true;
		}
		return false;
	}
} // namespace

LText LText::FromJson(const Json& json)
{
	return LText{json["en"].AsString(), json["es"].AsString()};
}

// ─── Params ───

Params::Entry& Params::Slot(const std::string& key)
{
	for (Entry& e : entries_)
	{
		if (e.key == key) return e;
	}
	entries_.push_back(Entry{key});
	return entries_.back();
}

Params& Params::Set(const std::string& key, std::string value)
{
	Entry& e = Slot(key);
	e.isText = false;
	e.str = std::move(value);
	return *this;
}

Params& Params::Set(const std::string& key, const LText& value)
{
	Entry& e = Slot(key);
	e.isText = true;
	e.text = value;
	return *this;
}

Params& Params::Merge(const Params& other)
{
	for (const Entry& e : other.entries_)
	{
		Slot(e.key) = e;
	}
	return *this;
}

const std::string* Params::Find(const std::string& key, Lang lang) const
{
	for (const Entry& e : entries_)
	{
		if (e.key == key) return e.isText ? &e.text.Get(lang) : &e.str;
	}
	return nullptr;
}

// ─── Fill ───

std::string Fill(const LText& text, Lang lang, const Params& params)
{
	// text[lang].replace(/\{(\w+)\}/g, ...): unknown keys stay as they are.
	const std::string& src = text.Get(lang);
	std::string out;
	out.reserve(src.size() + 32);
	for (size_t i = 0; i < src.size();)
	{
		if (src[i] == '{')
		{
			size_t j = i + 1;
			while (j < src.size() && IsWord(static_cast<unsigned char>(src[j]))) ++j;
			if (j > i + 1 && j < src.size() && src[j] == '}')
			{
				const std::string key = src.substr(i + 1, j - i - 1);
				if (const std::string* value = params.Find(key, lang))
					out += *value;
				else
					out += src.substr(i, j - i + 1);
				i = j + 1;
				continue;
			}
		}
		out += src[i++];
	}

	std::u32string s = Decode(out);
	if (lang == Lang::Es)
	{
		s = ReplaceWord(s, U"de el", U"del");
		s = ReplaceWord(s, U"a el", U"al");
		s = ReplaceWord(s, U"De el", U"Del");
		s = ReplaceWord(s, U"A el", U"Al");
	}
	return Encode(CapitalizeSentences(std::move(s)));
}

// ─── Formatting ───

std::string FormatTime(int minutes, Lang lang)
{
	// Math.floor / % on possibly negative values, as JavaScript does them.
	const int h = static_cast<int>(std::floor(minutes / 60.0)) % 24;
	const int m = minutes % 60;
	char mm[16];
	std::snprintf(mm, sizeof(mm), "%02d", m);
	if (lang == Lang::Es)
	{
		return std::to_string(h) + ":" + mm;
	}
	const int h12 = h % 12 == 0 ? 12 : h % 12;
	return std::to_string(h12) + ":" + mm + (h < 12 ? " AM" : " PM");
}

std::string FormatWindow(int startMinutes, Lang lang)
{
	const std::string a = FormatTime(startMinutes, lang);
	const std::string b = FormatTime(startMinutes + 30, lang);
	return lang == Lang::Es ? "las " + a + " y las " + b : a + " and " + b;
}

std::string JoinNames(const std::vector<std::string>& names, Lang lang)
{
	if (names.size() <= 1) return names.empty() ? "" : names[0];
	std::string out;
	for (size_t i = 0; i + 1 < names.size(); ++i)
	{
		if (i) out += ", ";
		out += names[i];
	}
	return out + (lang == Lang::Es ? " y " : " and ") + names.back();
}

std::string InitialsOf(const std::string& name)
{
	// name.replace(/["“”]/g, '').split(/\s+/).filter(p => !TITLE_WORDS.test(p))
	std::u32string clean;
	for (const char32_t c : Decode(name))
	{
		if (c != '"' && c != U'“' && c != U'”') clean += c;
	}
	// split(/\s+/): runs of whitespace separate pieces; leading or trailing whitespace leaves an empty piece.
	std::vector<std::u32string> split;
	std::u32string cur;
	for (size_t i = 0; i < clean.size();)
	{
		if (IsSpace(clean[i]))
		{
			split.push_back(cur);
			cur.clear();
			while (i < clean.size() && IsSpace(clean[i])) ++i;
		}
		else
		{
			cur += clean[i++];
		}
	}
	split.push_back(cur);
	std::vector<std::u32string> kept;
	for (const std::u32string& p : split)
	{
		if (!IsTitleWord(Encode(p))) kept.push_back(p);
	}
	auto first = [](const std::u32string& p) { return p.empty() ? std::string() : Encode(std::u32string(1, p[0])); };
	std::string out = (kept.empty() ? "" : first(kept[0])) + ".";
	if (kept.size() > 1) out += " " + first(kept.back()) + ".";
	return out;
}

std::string LowerFirst(const std::string& s)
{
	std::u32string d = Decode(s);
	if (!d.empty()) d[0] = ToLower(d[0]);
	return Encode(d);
}

size_t JsLength(const std::string& utf8)
{
	return ToUtf16(utf8).size();
}

std::u16string ToUtf16(const std::string& utf8)
{
	std::u16string out;
	for (const char32_t cp : Decode(utf8))
	{
		if (cp >= 0x10000)
		{
			out += static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10));
			out += static_cast<char16_t>(0xDC00 + ((cp - 0x10000) & 0x3FF));
		}
		else
		{
			out += static_cast<char16_t>(cp);
		}
	}
	return out;
}

std::vector<std::string> SplitSpaces(const std::string& s)
{
	std::vector<std::string> out;
	size_t start = 0;
	for (size_t i = 0; i <= s.size(); ++i)
	{
		if (i == s.size() || s[i] == ' ')
		{
			out.push_back(s.substr(start, i - start));
			start = i + 1;
		}
	}
	return out;
}

std::string Join(const std::vector<std::string>& parts, const std::string& sep)
{
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (i) out += sep;
		out += parts[i];
	}
	return out;
}

} // namespace mystery
