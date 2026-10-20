// The case with no language attached: who did it, how, and every clue. Built from a
// seed, theme and difficulty; the same inputs always give the same case, in this port
// and in the web game.
#pragma once

#include "MysteryCore/Api.h"
#include "MysteryCore/Json.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mystery
{

class Content;
class SeededRandom;

// ─── Floor plan ───

struct PlanRoom
{
	int idx = 0; // 0 is the crime scene, 1.. are the theme's alibi places
	double x = 0, y = 0, w = 0, h = 0;
	double doorX = 0, doorY = 0;
	int walk = 0; // minutes on foot to the crime scene
};

struct PlanRect
{
	double x = 0, y = 0, w = 0, h = 0;
};

struct Plan
{
	std::string layout; // "corridor" | "linear"
	double width = 0, height = 0;
	std::vector<PlanRoom> rooms; // sorted by idx
	std::optional<PlanRect> corridor;
};

MYSTERYCORE_API Plan GeneratePlan(SeededRandom& rng, int roomCount, const std::string& layout);

// ─── Blueprint ───

struct BlueprintSuspect
{
	std::string id;
	int seedIdx = 0;
	int age = 0;
	std::string color;
	int bioIdx = 0;
	int placeIdx = 0;
	std::map<std::string, int> traits; // dim -> index into Content::traits[dim]
	int motiveIdx = 0;
	std::string motiveOptionId;
	bool verified = false;
	std::string noticed = "nothing"; // "nothing" | "true" | "frame"
	std::optional<int> frameTargetIdx;
};

struct BlueprintEvidence
{
	std::string id;
	std::string role; // "lead" | "analyst"
	std::string kind;
	/** Suspect index (documents and twists), place index (records) or weapon index (weapons). */
	std::optional<int> ref;
	std::optional<int> foundPlaceIdx;
};

struct BlueprintSpot
{
	int room = 0;
	int spotIdx = 0;
	int noteIdx = 0;
	bool holdsItems = false;
};

struct BlueprintWeapon
{
	int weaponIdx = 0;
	std::string optionId;
};

struct Solution
{
	std::string killerId, motiveId, weaponId, alibiEvidenceId;
	std::optional<std::string> accompliceId;
};

struct MYSTERYCORE_API Blueprint
{
	std::string seed, theme, difficulty;
	int titleIdx = 0, victimIdx = 0, victimAge = 0, sceneIdx = 0;
	int startMin = 0, witnessIdx = 0, witnessMin = 0, cameraIdx = 0, cameraMin = 0, phoneMin = 0, todMin = 0;
	Plan plan;

	/** An innocent cleared by distance: seen far from the scene just before the death. */
	struct Sighting { int suspectIdx, witnessIdx; };
	std::optional<Sighting> sighting;
	std::vector<BlueprintSuspect> suspects;
	int killerIdx = 0;
	std::vector<std::string> clueDims;
	bool slip = false;
	/** Mastermind twist: an innocent who lies to cover for the killer. */
	std::optional<int> accompliceIdx;
	/** Twist: an item placed by the body to frame an innocent. */
	struct Planted { int suspectIdx, itemIdx; };
	std::optional<Planted> planted;
	/** Twist: a witness falsely accuses an innocent whom a staff record clears. */
	struct Claim { int suspectIdx, witnessIdx, min; };
	std::optional<Claim> claim;
	/** Twist: a clue held back that arrives a few minutes into the case. */
	std::optional<std::string> lateEvidenceId;
	/** "blackout" (no cameras), "storm" (no footprints) or "tampered" evidence. */
	std::optional<std::string> modifier;
	int powerCutMin = 0;
	struct Tampered { int weaponIdx, fakeIdx; };
	std::optional<Tampered> tampered;
	/** Two searchable spots per room. Room 0 is the crime scene. */
	std::vector<BlueprintSpot> spots;
	int receiptPlaceIdx = 0;
	int receiptMin = 0;
	std::vector<BlueprintWeapon> weapons;
	int trueWeaponIdx = 0;
	std::vector<BlueprintEvidence> evidence;
	Solution solution;

	const BlueprintEvidence* FindEvidence(const std::string& id) const;
	int FindSuspect(const std::string& id) const;
	bool HasEvidenceKind(const std::string& kind) const;

	/** Same shape as JSON.stringify of the TypeScript blueprint (used to check parity). */
	Json ToJson() const;
};

/** Difficulty: "Novice", "Detective" or "Mastermind". Unknown themes fall back to gothic. */
MYSTERYCORE_API Blueprint BuildBlueprint(const Content& content, const std::string& difficulty, const std::string& theme,
	const std::string& seed);

} // namespace mystery
