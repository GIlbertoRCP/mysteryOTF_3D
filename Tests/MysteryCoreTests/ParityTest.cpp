// Parity test for the C++ case generator: regenerates every case in the golden file
// (exported from the web game by Tools/MysteryExport/export.mts) and checks that the
// blueprint and everything derived from it match the TypeScript engine exactly.
//
//   ParityTest <content.json> <golden.ndjson>

#include "MysteryCore/Blueprint.h"
#include "MysteryCore/Case.h"
#include "MysteryCore/Content.h"
#include "MysteryCore/Json.h"
#include "MysteryCore/Staging.h"

#include <algorithm>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

using namespace mystery;

namespace
{
	int failures = 0;
	int failedCases = 0;

	bool Check(const std::string& seed, const std::string& what, const Json& expected, const Json& actual)
	{
		const std::string diff = JsonDiff(expected, actual);
		if (diff.empty()) return true;
		if (++failures <= 25) std::printf("x %s %s\n  %s\n", seed.c_str(), what.c_str(), diff.c_str());
		return false;
	}

	int stagingProblems = 0;

	void StagingFail(const std::string& seed, const std::string& what)
	{
		if (++stagingProblems <= 25) std::printf("x %s staging: %s\n", seed.c_str(), what.c_str());
	}

	/** A five-room wing like L_Manor: a study that can hold the body, four other rooms. */
	StageLayout SampleLayout()
	{
		const std::vector<std::vector<std::string>> kinds = {
			{"desk", "bookshelf", "rug", "sofa"},
			{"cupboard", "window", "rug", "coatrack"},
			{"desk", "wastebasket", "sofa", "cupboard"},
			{"bookshelf", "window", "wastebasket", "rug"},
			{"sofa", "coatrack", "desk", "window"},
		};
		StageLayout layout;
		for (size_t r = 0; r < kinds.size(); ++r)
		{
			StageRoom room;
			room.id = "Room" + std::to_string(r);
			room.canBeScene = r == 0;
			room.standPoints = 2;
			for (size_t f = 0; f < kinds[r].size(); ++f)
			{
				room.furniture.push_back(StageFurniture{room.id + "_" + kinds[r][f] + std::to_string(f), kinds[r][f]});
			}
			layout.rooms.push_back(room);
		}
		return layout;
	}

	/** L_Train's six carriages: furniture kinds, the places each is dressed for, where the body can lie. */
	StageLayout TrainLayout()
	{
		struct Car { const char* id; bool scene; std::vector<std::string> kinds, suits; };
		const std::vector<Car> cars = {
			{"C1", true, {"sofa", "desk", "bookshelf", "window", "rug"}, {"Compartment 7", "Sleeping-Car Corridor"}},
			{"C2", false, {"sofa", "sofa", "desk", "wastebasket", "window"}, {"Dining Car"}},
			{"C3", false, {"coatrack", "desk", "sofa", "cupboard"}, {"Bar Car", "Smoking Salon"}},
			{"C4", false, {"bookshelf", "desk", "rug", "sofa", "cupboard"}, {"Library Car", "Smoking Salon"}},
			{"C5", true, {"cupboard", "window", "coatrack", "wastebasket", "sofa"}, {"Luggage Van", "Third-Class Carriage"}},
			{"C6", true, {"window", "sofa", "bookshelf", "desk"}, {"Observation Car", "Bar Car"}},
		};
		StageLayout layout;
		for (const Car& c : cars)
		{
			StageRoom room;
			room.id = c.id;
			room.canBeScene = c.scene;
			room.standPoints = 2;
			room.suits = c.suits;
			for (size_t f = 0; f < c.kinds.size(); ++f)
			{
				room.furniture.push_back(StageFurniture{room.id + "_" + c.kinds[f] + std::to_string(f), c.kinds[f]});
			}
			layout.rooms.push_back(room);
		}
		return layout;
	}

