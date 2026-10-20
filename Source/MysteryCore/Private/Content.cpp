#include "MysteryCore/Content.h"

#include "MysteryCore/Json.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace mystery
{

namespace
{
	std::vector<LText> Texts(const Json& arr)
	{
		std::vector<LText> out;
		for (const Json& item : arr.AsArray()) out.push_back(LText::FromJson(item));
		return out;
	}

	Place ToPlace(const Json& j) { return Place{LText::FromJson(j["name"]), LText::FromJson(j["label"])}; }

	std::vector<Place> Places(const Json& arr)
	{
		std::vector<Place> out;
		for (const Json& item : arr.AsArray()) out.push_back(ToPlace(item));
		return out;
	}

	std::vector<std::string> Strings(const Json& arr)
	{
		std::vector<std::string> out;
		for (const Json& item : arr.AsArray()) out.push_back(item.AsString());
		return out;
	}

	void TextTable(const Json& obj, std::map<std::string, LText>& out)
	{
		for (const auto& [key, value] : obj.AsObject())
		{
			if (value.Has("en")) out[key] = LText::FromJson(value);
		}
	}
} // namespace

bool Content::LoadFile(const std::string& path, std::string* error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		if (error) *error = "cannot open " + path;
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	Json json;
	if (!Json::Parse(buffer.str(), json, error)) return false;
	return Load(json, error);
}

bool Content::Load(const Json& j, std::string* error)
{
	if (!j.IsObject() || j["version"].AsInt() != 1 || !j["themes"].IsObject())
	{
		if (error) *error = "not a version 1 case content file";
		return false;
	}

	for (const auto& [dim, values] : j["traits"].AsObject())
	{
		for (const Json& v : values.AsArray())
		{
			traits[dim].push_back(TraitValue{v["key"].AsString(), LText::FromJson(v["label"]), LText::FromJson(v["clue"])});
		}
	}
	leadDims = Strings(j["leadDims"]);
	analystDims = Strings(j["analystDims"]);

	for (const Json& m : j["motives"].AsArray())
	{
		motives.push_back(Motive{m["key"].AsString(), LText::FromJson(m["label"]), LText::FromJson(m["docTitle"]),
			LText::FromJson(m["docSummary"]), LText::FromJson(m["doc"]), LText::FromJson(m["deflect"]),
			LText::FromJson(m["admit"])});
	}
	for (int lang = 0; lang < 2; ++lang)
	{
		for (const Json& r : j["motiveRank"][lang == 0 ? "en" : "es"].AsArray()) motiveRank[lang].push_back(r.AsInt());
	}

	for (const auto& [key, c] : j["causes"].AsObject())
	{
		causes[key] = Cause{LText::FromJson(c["autopsy"]), LText::FromJson(c["slipNoun"])};
	}
	for (const auto& [key, m] : j["modifiers"].AsObject())
	{
		modifiers[key] = Modifier{LText::FromJson(m["label"]), LText::FromJson(m["note"])};
	}
	tamperedNote = LText::FromJson(j["tamperedNote"]);
	fakeBlood = Texts(j["fakeBlood"]);
	for (const Json& s : j["spots"].AsArray())
	{
		spots.push_back(Spot{LText::FromJson(s["name"]), LText::FromJson(s["label"])});
	}
	spotNothing = Texts(j["spotNothing"]);
	spotScene = Texts(j["spotScene"]);
	spotFound = LText::FromJson(j["spotFound"]);
	for (const Json& p : j["plantedItems"].AsArray())
	{
		plantedItems.push_back(PlantedItem{LText::FromJson(p["name"]), LText::FromJson(p["cap"]), LText::FromJson(p["full"])});
	}

	TextTable(j["text"], text_);
	TextTable(j["followUp"], followUp_);
	TextTable(j["followUp"]["seen"], followUpSeen_);
	TextTable(j["story"], story_);

	themeKeys = Strings(j["themeKeys"]);
	for (const std::string& key : themeKeys)
	{
		const Json& t = j["themes"][key];
		Theme th;
		th.titles = Texts(t["titles"]);
		for (const Json& v : t["victims"].AsArray())
		{
			th.victims.push_back(Victim{v["name"].AsString(), LText::FromJson(v["occupation"])});
		}
		th.scenes = Places(t["scenes"]);
		th.alibiPlaces = Places(t["alibiPlaces"]);
		th.witnesses = Places(t["witnesses"]);
		th.cameras = Texts(t["cameras"]);
		th.floor = LText::FromJson(t["floor"]);
		th.phone = LText::FromJson(t["phone"]);
		for (const Json& s : t["suspects"].AsArray())
		{
			th.suspects.push_back(SuspectSeed{s["name"].AsString(), LText::FromJson(s["role"]), s["look"].AsString()});
		}
		th.bioLines = Texts(t["bioLines"]);
		for (const Json& w : t["weapons"].AsArray())
		{
			th.weapons.push_back(Weapon{w["key"].AsString(), LText::FromJson(w["name"]), LText::FromJson(w["label"]),
				w["cause"].AsString(), LText::FromJson(w["trace"]), LText::FromJson(w["desc"])});
		}
		th.synopsis = LText::FromJson(t["synopsis"]);
		themes[key] = std::move(th);
	}
	return true;
}

bool Content::IsLeadDim(const std::string& dim) const
{
	return std::find(leadDims.begin(), leadDims.end(), dim) != leadDims.end();
}

bool Content::IsAnalystDim(const std::string& dim) const
{
	return std::find(analystDims.begin(), analystDims.end(), dim) != analystDims.end();
}

const LText& Content::Lookup(const std::map<std::string, LText>& table, const std::string& key)
{
	static const LText Missing{"{missing}", "{missing}"};
	const auto it = table.find(key);
	return it != table.end() ? it->second : Missing;
}

} // namespace mystery
