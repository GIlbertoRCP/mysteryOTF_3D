// Staging: where a generated case physically sits in a hand-built level. The web game only
// needs a floor plan; the 3D game also needs to know which real room plays each case room,
// which piece of furniture hides each searchable spot, where every clue is found and where
// each suspect stands.
//
// Staging is a separate step after BuildBlueprint, with its own random stream, so it never
// changes the blueprint (which must stay identical to the web game's).
#pragma once

#include "MysteryCore/Api.h"
#include "MysteryCore/Blueprint.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mystery
{

/** Furniture kinds, matching the web game's searchable spots by index. */
MYSTERYCORE_API const std::vector<std::string>& SpotKinds();

// ─── What the level offers ───

struct StageFurniture
{
	std::string id;
	std::string kind; // one of SpotKinds(), or anything else for furniture with no matching spot
};

struct StageRoom
{
	std::string id;
	bool canBeScene = false; // has a place for the body
	std::vector<StageFurniture> furniture;
	int standPoints = 0;
	/** Case places this room is dressed for, by English label ("Dining Car"); preferred when casting.
	 *  Empty: any place. */
	std::vector<std::string> suits;
};

struct StageLayout
{
	std::vector<StageRoom> rooms;
};

// ─── Where things went ───

struct EvidencePlacement
{
	enum class Where
	{
		CaseFile,  // a statement, record or lab result: known from the start (or as a late lead)
		Furniture, // hidden in a piece of furniture, found by searching it
		Body,      // on or beside the body
	};
	std::string evidenceId;
	Where where = Where::CaseFile;
	std::string roomId;      // Furniture and Body
	std::string furnitureId; // Furniture
	bool late = false;       // the late lead: arrives a few minutes into the case
};

struct SuspectPlacement
{
	std::string suspectId;
	std::string roomId;
	int standIdx = 0; // may exceed the room's stand points; the level then improvises
};

struct SpotPlacement
{
	std::string spotId; // "spot-<room>-<n>", as SearchSpot takes it
	std::string roomId;
	std::string furnitureId;
};

struct MYSTERYCORE_API Staging
{
	/** Case room (0 = scene, 1.. = alibi place + 1) -> level room. Unmapped case rooms are offstage. */
	std::map<int, std::string> roomFor;
	std::string sceneRoomId;
	std::vector<SpotPlacement> spots;
	std::vector<EvidencePlacement> evidence;
	std::vector<SuspectPlacement> suspects;
	/** Problems that make the case unfair to play in this level (empty when all is well). */
	std::vector<std::string> problems;

	const EvidencePlacement* FindEvidence(const std::string& id) const;
	/** Case room index played by a level room, or -1. */
	int CaseRoomOf(const std::string& roomId) const;
};

MYSTERYCORE_API Staging StageCase(const Content& content, const Blueprint& bp, const StageLayout& layout);

} // namespace mystery