	/** L_Manor as built: the six rooms of the house (furniture as Scripts/build_manor.py lays it),
	 *  and on the grounds the conservatory and the chapel, which play only those places. */
	StageLayout ManorLayout()
	{
		struct Room { const char* id; bool scene; std::vector<std::string> kinds; };
		const std::vector<Room> house = {
			{"N1", false, {"cupboard", "coatrack", "desk", "rug"}},
			{"N2", true, {"bookshelf", "window", "desk", "sofa"}},
			{"N3", false, {"window", "bookshelf", "wastebasket", "sofa"}},
			{"S1", true, {"desk", "bookshelf", "wastebasket", "rug"}},
			{"S2", false, {"window", "cupboard", "coatrack", "sofa"}},
			{"S3", false, {"cupboard", "window", "desk", "rug"}},
		};
		StageLayout layout;
		for (const Room& h : house)
		{
			StageRoom room;
			room.id = h.id;
			room.canBeScene = h.scene;
			room.standPoints = 2;
			for (size_t f = 0; f < h.kinds.size(); ++f) room.furniture.push_back({room.id + "_" + h.kinds[f], h.kinds[f]});
			layout.rooms.push_back(room);
		}
		StageRoom conservatory;
		conservatory.id = "Conservatory";
		conservatory.canBeScene = true;
		conservatory.exclusive = true;
		conservatory.suits = {"Conservatory"};
		conservatory.standPoints = 2;
		for (const char* k : {"desk", "window", "cupboard", "sofa", "wastebasket"}) conservatory.furniture.push_back({std::string("Con_") + k, k});
		layout.rooms.push_back(conservatory);
		StageRoom chapel;
		chapel.id = "Chapel";
		chapel.exclusive = true;
		chapel.suits = {"Chapel"};
		chapel.standPoints = 3;
		for (const char* k : {"desk", "sofa", "cupboard", "window", "rug"}) chapel.furniture.push_back({std::string("Chp_") + k, k});
		layout.rooms.push_back(chapel);
		return layout;
	}

	int dressedCast = 0, castTotal = 0;
	int outbuildingCases = 0, conservatoryScenes = 0, chapelCases = 0, gothicCases = 0;

	/** Every case must be fair to play once staged: findable items, honest alibis, no double booking. */
	void CheckStaging(const Content& content, const Blueprint& bp, const StageLayout& layout, bool countDressing = true)
	{
		const Staging st = StageCase(content, bp, layout);
		for (const std::string& p : st.problems) StagingFail(bp.seed, p);
		for (const StageRoom& r : layout.rooms)
		{
			if (r.id == st.roomFor.at(0) && !r.canBeScene) StagingFail(bp.seed, "the scene is not a room that can hold the body");
		}
		// How often a place lands in a room dressed for it (only meaningful for dressed levels).
		const Theme& theme = content.themes.at(bp.theme);
		for (const auto& [caseRoom, roomId] : st.roomFor)
		{
			const std::string label = caseRoom == 0 ? theme.scenes[bp.sceneIdx].label.en : theme.alibiPlaces[caseRoom - 1].label.en;
			for (const StageRoom& r : layout.rooms)
			{
				if (!countDressing || r.id != roomId || r.suits.empty()) continue;
				++castTotal;
				dressedCast += std::find(r.suits.begin(), r.suits.end(), label) != r.suits.end();
			}
		}

		std::set<std::string> furniture;
		for (const SpotPlacement& sp : st.spots)
		{
			if (!furniture.insert(sp.furnitureId).second) StagingFail(bp.seed, sp.furnitureId + " holds two spots");
		}
		for (const BlueprintEvidence& e : bp.evidence)
		{
			const EvidencePlacement* p = st.FindEvidence(e.id);
			if (!p) { StagingFail(bp.seed, e.id + " not placed"); continue; }
			if (e.kind == "weapon")
			{
				const auto room = st.roomFor.find(*e.foundPlaceIdx + 1);
				if (p->where != EvidencePlacement::Where::Furniture || room == st.roomFor.end() || p->roomId != room->second)
					StagingFail(bp.seed, "weapon " + e.id + " is not in the room it was found in");
			}
			if ((e.kind == "autopsy" || e.kind == "planted") && p->where != EvidencePlacement::Where::Body)
				StagingFail(bp.seed, e.kind + " is not on the body");
			if (p->late != (bp.lateEvidenceId && *bp.lateEvidenceId == e.id)) StagingFail(bp.seed, "late lead flag wrong");
		}
		for (size_t i = 0; i < bp.suspects.size(); ++i)
		{
			const auto room = st.roomFor.find(bp.suspects[i].placeIdx + 1);
			if (room == st.roomFor.end() || st.suspects[i].roomId != room->second)
				StagingFail(bp.seed, bp.suspects[i].id + " does not stand where their alibi puts them");
		}
		const Staging again = StageCase(content, bp, layout);
		if (again.spots.size() != st.spots.size() || again.sceneRoomId != st.sceneRoomId || again.roomFor != st.roomFor)
			StagingFail(bp.seed, "not deterministic");
	}

