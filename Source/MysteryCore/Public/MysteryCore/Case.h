// A blueprint put into words: the case as players see it (never containing the solution),
// plus everything that happens during play: confronting suspects, searching, hints, and
// the true story once the case is closed.
#pragma once

#include "MysteryCore/Api.h"
#include "MysteryCore/Blueprint.h"
#include "MysteryCore/Json.h"
#include "MysteryCore/Text.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mystery
{

class Content;

struct QA
{
	std::string q, a;
};

struct PublicSuspect
{
	std::string id, name, role;
	int age = 0;
	std::string color, bio, alibiPlace, alibiRoomId;
	std::string look; // "f" | "m"
	std::string alibiStatement;
	std::map<std::string, std::string> traits;    // dim -> label
	std::map<std::string, std::string> traitKeys; // dim -> key
	std::vector<QA> interview;
};

/** A small hands-on puzzle that reveals a clue. */
struct Puzzle
{
	std::string type; // "hand" | "sole" | "uv" | "blood" | "torn" | "cipher"
	std::string answer;  // hand, sole, blood
	std::string key;     // uv
	std::vector<std::string> pieces; // torn
	int shift = 0;       // cipher
};

/** A 3D object to turn over in the inspector. */
struct EvidenceModel
{
	std::string type; // "watch" | "monogram" | "letter"
	int min = 0;
	std::string engraving; // watch
	std::string item, initials; // monogram
	std::string seal; // letter
};

struct PublicEvidence
{
	std::string id, role, kind, title, summary, details;
	struct Event
	{
		int min = 0;
		std::optional<std::string> roomId, suspectId;
	};
	std::optional<Event> event;
	std::optional<Puzzle> puzzle;
	struct WeaponInfo
	{
		std::string key;
		bool residue = false;
	};
	std::optional<WeaponInfo> weapon;
	std::optional<std::string> spotId;
	std::optional<std::vector<std::string>> confirms;
	std::optional<EvidenceModel> model;
};

struct PublicPlanRoom
{
	std::string id, label, kind;
	double x = 0, y = 0, w = 0, h = 0, doorX = 0, doorY = 0;
	int walk = 0;
	std::vector<std::pair<std::string, std::string>> spots; // id, label
};

struct Option
{
	std::string id, label;
};

struct MYSTERYCORE_API PublicMystery
{
	std::string caseId, seed, theme, difficulty, lang, title;
	struct Victim
	{
		std::string name;
		int age = 0;
		std::string occupation, scene, timeWindow;
		int windowStart = 0, windowEnd = 0;
	} victim;
	struct PublicPlan
	{
		std::string layout;
		double width = 0, height = 0;
		std::vector<PublicPlanRoom> rooms;
		std::optional<PlanRect> corridor;
	} plan;
	struct ModifierInfo
	{
		std::string key, label, note;
	};
	std::optional<ModifierInfo> modifier;
	std::string synopsis;
	std::vector<PublicSuspect> suspects;
	std::vector<PublicEvidence> evidence;
	std::vector<Option> motiveOptions;
	std::vector<Option> weaponOptions;
	bool accompliceOption = false;

	Json ToJson() const;
};

struct MYSTERYCORE_API ConfrontOutcome
{
	bool cracked = false;
	/** cleared: proves them innocent of the lie; admit: confirms a motive; confess/slip: points somewhere new */
	std::string kind; // "admit" | "cleared" | "confess" | "slip" | "deflect"
	std::string line;
	/** A new question that cracking them unlocks. */
	std::optional<QA> followUp;

	Json ToJson() const;
};

struct SearchResult
{
	std::string text;
	std::vector<std::string> found; // evidence ids
};

struct StoryEntry
{
	int min = 0;
	std::string text;
};

struct DailyCase
{
	std::string seed, theme, difficulty;
};

MYSTERYCORE_API PublicMystery Localize(const Content& content, const Blueprint& bp, Lang lang);
MYSTERYCORE_API std::string ExplainSolution(const Content& content, const Blueprint& bp, Lang lang);
/** Four hints, each narrowing things further, none naming the killer outright. */
MYSTERYCORE_API std::vector<std::string> HintsFor(const Content& content, const Blueprint& bp, Lang lang);
/** Presenting a clue to a suspect. Most clues get a brush-off; the right one makes them crack. */
MYSTERYCORE_API std::optional<ConfrontOutcome> Confront(const Content& content, const Blueprint& bp, const std::string& suspectId,
	const std::string& evidenceId, Lang lang);
/** Searching a spot ("spot-<room>-<n>"): what you find, and which item cards it unlocks. */
MYSTERYCORE_API std::optional<SearchResult> SearchSpot(const Content& content, const Blueprint& bp, const std::string& spotId, Lang lang);
/** The true sequence of events, revealed once the case is closed. */
MYSTERYCORE_API std::vector<StoryEntry> StoryFor(const Content& content, const Blueprint& bp, Lang lang);
/** Everyone gets the same case on the same day ("2026-10-19"). */
MYSTERYCORE_API DailyCase DailyCaseFor(const Content& content, const std::string& date);

} // namespace mystery
