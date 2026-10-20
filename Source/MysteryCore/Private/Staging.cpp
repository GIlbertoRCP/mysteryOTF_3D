#include "MysteryCore/Staging.h"

#include "MysteryCore/Content.h"
#include "MysteryCore/Random.h"

#include <algorithm>
#include <set>

namespace mystery
{

const std::vector<std::string>& SpotKinds()
{
	// Same order as the web game's SPOTS: desk drawer, wastebasket, window, cupboard, rug,
	// bookshelf, coat rack, sofa cushions.
	static const std::vector<std::string> Kinds = {"desk", "wastebasket", "window", "cupboard", "rug", "bookshelf", "coatrack", "sofa"};
	return Kinds;
}

const EvidencePlacement* Staging::FindEvidence(const std::string& id) const
{
	for (const EvidencePlacement& p : evidence)
	{
		if (p.evidenceId == id) return &p;
	}
	return nullptr;
}

int Staging::CaseRoomOf(const std::string& roomId) const
{
	for (const auto& [caseRoom, levelRoom] : roomFor)
	{
		if (levelRoom == roomId) return caseRoom;
	}
	return -1;
}

Staging StageCase(const Content& content, const Blueprint& bp, const StageLayout& layout)
{
	Staging st;
	if (layout.rooms.empty())
	{
		st.problems.push_back("the level has no rooms");
		return st;
	}
	SeededRandom rng(bp.seed + "|" + bp.theme + "|" + bp.difficulty + "|stage");
	const int alibiCount = static_cast<int>(content.themes.at(bp.theme).alibiPlaces.size());

	// ─── Rooms: the scene first, then every room a suspect claims or a weapon lies in ───
	std::vector<const StageRoom*> sceneChoices;
	for (const StageRoom& r : layout.rooms)
	{
		if (r.canBeScene) sceneChoices.push_back(&r);
	}
	if (sceneChoices.empty())
	{
		for (const StageRoom& r : layout.rooms) sceneChoices.push_back(&r);
	}
	// The English label of a case room: 0 is the scene, 1.. the alibi places.
	const Theme& theme = content.themes.at(bp.theme);
	auto caseLabel = [&](int caseRoom) -> std::string
	{
		if (caseRoom == 0) return theme.scenes[bp.sceneIdx].label.en;
		return theme.alibiPlaces[caseRoom - 1].label.en;
	};
	auto suits = [](const StageRoom& r, const std::string& label)
	{
		return std::find(r.suits.begin(), r.suits.end(), label) != r.suits.end();
	};
	{
		// A room dressed for the scene wins (levels without dressing pick exactly as before).
		std::vector<const StageRoom*> dressed;
		for (const StageRoom* r : sceneChoices)
		{
			if (suits(*r, caseLabel(0))) dressed.push_back(r);
		}
		if (!dressed.empty()) sceneChoices = dressed;
	}
	const StageRoom* scene = rng.Pick(sceneChoices);
	st.sceneRoomId = scene->id;
	st.roomFor[0] = scene->id;

	std::vector<int> required; // case rooms that must be physical, in a stable order
	auto require = [&](int caseRoom)
	{
		if (std::find(required.begin(), required.end(), caseRoom) == required.end()) required.push_back(caseRoom);
	};
	for (const BlueprintSuspect& s : bp.suspects) require(s.placeIdx + 1);
	for (const BlueprintEvidence& e : bp.evidence)
	{
		if (e.kind == "weapon") require(*e.foundPlaceIdx + 1);
	}
	std::vector<int> optional;
	for (int caseRoom = 1; caseRoom <= alibiCount; ++caseRoom)
	{
		if (std::find(required.begin(), required.end(), caseRoom) == required.end()) optional.push_back(caseRoom);
	}
	optional = rng.Shuffle(optional);

	std::vector<const StageRoom*> freeRooms;
	for (const StageRoom& r : layout.rooms)
	{
		if (&r != scene) freeRooms.push_back(&r);
	}
	freeRooms = rng.Shuffle(freeRooms);
	// Two passes: first every case room that has a free room dressed for it takes the most specific
	// one; then the rest take what is left. (Undressed levels: the same order as a single pass.)
	auto take = [&](size_t i) -> const StageRoom*
	{
		const StageRoom* r = freeRooms[i];
		freeRooms.erase(freeRooms.begin() + static_cast<std::ptrdiff_t>(i));
		return r;
	};
	auto dressedFor = [&](int caseRoom) -> size_t
	{
		size_t best = freeRooms.size();
		for (size_t i = 0; i < freeRooms.size(); ++i)
		{
			if (suits(*freeRooms[i], caseLabel(caseRoom)) && (best == freeRooms.size() || freeRooms[i]->suits.size() < freeRooms[best]->suits.size()))
				best = i;
		}
		return best;
	};
	std::vector<int> waiting;
	size_t requiredLeft = required.size();
	for (const int caseRoom : required)
	{
		const size_t i = dressedFor(caseRoom);
		if (i < freeRooms.size()) { st.roomFor[caseRoom] = take(i)->id; --requiredLeft; }
		else waiting.push_back(caseRoom);
	}
	std::vector<int> optionalWaiting;
	for (const int caseRoom : optional)
	{
		const size_t i = dressedFor(caseRoom);
		// Never at the cost of a room a suspect or a weapon needs.
		if (i < freeRooms.size() && freeRooms.size() > requiredLeft) st.roomFor[caseRoom] = take(i)->id;
		else optionalWaiting.push_back(caseRoom);
	}
	for (const int caseRoom : waiting)
	{
		if (!freeRooms.empty())
			st.roomFor[caseRoom] = take(0)->id;
		else
			st.problems.push_back("case room " + std::to_string(caseRoom) + " is needed but the level has no room left for it");
	}
	for (const int caseRoom : optionalWaiting)
	{
		if (!freeRooms.empty()) st.roomFor[caseRoom] = take(0)->id;
	}

	auto levelRoom = [&](const std::string& id) -> const StageRoom*
	{
		for (const StageRoom& r : layout.rooms)
		{
			if (r.id == id) return &r;
		}
		return nullptr;
	};

	// ─── Searchable spots -> furniture (spots that hide items first, matching kinds preferred) ───
	std::set<std::string> usedFurniture;
	auto chooseFurniture = [&](const StageRoom& room, const std::string& kind, bool allowUsed) -> const StageFurniture*
	{
		std::vector<const StageFurniture*> matching, any, used;
		for (const StageFurniture& f : room.furniture)
		{
			if (usedFurniture.count(f.id))
			{
				used.push_back(&f);
				continue;
			}
			any.push_back(&f);
			if (!kind.empty() && f.kind == kind) matching.push_back(&f);
		}
		if (!matching.empty()) return rng.Pick(matching);
		if (!any.empty()) return rng.Pick(any);
		if (allowUsed && !used.empty()) return rng.Pick(used);
		return nullptr;
	};

	for (const auto& [caseRoom, roomId] : st.roomFor)
	{
		const StageRoom& room = *levelRoom(roomId);
		struct Pending { std::string spotId; int spotIdx; bool holdsItems; };
		std::vector<Pending> pending;
		int k = 0;
		for (const BlueprintSpot& sp : bp.spots)
		{
			if (sp.room != caseRoom) continue;
			pending.push_back({"spot-" + std::to_string(caseRoom) + "-" + std::to_string(k++), sp.spotIdx, sp.holdsItems});
		}
		std::stable_sort(pending.begin(), pending.end(), [](const Pending& a, const Pending& b) { return a.holdsItems && !b.holdsItems; });
		for (const Pending& p : pending)
		{
			const StageFurniture* f = chooseFurniture(room, SpotKinds()[p.spotIdx], false);
			if (!f)
			{
				if (p.holdsItems) st.problems.push_back(p.spotId + " hides items but " + roomId + " has no furniture left for it");
				continue;
			}
			usedFurniture.insert(f->id);
			st.spots.push_back(SpotPlacement{p.spotId, roomId, f->id});
		}
	}

	// ─── Evidence ───
	auto spotFurniture = [&](int caseRoom) -> const SpotPlacement*
	{
		int k = 0;
		for (const BlueprintSpot& sp : bp.spots)
		{
			if (sp.room != caseRoom) continue;
			const std::string id = "spot-" + std::to_string(caseRoom) + "-" + std::to_string(k++);
			if (!sp.holdsItems) continue;
			for (const SpotPlacement& placed : st.spots)
			{
				if (placed.spotId == id) return &placed;
			}
		}
		return nullptr;
	};
	auto hideIn = [&](EvidencePlacement& p, const std::string& roomId, const std::string& kind)
	{
		const StageRoom* room = levelRoom(roomId);
		const StageFurniture* f = room ? chooseFurniture(*room, kind, true) : nullptr;
		if (!f) return; // stays in the case file
		usedFurniture.insert(f->id);
		p.where = EvidencePlacement::Where::Furniture;
		p.roomId = roomId;
		p.furnitureId = f->id;
	};

	for (const BlueprintEvidence& e : bp.evidence)
	{
		EvidencePlacement p;
		p.evidenceId = e.id;
		p.late = bp.lateEvidenceId && *bp.lateEvidenceId == e.id;
		if (e.kind == "weapon")
		{
			if (const SpotPlacement* spot = spotFurniture(*e.foundPlaceIdx + 1))
			{
				p.where = EvidencePlacement::Where::Furniture;
				p.roomId = spot->roomId;
				p.furnitureId = spot->furnitureId;
			}
			else
			{
				st.problems.push_back("weapon " + e.id + " has nowhere to be found");
			}
		}
		else if (e.kind == "autopsy" || e.kind == "planted")
		{
			p.where = EvidencePlacement::Where::Body;
			p.roomId = st.sceneRoomId;
		}
		else if (e.kind == "diary")
		{
			hideIn(p, st.sceneRoomId, "desk");
		}
		else if (e.kind == "document")
		{
			// A motive letter turns up where its subject spends the evening, or at the scene.
			const auto it = st.roomFor.find(bp.suspects[*e.ref].placeIdx + 1);
			hideIn(p, it != st.roomFor.end() ? it->second : st.sceneRoomId, "");
		}
		st.evidence.push_back(p);
	}

	// ─── Suspects: in the room their alibi claims, or gathered at the scene if it is offstage ───
	std::map<std::string, int> standCount;
	for (const BlueprintSuspect& s : bp.suspects)
	{
		const auto it = st.roomFor.find(s.placeIdx + 1);
		const std::string roomId = it != st.roomFor.end() ? it->second : st.sceneRoomId;
		st.suspects.push_back(SuspectPlacement{s.id, roomId, standCount[roomId]++});
	}
	return st;
}

} // namespace mystery
