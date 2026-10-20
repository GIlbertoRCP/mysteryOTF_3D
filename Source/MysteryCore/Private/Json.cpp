#include "MysteryCore/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mystery
{

namespace
{
	const Json& NullValue()
	{
		static const Json Null;
		return Null;
	}

	void AppendUtf8(std::string& out, unsigned cp)
	{
		if (cp < 0x80)
		{
			out += static_cast<char>(cp);
		}
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

	struct Parser
	{
		const std::string& s;
		size_t i = 0;
		std::string error;

		void SkipWs()
		{
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
			{
				++i;
			}
		}

		bool Fail(const char* what)
		{
			if (error.empty())
			{
				error = std::string(what) + " at offset " + std::to_string(i);
			}
			return false;
		}

		bool Literal(const char* word)
		{
			for (const char* p = word; *p; ++p, ++i)
			{
				if (i >= s.size() || s[i] != *p)
				{
					return Fail("bad literal");
				}
			}
			return true;
		}

		bool Hex4(unsigned& out)
		{
			if (i + 4 > s.size())
			{
				return Fail("short \\u escape");
			}
			out = 0;
			for (int k = 0; k < 4; ++k)
			{
				const char c = s[i++];
				out <<= 4;
				if (c >= '0' && c <= '9') out |= c - '0';
				else if (c >= 'a' && c <= 'f') out |= c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') out |= c - 'A' + 10;
				else return Fail("bad hex digit");
			}
			return true;
		}

		bool String(std::string& out)
		{
			++i; // opening quote
			while (i < s.size())
			{
				const char c = s[i++];
				if (c == '"')
				{
					return true;
				}
				if (c != '\\')
				{
					out += c;
					continue;
				}
				if (i >= s.size())
				{
					break;
				}
				const char e = s[i++];
				switch (e)
				{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					unsigned cp = 0;
					if (!Hex4(cp)) return false;
					if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u')
					{
						i += 2;
						unsigned lo = 0;
						if (!Hex4(lo)) return false;
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					AppendUtf8(out, cp);
					break;
				}
				default: return Fail("bad escape");
				}
			}
			return Fail("unterminated string");
		}

		bool Value(Json& out)
		{
			SkipWs();
			if (i >= s.size())
			{
				return Fail("unexpected end");
			}
			const char c = s[i];
			if (c == '{')
			{
				++i;
				Json::Object obj;
				SkipWs();
				if (i < s.size() && s[i] == '}')
				{
					++i;
					out = Json(std::move(obj));
					return true;
				}
				while (true)
				{
					SkipWs();
					if (i >= s.size() || s[i] != '"') return Fail("expected key");
					std::string key;
					if (!String(key)) return false;
					SkipWs();
					if (i >= s.size() || s[i] != ':') return Fail("expected ':'");
					++i;
					Json v;
					if (!Value(v)) return false;
					obj.emplace_back(std::move(key), std::move(v));
					SkipWs();
					if (i < s.size() && s[i] == ',') { ++i; continue; }
					if (i < s.size() && s[i] == '}') { ++i; break; }
					return Fail("expected ',' or '}'");
				}
				out = Json(std::move(obj));
				return true;
			}
			if (c == '[')
			{
				++i;
				Json::Array arr;
				SkipWs();
				if (i < s.size() && s[i] == ']')
				{
					++i;
					out = Json(std::move(arr));
					return true;
				}
				while (true)
				{
					Json v;
					if (!Value(v)) return false;
					arr.push_back(std::move(v));
					SkipWs();
					if (i < s.size() && s[i] == ',') { ++i; continue; }
					if (i < s.size() && s[i] == ']') { ++i; break; }
					return Fail("expected ',' or ']'");
				}
				out = Json(std::move(arr));
				return true;
			}
			if (c == '"')
			{
				std::string str;
				if (!String(str)) return false;
				out = Json(std::move(str));
				return true;
			}
			if (c == 't') { if (!Literal("true")) return false; out = Json(true); return true; }
			if (c == 'f') { if (!Literal("false")) return false; out = Json(false); return true; }
			if (c == 'n') { if (!Literal("null")) return false; out = Json(); return true; }
			// Number: strtod parses the shortest round-trip form JavaScript writes back to the same double.
			const char* begin = s.c_str() + i;
			char* end = nullptr;
			const double n = std::strtod(begin, &end);
			if (end == begin) return Fail("unexpected character");
			i += static_cast<size_t>(end - begin);
			out = Json(n);
			return true;
		}
	};

	void DumpString(const std::string& str, std::string& out)
	{
		out += '"';
		for (const char c : str)
		{
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				}
				else
				{
					out += c;
				}
			}
		}
		out += '"';
	}

	void DumpValue(const Json& v, std::string& out)
	{
		switch (v.GetType())
		{
		case Json::Type::Null: out += "null"; break;
		case Json::Type::Bool: out += v.AsBool() ? "true" : "false"; break;
		case Json::Type::Number:
		{
			const double n = v.AsNumber();
			char buf[32];
			if (n == std::floor(n) && std::fabs(n) < 1e15)
				std::snprintf(buf, sizeof(buf), "%.0f", n);
			else
				std::snprintf(buf, sizeof(buf), "%.17g", n);
			out += buf;
			break;
		}
		case Json::Type::String: DumpString(v.AsString(), out); break;
		case Json::Type::Array:
		{
			out += '[';
			bool first = true;
			for (const Json& item : v.AsArray())
			{
				if (!first) out += ',';
				first = false;
				DumpValue(item, out);
			}
			out += ']';
			break;
		}
		case Json::Type::Object:
		{
			out += '{';
			bool first = true;
			for (const auto& [key, item] : v.AsObject())
			{
				if (!first) out += ',';
				first = false;
				DumpString(key, out);
				out += ':';
				DumpValue(item, out);
			}
			out += '}';
			break;
		}
		}
	}
} // namespace