	Json StringsJson(const std::vector<std::string>& items)
	{
		Json arr = Json::MakeArray();
		for (const std::string& s : items) arr.Push(s);
		return arr;
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("usage: ParityTest <content.json> <golden.ndjson>\n");
		return 2;
	}
	Content content;
	std::string error;
	if (!content.LoadFile(argv[1], &error))
	{
		std::printf("content: %s\n", error.c_str());
		return 2;
	}
	std::ifstream golden(argv[2], std::ios::binary);
	if (!golden)
	{
		std::printf("cannot open %s\n", argv[2]);
		return 2;
	}

	const auto start = std::chrono::steady_clock::now();
	int cases = 0;
	int dailies = 0;
	std::string line;
	while (std::getline(golden, line))
	{
		if (line.empty()) continue;
		Json g;
		if (!Json::Parse(line, g, &error))
		{
			std::printf("golden: %s\n", error.c_str());
			return 2;
		}

		if (g.Has("daily"))
		{
			for (const Json& d : g["daily"].AsArray())
			{
				const DailyCase dc = DailyCaseFor(content, d["date"].AsString());
				Json actual = Json::MakeObject();
				actual.Set("date", d["date"]);
				actual.Set("seed", dc.seed);
				actual.Set("theme", dc.theme);
				actual.Set("difficulty", dc.difficulty);
				Check(d["date"].AsString(), "dailyCase", d, actual);
				++dailies;
			}
			continue;
		}

		const std::string seed = g["seed"].AsString();
		const int before = failures;
		++cases;

		const Blueprint bp = BuildBlueprint(content, g["difficulty"].AsString(), g["theme"].AsString(), seed);
		const Json bpJson = bp.ToJson();
		Check(seed, "blueprint", g["blueprint"], bpJson);
		Check(seed, "determinism", bpJson, BuildBlueprint(content, g["difficulty"].AsString(), g["theme"].AsString(), seed).ToJson());

		for (const Lang lang : {Lang::En, Lang::Es})
		{
			const std::string code = lang == Lang::En ? "en" : "es";
			const Json& expect = g[code];
			Check(seed, "[" + code + "] mystery", expect["mystery"], Localize(content, bp, lang).ToJson());
			Check(seed, "[" + code + "] explanation", expect["explanation"], Json(ExplainSolution(content, bp, lang)));
			Check(seed, "[" + code + "] hints", expect["hints"], StringsJson(HintsFor(content, bp, lang)));

			Json story = Json::MakeArray();
			for (const StoryEntry& e : StoryFor(content, bp, lang))
			{
				Json o = Json::MakeObject();
				o.Set("min", e.min);
				o.Set("text", e.text);
				story.Push(o);
			}
			Check(seed, "[" + code + "] story", expect["story"], story);

			Json confront = Json::MakeArray();
			for (const BlueprintEvidence& e : bp.evidence)
			{
				Json row = Json::MakeArray();
				for (const BlueprintSuspect& s : bp.suspects)
				{
					const auto o = Confront(content, bp, s.id, e.id, lang);
					row.Push(o ? o->ToJson() : Json());
				}
				confront.Push(row);
			}
			Check(seed, "[" + code + "] confront", expect["confront"], confront);

			Json search = Json::MakeObject();
			for (const auto& [spotId, unused] : expect["search"].AsObject())
			{
				const auto r = SearchSpot(content, bp, spotId, lang);
				if (!r)
				{
					search.Set(spotId, Json());
					continue;
				}
				Json o = Json::MakeObject();
				o.Set("text", r->text);
				o.Set("found", StringsJson(r->found));
				search.Set(spotId, o);
			}
			Check(seed, "[" + code + "] search", expect["search"], search);
		}
		if (failures != before) ++failedCases;
		static const StageLayout layout = SampleLayout();
		CheckStaging(content, bp, layout);
		// Every clue that cracks a suspect breaks exactly one of their four statements, never "noticed".
		for (const BlueprintSuspect& sus : bp.suspects)
		{
			for (const BlueprintEvidence& ev : bp.evidence)
			{
				const auto o = Confront(content, bp, sus.id, ev.id, Lang::En);
				if (!o || !o->cracked) continue;
				int broken = 0;
				for (const char* st : {"where", "relation", "heard", "noticed"}) broken += EvaluateContradiction(content, bp, sus.id, st, ev.id);
				if (broken != 1 || EvaluateContradiction(content, bp, sus.id, "noticed", ev.id))
					StagingFail(bp.seed, ev.kind + " cracks " + sus.id + " but breaks " + std::to_string(broken) + " statements");
			}
		}
		if (bp.theme == "express")
		{
			static const StageLayout train = TrainLayout();
			CheckStaging(content, bp, train);
		}
		if (bp.theme == "gothic")
		{
			static const StageLayout manor = ManorLayout();
			CheckStaging(content, bp, manor, false);
			// Outbuildings play only their own places, and only when the case has them.
			const Staging st = StageCase(content, bp, manor);
			const Theme& theme = content.themes.at(bp.theme);
			bool usesOut = false;
			for (const auto& [caseRoom, roomId] : st.roomFor)
			{
				const std::string label = caseRoom == 0 ? theme.scenes[bp.sceneIdx].label.en : theme.alibiPlaces[caseRoom - 1].label.en;
				if ((roomId == "Conservatory" || roomId == "Chapel") && label != roomId)
					StagingFail(bp.seed, roomId + " cast as " + label);
				if (roomId == "Chapel") ++chapelCases;
				if (roomId == "Conservatory" && caseRoom == 0) ++conservatoryScenes;
				usesOut |= roomId == "Conservatory" || roomId == "Chapel";
			}
			outbuildingCases += usesOut;
			++gothicCases;
		}
	}

	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	if (stagingProblems)
	{
		std::printf("\nx %d staging problems\n", stagingProblems);
		return 1;
	}
	std::printf("staging: all %d cases fit the five-room wing fairly; express cases fit the train, %d of %d places in a carriage dressed for them\n",
		cases, dressedCast, castTotal);
	std::printf("staging: gothic cases fit the manor and its grounds; %d of %d use an outbuilding (%d with the body in the conservatory, the chapel cast in %d)\n",
		outbuildingCases, gothicCases, conservatoryScenes, chapelCases);
	if (failures)
	{
		std::printf("\nx %d mismatches in %d of %d cases (%d daily dates checked), %.1fs\n", failures, failedCases, cases, dailies, seconds);
		return 1;
	}
	std::printf("ok: %d cases match the TypeScript engine exactly in English and Spanish, plus %d daily dates (%.1fs)\n",
		cases, dailies, seconds);
	return 0;
}
