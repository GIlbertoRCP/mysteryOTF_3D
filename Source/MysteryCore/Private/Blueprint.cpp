#include "MysteryCore/Blueprint.h"

#include "MysteryCore/Content.h"
#include "MysteryCore/Random.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace mystery
{

namespace
{
	/** Clue kinds that can be held back and arrive later as a new lead. */
	const std::vector<std::string> LateKinds = {"witness", "camera", "phone", "smudge", "shoeprint", "trace", "scrapings"};
	const std::vector<std::string> SuspectColors = {"#e17055", "#74b9ff", "#55efc4", "#fdcb6e", "#a29bfe", "#fd79a8", "#81ecec", "#fab1a0"};

	struct DifficultyConfig
	{
		int suspects, verified, places, weapons, extraMotiveDocs;
		bool slip, frame;
		const char* prefer;
		double accomplice, planted, claim, late, modifier;
	};

	const DifficultyConfig& ConfigFor(const std::string& difficulty)
	{
		static const DifficultyConfig Novice{4, 1, 3, 3, 1, true, false, "max", 0, 0, 0, 0, 0};
		static const DifficultyConfig Detective{5, 2, 3, 3, 2, true, true, "min+1", 0, 0.4, 0.35, 0.6, 0.35};
		static const DifficultyConfig Mastermind{6, 2, 4, 4, 99, false, true, "min", 0.5, 0.5, 0.5, 0.8, 0.5};
		if (difficulty == "Novice") return Novice;
		if (difficulty == "Mastermind") return Mastermind;
		return Detective;
	}

	std::vector<int> Range(int n)
	{
		std::vector<int> out(n);
		for (int i = 0; i < n; ++i) out[i] = i;
		return out;
	}

	template <typename T>
	bool Contains(const std::vector<T>& items, const T& value)
	{
		return std::find(items.begin(), items.end(), value) != items.end();
	}

	template <typename T, typename F>
	std::vector<T> Filter(const std::vector<T>& items, F keep)
	{
		std::vector<T> out;
		for (const T& item : items)
		{
			if (keep(item)) out.push_back(item);
		}
		return out;
	}

	template <typename T>
	std::vector<T> Slice(const std::vector<T>& items, size_t begin, size_t end)
	{
		begin = std::min(begin, items.size());
		end = std::min(end, items.size());
		return begin < end ? std::vector<T>(items.begin() + begin, items.begin() + end) : std::vector<T>();
	}

	int Matching(const std::vector<BlueprintSuspect>& suspects, const BlueprintSuspect& killer,
		const std::vector<std::string>& dims, bool useAlibi)
	{
		int count = 0;
		for (const BlueprintSuspect& s : suspects)
		{
			if (useAlibi && s.verified) continue;
			bool all = true;
			for (const std::string& d : dims)
			{
				if (s.traits.at(d) != killer.traits.at(d)) { all = false; break; }
			}
			count += all;
		}
		return count;
	}

	std::optional<std::vector<std::string>> ChooseClueDims(SeededRandom& rng, const Content& content,
		const std::vector<BlueprintSuspect>& suspects, const BlueprintSuspect& killer, const std::string& prefer,
		const std::vector<std::string>& allowed)
	{
		std::vector<std::vector<std::string>> valid;
		const int n = static_cast<int>(allowed.size());
		const int all = static_cast<int>(suspects.size());
		for (int mask = 1; mask < (1 << n); ++mask)
		{
			std::vector<std::string> dims, leadDims, analystDims;
			for (int i = 0; i < n; ++i)
			{
				if (mask & (1 << i)) dims.push_back(allowed[i]);
			}
			for (const std::string& d : dims)
			{
				if (content.IsLeadDim(d)) leadDims.push_back(d);
				if (content.IsAnalystDim(d)) analystDims.push_back(d);
			}
			if (leadDims.empty() || analystDims.empty()) continue;
			// Together the clues must point to exactly one person...
			if (Matching(suspects, killer, dims, true) != 1) continue;
			// ...but neither player can get there alone.
			if (Matching(suspects, killer, leadDims, true) < 2) continue;
			if (Matching(suspects, killer, analystDims, false) < 2) continue;
			// Every clue has to rule somebody out, or it is just noise.
			bool noise = false;
			for (const std::string& d : dims)
			{
				if (Matching(suspects, killer, {d}, false) == all) { noise = true; break; }
			}
			if (noise) continue;
			valid.push_back(dims);
		}
		if (valid.empty()) return std::nullopt;

		size_t min = valid[0].size(), max = valid[0].size();
		for (const auto& v : valid)
		{
			min = std::min(min, v.size());
			max = std::max(max, v.size());
		}
		const std::string p = prefer;
		const size_t target = p == "max" ? max : p == "min+1" ? std::min(min + 1, max) : min;
		return rng.Pick(Filter(valid, [&](const std::vector<std::string>& v) { return v.size() == target; }));
	}

	Json OptInt(const std::optional<int>& v) { return v ? Json(*v) : Json(); }
} // namespace

// ─── Floor plan ───

Plan GeneratePlan(SeededRandom& rng, int roomCount, const std::string& layout)
{
	constexpr double RoomH = 5, CorridorH = 2, CarW = 7, CarH = 3.5;
	Plan plan;
	plan.layout = layout;

	if (layout == "linear")
	{
		// A train: cars in a row, the crime scene somewhere in the middle third.
		std::vector<int> order = rng.Shuffle(Filter(Range(roomCount), [](int i) { return i != 0; }));
		const int scenePos = rng.Int(1, std::max(1, roomCount - 2));
		order.insert(order.begin() + std::min<size_t>(scenePos, order.size()), 0);
		for (size_t pos = 0; pos < order.size(); ++pos)
		{
			PlanRoom r;
			r.idx = order[pos];
			r.x = static_cast<double>(pos) * (CarW + 0.6);
			r.y = 0;
			r.w = CarW;
			r.h = CarH;
			r.doorX = static_cast<double>(pos) * (CarW + 0.6) + CarW / 2;
			r.doorY = CarH / 2;
			r.walk = std::abs(static_cast<int>(pos) - scenePos) * 2 + (r.idx == 0 ? 0 : 1);
			plan.rooms.push_back(r);
		}
		plan.width = static_cast<double>(plan.rooms.size()) * (CarW + 0.6) - 0.6;
		plan.height = CarH;
	}
	else
	{
		// A building: rooms above and below a corridor, the crime scene at one end.
		const std::vector<int> others = rng.Shuffle(Filter(Range(roomCount), [](int i) { return i != 0; }));
		const size_t half = (others.size() + 1) / 2;
		std::vector<int> top = {0};
		for (const int i : Slice(others, 0, half)) top.push_back(i);
		const std::vector<int> bottom = Slice(others, half, others.size());
		auto place = [&](const std::vector<int>& row, double y, double doorY)
		{
			double x = 0;
			for (const int idx : row)
			{
				const int w = rng.Int(4, 6);
				PlanRoom r;
				r.idx = idx;
				r.x = x;
				r.y = y;
				r.w = w;
				r.h = RoomH;
				r.doorX = x + w / 2.0;
				r.doorY = doorY;
				plan.rooms.push_back(r);
				x += w;
			}
		};
		place(top, 0, RoomH);
		place(bottom, RoomH + CorridorH, RoomH + CorridorH);
		double width = 0;
		for (const PlanRoom& r : plan.rooms) width = std::max(width, r.x + r.w);
		const PlanRoom scene = *std::find_if(plan.rooms.begin(), plan.rooms.end(), [](const PlanRoom& r) { return r.idx == 0; });
		for (PlanRoom& r : plan.rooms)
		{
			if (r.idx == 0) continue;
			// Along the corridor at a brisk walk, plus a minute to get through the doors.
			r.walk = 1 + std::max(1, static_cast<int>(std::floor(std::abs(r.doorX - scene.doorX) / 2.5 + 0.5)));
		}
		plan.width = width;
		plan.height = RoomH * 2 + CorridorH;
		plan.corridor = PlanRect{0, RoomH, width, CorridorH};
	}
	std::stable_sort(plan.rooms.begin(), plan.rooms.end(), [](const PlanRoom& a, const PlanRoom& b) { return a.idx < b.idx; });
	return plan;
}

// ─── Blueprint ───

Blueprint BuildBlueprint(const Content& content, const std::string& difficulty, const std::string& themeKey,
	const std::string& seed)
{
	const std::string theme = content.themes.count(themeKey) ? themeKey : "gothic";
	const Theme& th = content.themes.at(theme);
	const DifficultyConfig& cfg = ConfigFor(difficulty);
	SeededRandom rng(seed + "|" + theme + "|" + difficulty);

	Blueprint bp;
	bp.seed = seed;
	bp.theme = theme;
	bp.difficulty = difficulty;
	bp.titleIdx = rng.Int(0, static_cast<int>(th.titles.size()) - 1);
	bp.victimIdx = rng.Int(0, static_cast<int>(th.victims.size()) - 1);
	bp.sceneIdx = rng.Int(0, static_cast<int>(th.scenes.size()) - 1);
	bp.victimAge = rng.Int(46, 74);
	bp.startMin = rng.Pick(std::vector<int>{21 * 60, 21 * 60 + 30, 22 * 60, 22 * 60 + 30});
	bp.witnessIdx = rng.Int(0, static_cast<int>(th.witnesses.size()) - 1);
	bp.cameraIdx = rng.Int(0, static_cast<int>(th.cameras.size()) - 1);
	bp.witnessMin = bp.startMin + rng.Int(6, 24);
	bp.cameraMin = bp.startMin + rng.Int(3, 26);
	bp.phoneMin = bp.startMin + rng.Int(4, 24);

	const int alibiCount = static_cast<int>(th.alibiPlaces.size());
	const std::vector<int> seedIdxs = Slice(rng.Shuffle(Range(static_cast<int>(th.suspects.size()))), 0, cfg.suspects);
	const std::vector<int> placeIdxs = Slice(rng.Shuffle(Range(alibiCount)), 0, cfg.places);
	const std::vector<int> motiveIdxs = rng.Shuffle(Range(static_cast<int>(content.motives.size())));
	const std::vector<int> bioIdxs = rng.Shuffle(Range(static_cast<int>(th.bioLines.size())));
	const std::vector<std::string> colors = rng.Shuffle(SuspectColors);

	// A complication for the case. Blackouts kill the cameras; storms wash out footprints.
	if (rng.Next() < cfg.modifier)
	{
		bp.modifier = rng.Pick(std::vector<std::string>{"blackout", "storm", "tampered"});
	}
	bp.powerCutMin = bp.startMin + rng.Int(1, 6);
	const std::vector<std::string> allowedDims = Filter(AllDims(), [&](const std::string& d)
	{
		return !(bp.modifier == "blackout" && d == "coat") && !(bp.modifier == "storm" && d == "footwear");
	});

	// Roll traits until the deduction puzzle is uniquely solvable and truly co-op.
	const int n = static_cast<int>(seedIdxs.size());
	auto traitCount = [&](const char* dim) { return static_cast<int>(content.traits.at(dim).size()); };
	std::optional<std::vector<std::string>> clueDims;
	for (int attempt = 0; attempt < 500 && !clueDims; ++attempt)
	{
		bp.killerIdx = rng.Int(0, cfg.suspects - 1);
		const std::vector<int> innocents = rng.Shuffle(Filter(Range(n), [&](int i) { return i != bp.killerIdx; }));
		const std::vector<int> verifiedSet = Slice(innocents, 0, cfg.verified);
		bp.suspects.clear();
		for (int i = 0; i < n; ++i)
		{
			// Same draw order as the TypeScript object literal.
			BlueprintSuspect s;
			s.seedIdx = seedIdxs[i];
			s.color = colors[i % colors.size()];
			s.bioIdx = bioIdxs[i % bioIdxs.size()];
			s.placeIdx = rng.Pick(placeIdxs);
			s.traits["height"] = rng.Int(0, traitCount("height") - 1);
			s.traits["coat"] = rng.Int(0, traitCount("coat") - 1);
			s.traits["hand"] = rng.Next() < 0.3 ? 0 : 1; // left-handers are rarer
			s.traits["footwear"] = rng.Int(0, traitCount("footwear") - 1);
			s.traits["habit"] = rng.Int(0, traitCount("habit") - 1);
			s.traits["voice"] = rng.Int(0, traitCount("voice") - 1);
			s.traits["blood"] = rng.Pick(std::vector<int>{0, 0, 1, 2, 3, 3, 3}); // A and O are the common types
			s.motiveIdx = motiveIdxs[i % motiveIdxs.size()];
			s.verified = Contains(verifiedSet, i);
			bp.suspects.push_back(std::move(s));
		}
		clueDims = ChooseClueDims(rng, content, bp.suspects, bp.suspects[bp.killerIdx], cfg.prefer, allowedDims);
	}
	bp.clueDims = clueDims ? *clueDims : allowedDims; // the fallback is practically unreachable

	// Ids are random so they never leak the answer.
	for (BlueprintSuspect& s : bp.suspects)
	{
		s.id = rng.Id("s");
		s.age = rng.Int(24, 70);
		s.motiveOptionId = rng.Id("m");
	}
	const int killerIdx = bp.killerIdx;
	const std::vector<int> innocentIdxs = Filter(Range(n), [&](int i) { return i != killerIdx; });

	// Mastermind twist: an unverified innocent swears they were with the killer.
	// Their own receipt puts them somewhere else, so both alibis collapse together.
	const std::vector<int> unverifiedInnocents = Filter(innocentIdxs, [&](int i) { return !bp.suspects[i].verified; });
	if (rng.Next() < cfg.accomplice && !unverifiedInnocents.empty())
	{
		bp.accompliceIdx = rng.Pick(unverifiedInnocents);
		bp.suspects[*bp.accompliceIdx].placeIdx = bp.suspects[killerIdx].placeIdx;
	}
	// The receipt comes from a place nobody claims, so no alibi check contradicts it.
	bp.receiptPlaceIdx = rng.Pick(Filter(Range(alibiCount), [&](int i) { return !Contains(placeIdxs, i); }));
	bp.receiptMin = bp.startMin + rng.Int(6, 24);

	// Floor plan and an exact time of death (the victim's broken watch).
	bp.plan = GeneratePlan(rng, alibiCount + 1, theme == "express" ? "linear" : "corridor");
	bp.todMin = bp.startMin + rng.Int(9, 21);

	// Above Novice, one cleared innocent is cleared by distance instead of by staff:
	// seen in their room a minute before the death, too far away to reach the scene.
	std::vector<int> verifiedIdxs;
	for (int i = 0; i < n; ++i)
	{
		if (bp.suspects[i].verified) verifiedIdxs.push_back(i);
	}
	if (difficulty != "Novice" && !verifiedIdxs.empty())
	{
		const std::vector<int> witnessChoices = Filter(Range(static_cast<int>(th.witnesses.size())), [&](int i) { return i != bp.witnessIdx; });
		const int suspectIdx = rng.Pick(verifiedIdxs);
		bp.sighting = Blueprint::Sighting{suspectIdx, rng.Pick(witnessChoices)};
	}

	// Twist: an item planted by the body to frame an innocent. The lab shows it was placed after the death.
	const std::vector<int> plantable = Filter(innocentIdxs, [&](int i) { return !bp.accompliceIdx || i != *bp.accompliceIdx; });
	if (rng.Next() < cfg.planted && !plantable.empty())
	{
		const int suspectIdx = rng.Pick(plantable);
		bp.planted = Blueprint::Planted{suspectIdx, rng.Int(0, static_cast<int>(content.plantedItems.size()) - 1)};
	}

	// Twist: a lone witness accuses an innocent whom a staff record places elsewhere.
	const std::vector<int> recordCleared = Filter(verifiedIdxs, [&](int i) { return !bp.sighting || i != bp.sighting->suspectIdx; });
	if (rng.Next() < cfg.claim && !recordCleared.empty())
	{
		const std::vector<int> free = Filter(Range(static_cast<int>(th.witnesses.size())), [&](int i)
		{
			return i != bp.witnessIdx && (!bp.sighting || i != bp.sighting->witnessIdx);
		});
		const int suspectIdx = rng.Pick(recordCleared);
		const int witnessIdx = !free.empty() ? rng.Pick(free) : rng.Int(0, static_cast<int>(th.witnesses.size()) - 1);
		bp.claim = Blueprint::Claim{suspectIdx, witnessIdx, bp.startMin + rng.Int(8, 22)};
	}

	// Interview flavour: one innocent may confirm the habit clue, the killer may frame someone.
	if (Contains(bp.clueDims, std::string("habit")))
	{
		bp.suspects[rng.Pick(innocentIdxs)].noticed = "true";
	}
	if (cfg.frame)
	{
		BlueprintSuspect& killer = bp.suspects[killerIdx];
		killer.noticed = "frame";
		killer.frameTargetIdx = rng.Pick(Filter(innocentIdxs, [&](int i) { return !bp.accompliceIdx || i != *bp.accompliceIdx; }));
	}

	// Weapons: the real one, a same-cause decoy when possible, then others.
	const int weaponCount = static_cast<int>(th.weapons.size());
	bp.trueWeaponIdx = rng.Int(0, weaponCount - 1);
	const std::string& trueCause = th.weapons[bp.trueWeaponIdx].cause;
	const std::vector<int> others = rng.Shuffle(Filter(Range(weaponCount), [&](int i) { return i != bp.trueWeaponIdx; }));
	const std::vector<int> sameCause = Slice(Filter(others, [&](int i) { return th.weapons[i].cause == trueCause; }), 0, 1);
	std::vector<int> decoys = sameCause;
	for (const int i : others)
	{
		if (!Contains(sameCause, i)) decoys.push_back(i);
	}
	decoys = Slice(decoys, 0, cfg.weapons - 1);
	std::vector<int> weaponPool = {bp.trueWeaponIdx};
	weaponPool.insert(weaponPool.end(), decoys.begin(), decoys.end());
	for (const int weaponIdx : rng.Shuffle(weaponPool))
	{
		bp.weapons.push_back(BlueprintWeapon{weaponIdx, rng.Id("w")});
	}

	// ─── Evidence ───
	std::vector<BlueprintEvidence> evidence;
	auto ev = [&](const std::string& role, const std::string& kind, std::optional<int> ref = std::nullopt,
		std::optional<int> foundPlaceIdx = std::nullopt) -> BlueprintEvidence&
	{
		evidence.push_back(BlueprintEvidence{rng.Id("ev"), role, kind, ref, foundPlaceIdx});
		return evidence.back();
	};
	const auto& dims = bp.clueDims;
	auto hasDim = [&](const char* d) { return Contains(dims, std::string(d)); };

	ev("lead", "diary");
	ev("analyst", "autopsy");
	if (hasDim("height")) ev("lead", "witness");
	if (hasDim("coat")) ev("lead", "camera");
	if (hasDim("hand")) ev("analyst", "smudge");
	if (hasDim("footwear")) ev("analyst", "shoeprint");
	if (hasDim("habit")) ev("analyst", "trace");
	if (hasDim("voice")) ev("lead", "phone");
	if (hasDim("blood")) ev("analyst", "scrapings");

	std::string alibiEvidenceId;
	std::vector<int> usedPlaces;
	for (const BlueprintSuspect& s : bp.suspects)
	{
		if (!Contains(usedPlaces, s.placeIdx)) usedPlaces.push_back(s.placeIdx);
	}
	for (const int placeIdx : usedPlaces)
	{
		const std::string id = ev("lead", "record", placeIdx).id;
		if (placeIdx == bp.suspects[killerIdx].placeIdx) alibiEvidenceId = id;
	}
	if (bp.sighting) ev("lead", "sighting", bp.sighting->suspectIdx);
	if (bp.planted) ev("analyst", "planted", bp.planted->suspectIdx);
	if (bp.claim) ev("lead", "claim", bp.claim->suspectIdx);

	// With an accomplice, the receipt is what actually breaks the alibi.
	if (bp.accompliceIdx) alibiEvidenceId = ev("lead", "receipt", *bp.accompliceIdx).id;

	std::vector<int> motiveDocSuspects = {killerIdx};
	for (const int i : Slice(rng.Shuffle(innocentIdxs), 0, cfg.extraMotiveDocs)) motiveDocSuspects.push_back(i);
	std::string motiveRole = rng.Next() < 0.5 ? "lead" : "analyst";
	for (const int idx : rng.Shuffle(motiveDocSuspects))
	{
		ev(motiveRole, "document", idx);
		motiveRole = motiveRole == "lead" ? "analyst" : "lead";
	}

	std::string weaponRole = rng.Next() < 0.5 ? "lead" : "analyst";
	for (const BlueprintWeapon& w : bp.weapons)
	{
		// The found place is drawn before the evidence id (argument evaluation order).
		const int foundPlaceIdx = rng.Pick(placeIdxs);
		ev(weaponRole, "weapon", w.weaponIdx, foundPlaceIdx);
		weaponRole = weaponRole == "lead" ? "analyst" : "lead";
	}

	const BlueprintWeapon& trueWeaponOption = *std::find_if(bp.weapons.begin(), bp.weapons.end(),
		[&](const BlueprintWeapon& w) { return w.weaponIdx == bp.trueWeaponIdx; });

	// Tampered evidence: one decoy is smeared to look like the murder weapon.
	if (bp.modifier == "tampered")
	{
		const int weaponIdx = rng.Pick(Filter(bp.weapons, [&](const BlueprintWeapon& w) { return w.weaponIdx != bp.trueWeaponIdx; })).weaponIdx;
		bp.tampered = Blueprint::Tampered{weaponIdx, rng.Int(0, static_cast<int>(content.fakeBlood.size()) - 1)};
	}

	// Two searchable spots per room. Recovered items sit in the first spot of the room they were found in.
	std::set<int> itemRooms;
	for (const BlueprintEvidence& e : evidence)
	{
		if (e.kind == "weapon") itemRooms.insert(*e.foundPlaceIdx + 1);
	}
	for (int room = 0; room <= alibiCount; ++room)
	{
		const std::vector<int> picks = Slice(rng.Shuffle(Range(static_cast<int>(content.spots.size()))), 0, 2);
		for (size_t k = 0; k < picks.size(); ++k)
		{
			BlueprintSpot spot;
			spot.room = room;
			spot.spotIdx = picks[k];
			spot.noteIdx = room == 0 ? rng.Int(0, static_cast<int>(content.spotScene.size()) - 1)
			                         : rng.Int(0, static_cast<int>(content.spotNothing.size()) - 1);
			spot.holdsItems = k == 0 && itemRooms.count(room) > 0;
			bp.spots.push_back(spot);
		}
	}

	// Twist: one physical clue arrives late, as a new lead a few minutes in.
	const std::vector<BlueprintEvidence> lateCandidates = Filter(evidence, [](const BlueprintEvidence& e) { return Contains(LateKinds, e.kind); });
	if (rng.Next() < cfg.late && !lateCandidates.empty())
	{
		bp.lateEvidenceId = rng.Pick(lateCandidates).id;
	}

	bp.slip = cfg.slip;
	bp.evidence = rng.Shuffle(evidence);
	const BlueprintSuspect& killer = bp.suspects[killerIdx];
	bp.solution.killerId = killer.id;
	bp.solution.motiveId = killer.motiveOptionId;
	bp.solution.weaponId = trueWeaponOption.optionId;
	bp.solution.alibiEvidenceId = alibiEvidenceId;
	if (bp.accompliceIdx) bp.solution.accompliceId = bp.suspects[*bp.accompliceIdx].id;
	return bp;
}

// ─── Lookups and JSON ───

const BlueprintEvidence* Blueprint::FindEvidence(const std::string& id) const
{
	for (const BlueprintEvidence& e : evidence)
	{
		if (e.id == id) return &e;
	}
	return nullptr;
}

int Blueprint::FindSuspect(const std::string& id) const
{
	for (size_t i = 0; i < suspects.size(); ++i)
	{
		if (suspects[i].id == id) return static_cast<int>(i);
	}
	return -1;
}

bool Blueprint::HasEvidenceKind(const std::string& kind) const
{
	for (const BlueprintEvidence& e : evidence)
	{
		if (e.kind == kind) return true;
	}
	return false;
}

Json Blueprint::ToJson() const
{
	Json j = Json::MakeObject();
	j.Set("seed", seed);
	j.Set("theme", theme);
	j.Set("difficulty", difficulty);
	j.Set("titleIdx", titleIdx);
	j.Set("victimIdx", victimIdx);
	j.Set("victimAge", victimAge);
	j.Set("sceneIdx", sceneIdx);
	j.Set("startMin", startMin);
	j.Set("witnessIdx", witnessIdx);
	j.Set("witnessMin", witnessMin);
	j.Set("cameraIdx", cameraIdx);
	j.Set("cameraMin", cameraMin);
	j.Set("phoneMin", phoneMin);
	j.Set("todMin", todMin);

	Json p = Json::MakeObject();
	p.Set("layout", plan.layout);
	p.Set("width", plan.width);
	p.Set("height", plan.height);
	Json rooms = Json::MakeArray();
	for (const PlanRoom& r : plan.rooms)
	{
		Json room = Json::MakeObject();
		room.Set("idx", r.idx);
		room.Set("x", r.x);
		room.Set("y", r.y);
		room.Set("w", r.w);
		room.Set("h", r.h);
		Json door = Json::MakeObject();
		door.Set("x", r.doorX);
		door.Set("y", r.doorY);
		room.Set("door", door);
		room.Set("walk", r.walk);
		rooms.Push(room);
	}
	p.Set("rooms", rooms);
	if (plan.corridor)
	{
		Json c = Json::MakeObject();
		c.Set("x", plan.corridor->x);
		c.Set("y", plan.corridor->y);
		c.Set("w", plan.corridor->w);
		c.Set("h", plan.corridor->h);
		p.Set("corridor", c);
	}
	else
	{
		p.Set("corridor", Json());
	}
	j.Set("plan", p);

	if (sighting)
	{
		Json s = Json::MakeObject();
		s.Set("suspectIdx", sighting->suspectIdx);
		s.Set("witnessIdx", sighting->witnessIdx);
		j.Set("sighting", s);
	}
	else j.Set("sighting", Json());
	if (planted)
	{
		Json s = Json::MakeObject();
		s.Set("suspectIdx", planted->suspectIdx);
		s.Set("itemIdx", planted->itemIdx);
		j.Set("planted", s);
	}
	else j.Set("planted", Json());
	if (claim)
	{
		Json s = Json::MakeObject();
		s.Set("suspectIdx", claim->suspectIdx);
		s.Set("witnessIdx", claim->witnessIdx);
		s.Set("min", claim->min);
		j.Set("claim", s);
	}
	else j.Set("claim", Json());
	j.Set("lateEvidenceId", lateEvidenceId ? Json(*lateEvidenceId) : Json());
	j.Set("modifier", modifier ? Json(*modifier) : Json());
	j.Set("powerCutMin", powerCutMin);
	if (tampered)
	{
		Json s = Json::MakeObject();
		s.Set("weaponIdx", tampered->weaponIdx);
		s.Set("fakeIdx", tampered->fakeIdx);
		j.Set("tampered", s);
	}
	else j.Set("tampered", Json());

	Json spotsJ = Json::MakeArray();
	for (const BlueprintSpot& s : spots)
	{
		Json o = Json::MakeObject();
		o.Set("room", s.room);
		o.Set("spotIdx", s.spotIdx);
		o.Set("noteIdx", s.noteIdx);
		o.Set("holdsItems", s.holdsItems);
		spotsJ.Push(o);
	}
	j.Set("spots", spotsJ);

	Json suspectsJ = Json::MakeArray();
	for (const BlueprintSuspect& s : suspects)
	{
		Json o = Json::MakeObject();
		o.Set("id", s.id);
		o.Set("seedIdx", s.seedIdx);
		o.Set("age", s.age);
		o.Set("color", s.color);
		o.Set("bioIdx", s.bioIdx);
		o.Set("placeIdx", s.placeIdx);
		Json traitsJ = Json::MakeObject();
		for (const auto& [dim, value] : s.traits) traitsJ.Set(dim, value);
		o.Set("traits", traitsJ);
		o.Set("motiveIdx", s.motiveIdx);
		o.Set("motiveOptionId", s.motiveOptionId);
		o.Set("verified", s.verified);
		o.Set("noticed", s.noticed);
		if (s.frameTargetIdx) o.Set("frameTargetIdx", *s.frameTargetIdx);
		suspectsJ.Push(o);
	}
	j.Set("suspects", suspectsJ);
	j.Set("killerIdx", killerIdx);
	Json dimsJ = Json::MakeArray();
	for (const std::string& d : clueDims) dimsJ.Push(d);
	j.Set("clueDims", dimsJ);
	j.Set("slip", slip);
	j.Set("accompliceIdx", OptInt(accompliceIdx));
	j.Set("receiptPlaceIdx", receiptPlaceIdx);
	j.Set("receiptMin", receiptMin);
	Json weaponsJ = Json::MakeArray();
	for (const BlueprintWeapon& w : weapons)
	{
		Json o = Json::MakeObject();
		o.Set("weaponIdx", w.weaponIdx);
		o.Set("optionId", w.optionId);
		weaponsJ.Push(o);
	}
	j.Set("weapons", weaponsJ);
	j.Set("trueWeaponIdx", trueWeaponIdx);
	Json evidenceJ = Json::MakeArray();
	for (const BlueprintEvidence& e : evidence)
	{
		Json o = Json::MakeObject();
		o.Set("id", e.id);
		o.Set("role", e.role);
		o.Set("kind", e.kind);
		if (e.ref) o.Set("ref", *e.ref);
		if (e.foundPlaceIdx) o.Set("foundPlaceIdx", *e.foundPlaceIdx);
		evidenceJ.Push(o);
	}
	j.Set("evidence", evidenceJ);
	Json sol = Json::MakeObject();
	sol.Set("killerId", solution.killerId);
	sol.Set("motiveId", solution.motiveId);
	sol.Set("weaponId", solution.weaponId);
	sol.Set("alibiEvidenceId", solution.alibiEvidenceId);
	if (solution.accompliceId) sol.Set("accompliceId", *solution.accompliceId);
	j.Set("solution", sol);
	return j;
}

} // namespace mystery