const Json& Json::operator[](const std::string& key) const
{
	if (type_ == Type::Object)
	{
		for (const auto& [k, v] : *object_)
		{
			if (k == key) return v;
		}
	}
	return NullValue();
}

Json& Json::Set(const std::string& key, Json value)
{
	for (auto& [k, v] : *object_)
	{
		if (k == key)
		{
			v = std::move(value);
			return v;
		}
	}
	object_->emplace_back(key, std::move(value));
	return object_->back().second;
}

bool Json::Has(const std::string& key) const
{
	if (type_ != Type::Object) return false;
	for (const auto& [k, v] : *object_)
	{
		if (k == key) return true;
	}
	return false;
}

size_t Json::Size() const
{
	if (type_ == Type::Array) return array_->size();
	if (type_ == Type::Object) return object_->size();
	return 0;
}

bool Json::operator==(const Json& other) const
{
	return JsonDiff(*this, other).empty();
}

std::string Json::Dump() const
{
	std::string out;
	DumpValue(*this, out);
	return out;
}

bool Json::Parse(const std::string& text, Json& out, std::string* error)
{
	Parser p{text};
	const bool ok = p.Value(out);
	if (!ok && error) *error = p.error;
	return ok;
}

std::string JsonDiff(const Json& expected, const Json& actual, const std::string& path)
{
	const std::string where = path.empty() ? "$" : path;
	if (expected.GetType() != actual.GetType())
	{
		return where + ": expected " + expected.Dump().substr(0, 200) + ", got " + actual.Dump().substr(0, 200);
	}
	switch (expected.GetType())
	{
	case Json::Type::Null: return "";
	case Json::Type::Bool:
		return expected.AsBool() == actual.AsBool() ? "" : where + ": bool differs";
	case Json::Type::Number:
		return expected.AsNumber() == actual.AsNumber() ? ""
			: where + ": expected " + expected.Dump() + ", got " + actual.Dump();
	case Json::Type::String:
		return expected.AsString() == actual.AsString() ? ""
			: where + ":\n    expected " + expected.Dump() + "\n    got      " + actual.Dump();
	case Json::Type::Array:
	{
		const auto& a = expected.AsArray();
		const auto& b = actual.AsArray();
		const size_t n = a.size() < b.size() ? a.size() : b.size();
		for (size_t k = 0; k < n; ++k)
		{
			std::string d = JsonDiff(a[k], b[k], where + "[" + std::to_string(k) + "]");
			if (!d.empty()) return d;
		}
		return a.size() == b.size() ? ""
			: where + ": expected " + std::to_string(a.size()) + " items, got " + std::to_string(b.size());
	}
	case Json::Type::Object:
	{
		for (const auto& [key, value] : expected.AsObject())
		{
			if (!actual.Has(key)) return where + "." + key + ": missing";
			std::string d = JsonDiff(value, actual[key], where + "." + key);
			if (!d.empty()) return d;
		}
		for (const auto& [key, value] : actual.AsObject())
		{
			if (!expected.Has(key)) return where + "." + key + ": unexpected (" + value.Dump().substr(0, 80) + ")";
		}
		return "";
	}
	}
	return "";
}

} // namespace mystery
