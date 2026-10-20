// Case content: every name, motive and clue sentence, in English and Spanish.
// Loaded from Content/Data/Mystery/content.json, which is exported from the web game.
#pragma once

#include "MysteryCore/Api.h"
#include "MysteryCore/Text.h"

#include <map>
#include <string>
#include <vector>

namespace mystery
{

class Json;

/** The physical traits used for deduction, in the order the generator iterates them. */
inline const std::vector<std::string>& AllDims()
{
	static const std::vector<std::string> Dims = {"height", "coat", "voice", "hand", "footwear", "habit", "blood"};
	return Dims;
}

struct TraitValue
{
	std::string key;
	LText label; // on the suspect profile: "Tall"
	LText clue;  // inside clue sentences: "a {value} figure"
};

struct Motive
{
	std::string key;
	LText label, docTitle, docSummary, doc, deflect, admit;
};

struct Cause
{
	LText autopsy, slipNoun;
};

struct Place
{
	LText name;  // with article, lowercase: "the library"
	LText label; // title form: "Library"
};

struct SuspectSeed
{
	std::string name;
	LText role;
	std::string look; // portrait styling: "f" or "m"
};

struct Weapon
{
	std::string key;
	LText name, label;
	std::string cause;
	LText trace, desc;
};

struct Victim
{
	std::string name;
	LText occupation;
};

struct Theme
{
	std::vector<LText> titles;
	std::vector<Victim> victims;
	std::vector<Place> scenes;
	std::vector<Place> alibiPlaces;
	std::vector<Place> witnesses;
	std::vector<LText> cameras;
	LText floor;
	LText phone;
	std::vector<SuspectSeed> suspects;
	std::vector<LText> bioLines;
	std::vector<Weapon> weapons;
	LText synopsis;
};

struct Modifier
{
	LText label, note;
};

struct PlantedItem
{
	LText name, cap, full;
};

struct Spot
{
	LText name, label;
};

class MYSTERYCORE_API Content
{
public:
	/** Returns false and fills Error if the JSON is malformed or incomplete. */
	bool Load(const Json& json, std::string* error = nullptr);
	bool LoadFile(const std::string& path, std::string* error = nullptr);

	std::map<std::string, std::vector<TraitValue>> traits;
	std::vector<std::string> leadDims;
	std::vector<std::string> analystDims;
	std::vector<Motive> motives;
	/** Per language, each motive's position in locale-aware alphabetical order. */
	std::vector<int> motiveRank[2];
	std::map<std::string, Cause> causes;
	std::map<std::string, Modifier> modifiers;
	LText tamperedNote;
	std::vector<LText> fakeBlood;
	std::vector<Spot> spots;
	std::vector<LText> spotNothing;
	std::vector<LText> spotScene;
	LText spotFound;
	std::vector<PlantedItem> plantedItems;
	std::vector<std::string> themeKeys;
	std::map<std::string, Theme> themes;

	/** Templates from the TEXT, FOLLOW_UP and STORY tables, by key. */
	const LText& Text(const std::string& key) const { return Lookup(text_, key); }
	const LText& FollowUp(const std::string& key) const { return Lookup(followUp_, key); }
	const LText& FollowUpSeen(const std::string& dim) const { return Lookup(followUpSeen_, dim); }
	const LText& Story(const std::string& key) const { return Lookup(story_, key); }

	bool IsLeadDim(const std::string& dim) const;
	bool IsAnalystDim(const std::string& dim) const;

private:
	static const LText& Lookup(const std::map<std::string, LText>& table, const std::string& key);
	std::map<std::string, LText> text_;
	std::map<std::string, LText> followUp_;
	std::map<std::string, LText> followUpSeen_;
	std::map<std::string, LText> story_;
};

} // namespace mystery
