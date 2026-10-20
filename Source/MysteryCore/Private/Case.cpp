#include "MysteryCore/Case.h"

#include "MysteryCore/Content.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace mystery
{

namespace
{
	std::string LangCode(Lang lang) { return lang == Lang::En ? "en" : "es"; }

	/** Plain text in one language, without placeholder filling (the TypeScript `L`). */
	const std::string& L(const LText& text, Lang lang) { return text.Get(lang); }

	std::string Room(int idx) { return "room-" + std::to_string(idx); }

	uint32_t HashId(const std::string& id)
	{
		uint32_t h = 0;
		for (const char16_t c : ToUtf16(id)) h = h * 33u + c;
		return h;
	}

	/** Splits text into roughly equal pieces at word boundaries. */
	std::vector<std::string> Tear(const std::string& text, int count)
	{
		const std::vector<std::string> words = SplitSpaces(text);
		const size_t per = static_cast<size_t>(std::ceil(static_cast<double>(words.size()) / count));
		std::vector<std::string> pieces;
		for (size_t i = 0; i < words.size(); i += per)
		{
			const size_t end = std::min(words.size(), i + per);
			pieces.push_back(Join(std::vector<std::string>(words.begin() + i, words.begin() + end), " "));
		}
		return pieces;
	}

	/** Shared lookups for one blueprint. */
	struct Ctx
	{
		const Content& c;
		const Blueprint& bp;
		const Theme& th;
		Lang lang;

		Ctx(const Content& content, const Blueprint& blueprint, Lang l)
			: c(content), bp(blueprint), th(content.themes.at(blueprint.theme)), lang(l) {}

		const BlueprintSuspect& Killer() const { return bp.suspects[bp.killerIdx]; }
		const std::string& NameOf(const BlueprintSuspect& s) const { return th.suspects[s.seedIdx].name; }
		const std::string& NameOf(int idx) const { return NameOf(bp.suspects[idx]); }
		const Weapon& TrueWeapon() const { return th.weapons[bp.trueWeaponIdx]; }
		const LText& KillerClue(const std::string& dim) const { return c.traits.at(dim)[Killer().traits.at(dim)].clue; }
		const TraitValue& KillerTrait(const std::string& dim) const { return c.traits.at(dim)[Killer().traits.at(dim)]; }
		std::string F(const LText& text, const Params& params = Params()) const { return Fill(text, lang, params); }
		std::string T(const std::string& key, const Params& params = Params()) const { return Fill(c.Text(key), lang, params); }
		std::string Time(int min) const { return FormatTime(min, lang); }
		static Params With(const Params& base, const Params& extra) { Params p = base; p.Merge(extra); return p; }
	};

	std::optional<std::string> SpotIdFor(const Blueprint& bp, int room)
	{
		int k = 0;
		for (const BlueprintSpot& sp : bp.spots)
		{
			if (sp.room != room) continue;
			if (sp.holdsItems) return "spot-" + std::to_string(room) + "-" + std::to_string(k);
			++k;
		}
		return std::nullopt;
	}

	std::optional<Puzzle> MakePuzzle(const Ctx& x, const BlueprintEvidence& e, const std::string& details)
	{
		Puzzle p;
		if (e.kind == "smudge") { p.type = "hand"; p.answer = x.KillerTrait("hand").key; return p; }
		if (e.kind == "shoeprint") { p.type = "sole"; p.answer = x.KillerTrait("footwear").key; return p; }
		if (e.kind == "trace") { p.type = "uv"; p.key = x.KillerTrait("habit").key; return p; }
		if (e.kind == "planted") { p.type = "uv"; p.key = "planted"; return p; }
		if (e.kind == "scrapings") { p.type = "blood"; p.answer = x.KillerTrait("blood").key; return p; }
		if (e.kind == "diary") { p.type = "cipher"; p.shift = 3 + static_cast<int>(HashId(e.id) % 20); return p; }
		if (e.kind == "document" && HashId(e.id) % 2 == 0)
		{
			// About half the documents arrive torn up.
			p.type = "torn";
			p.pieces = Tear(details, 3);
			return p;
		}
		return std::nullopt;
	}

	Json OptionsJson(const std::vector<Option>& options)
	{
		Json arr = Json::MakeArray();
		for (const Option& o : options)
		{
			Json j = Json::MakeObject();
			j.Set("id", o.id);
			j.Set("label", o.label);
			arr.Push(j);
		}
		return arr;
	}

	Json StringsJson(const std::vector<std::string>& items)
	{
		Json arr = Json::MakeArray();
		for (const std::string& s : items) arr.Push(s);
		return arr;
	}

	Json QAJson(const QA& qa)
	{
		Json j = Json::MakeObject();
		j.Set("q", qa.q);
		j.Set("a", qa.a);
		return j;
	}
} // namespace

// ─── Localize ───

PublicMystery Localize(const Content& content, const Blueprint& bp, Lang lang)
{
	const Ctx x(content, bp, lang);
	const Theme& th = x.th;
	const Victim& victim = th.victims[bp.victimIdx];
	const Place& scene = th.scenes[bp.sceneIdx];
	const BlueprintSuspect& killer = x.Killer();
	const Weapon& trueWeapon = x.TrueWeapon();

	Params base;
	base.Set("victim", victim.name).Set("scene", scene.name).Set("window", FormatWindow(bp.startMin, lang));
	auto with = [&](const Params& extra) { return Ctx::With(base, extra); };

	PublicMystery m;
	m.caseId = "case-" + bp.seed;
	m.seed = bp.seed;
	m.theme = bp.theme;
	m.difficulty = bp.difficulty;
	m.lang = LangCode(lang);
	m.title = L(th.titles[bp.titleIdx], lang);

	// ─── Suspects ───
	for (size_t i = 0; i < bp.suspects.size(); ++i)
	{
		const BlueprintSuspect& s = bp.suspects[i];
		const SuspectSeed& seed = th.suspects[s.seedIdx];
		const Place& place = th.alibiPlaces[s.placeIdx];
		const Motive& motive = content.motives[s.motiveIdx];
		const bool isKiller = static_cast<int>(i) == bp.killerIdx;
		std::optional<int> partner;
		if (bp.accompliceIdx)
		{
			if (isKiller) partner = *bp.accompliceIdx;
			else if (static_cast<int>(i) == *bp.accompliceIdx) partner = bp.killerIdx;
		}

		std::string noticed = x.T("aNoticedNothing");
		if (s.noticed == "true")
		{
			noticed = x.T("aNoticedTrue", with(Params().Set("value", x.KillerClue("habit"))));
		}
		else if (s.noticed == "frame" && s.frameTargetIdx)
		{
			noticed = x.T("aNoticedFrame", with(Params().Set("other", x.NameOf(*s.frameTargetIdx))));
		}

		PublicSuspect ps;
		ps.id = s.id;
		ps.name = seed.name;
		ps.role = L(seed.role, lang);
		ps.age = s.age;
		ps.color = s.color;
		ps.bio = x.T("bio", Params().Set("role", seed.role).Set("line", x.F(th.bioLines[s.bioIdx], base)));
		ps.alibiPlace = L(place.label, lang);
		ps.alibiRoomId = Room(s.placeIdx + 1);
		ps.look = seed.look;
		ps.alibiStatement = x.T("alibiStatement", with(Params().Set("place", place.name)));
		for (const std::string& dim : AllDims())
		{
			const TraitValue& v = content.traits.at(dim)[s.traits.at(dim)];
			ps.traits[dim] = L(v.label, lang);
			ps.traitKeys[dim] = v.key;
		}
		ps.interview.push_back(QA{x.T("qWhere", base), partner
			? x.T("aWhereTogether", with(Params().Set("place", place.name).Set("other", x.NameOf(*partner))))
			: x.T("aWhere", with(Params().Set("place", place.name)))});
		ps.interview.push_back(QA{x.T("qRelation", base), x.F(motive.deflect, base)});
		ps.interview.push_back(QA{x.T("qHeard", base), isKiller && bp.slip
			? x.T("aHeardSlip", with(Params().Set("cause", content.causes.at(trueWeapon.cause).slipNoun)))
			: x.T("aHeardInnocent", base)});
		ps.interview.push_back(QA{x.T("qNoticed", base), noticed});
		m.suspects.push_back(std::move(ps));
	}

	// ─── Evidence ───
	for (const BlueprintEvidence& e : bp.evidence)
	{
		PublicEvidence pe;
		pe.id = e.id;
		pe.role = e.role;
		pe.kind = e.kind;

		const std::string scene0 = "room-0";
		auto event = [](int min, std::string room, std::optional<std::string> suspect = std::nullopt)
		{
			PublicEvidence::Event ev;
			ev.min = min;
			ev.roomId = std::move(room);
			ev.suspectId = std::move(suspect);
			return ev;
		};
		if (e.kind == "witness") pe.event = event(bp.witnessMin, scene0);
		else if (e.kind == "camera") pe.event = event(bp.cameraMin, scene0);
		else if (e.kind == "phone") pe.event = event(bp.phoneMin, scene0);
		else if (e.kind == "autopsy") pe.event = event(bp.todMin, scene0);
		else if (e.kind == "diary") pe.event = event(bp.startMin, scene0);
		else if (e.kind == "receipt") pe.event = event(bp.receiptMin, Room(bp.receiptPlaceIdx + 1), bp.suspects[*e.ref].id);
		else if (e.kind == "sighting") pe.event = event(bp.todMin - 1, Room(bp.suspects[*e.ref].placeIdx + 1), bp.suspects[*e.ref].id);
		else if (e.kind == "weapon") pe.event = event(-1, Room(*e.foundPlaceIdx + 1));
		else if (e.kind == "claim") pe.event = event(bp.claim->min, scene0, bp.suspects[*e.ref].id);

		std::string title, summary, details;
		if (e.kind == "diary")
		{
			title = x.T("diaryTitle", base);
			summary = x.T("diarySummary", base);
			details = x.T("diaryDetails", with(Params().Set("start", x.Time(bp.startMin))));
		}
		else if (e.kind == "autopsy")
		{
			const std::string cause = x.F(content.causes.at(trueWeapon.cause).autopsy, Params().Set("trace", trueWeapon.trace));
			title = x.T("autopsyTitle", base);
			summary = x.T("autopsySummary", base);
			details = x.T("autopsyDetails", with(Params().Set("cause", cause).Set("tod", x.Time(bp.todMin))));
		}
		else if (e.kind == "witness")
		{
			const Place& w = th.witnesses[bp.witnessIdx];
			const Params p = with(Params().Set("witness", w.name).Set("value", x.KillerClue("height")).Set("time", x.Time(bp.witnessMin)));
			title = x.T("witnessTitle", Params().Set("witness", w.label));
			summary = x.T("witnessSummary", p);
			details = x.T("witnessDetails", p);
		}
		else if (e.kind == "camera")
		{
			const LText& source = th.cameras[bp.cameraIdx];
			const Params p = with(Params().Set("source", source).Set("value", x.KillerClue("coat")).Set("time", x.Time(bp.cameraMin)));
			title = L(source, lang);
			summary = x.T("cameraSummary", p);
			details = x.T("cameraDetails", p);
		}
		else if (e.kind == "smudge")
		{
			title = x.T("smudgeTitle");
			summary = x.T("smudgeSummary");
			details = x.T("smudgeDetails", with(Params().Set("value", x.KillerClue("hand"))));
		}
		else if (e.kind == "shoeprint")
		{
			title = x.T("shoeprintTitle");
			summary = x.T("shoeprintSummary");
			details = x.T("shoeprintDetails", with(Params().Set("floor", th.floor).Set("value", x.KillerClue("footwear"))));
		}
		else if (e.kind == "phone")
		{
			const Params p = with(Params().Set("source", th.phone).Set("value", x.KillerClue("voice")).Set("time", x.Time(bp.phoneMin)));
			title = L(th.phone, lang);
			summary = x.T("phoneSummary", p);
			details = x.T("phoneDetails", p);
		}
		else if (e.kind == "scrapings")
		{
			title = x.T("scrapingsTitle");
			summary = x.T("scrapingsSummary");
			details = x.T("scrapingsDetails", with(Params().Set("value", x.KillerClue("blood"))));
		}
		else if (e.kind == "trace")
		{
			title = x.T("traceTitle");
			summary = x.T("traceSummary");
			details = x.T("traceDetails", with(Params().Set("value", x.KillerClue("habit"))));
		}
		else if (e.kind == "record")
		{
			const Place& place = th.alibiPlaces[*e.ref];
			std::vector<std::string> confirmed;
			for (size_t i = 0; i < bp.suspects.size(); ++i)
			{
				const BlueprintSuspect& s = bp.suspects[i];
				if (s.verified && s.placeIdx == *e.ref && !(bp.sighting && static_cast<int>(i) == bp.sighting->suspectIdx))
					confirmed.push_back(x.NameOf(s));
			}
			const Params p = with(Params().Set("place", place.name).Set("names", JoinNames(confirmed, lang)));
			title = x.T("recordTitle", Params().Set("place", place.label));
			summary = x.T("recordSummary", p);
			details = x.T(confirmed.empty() ? "recordEmpty" : "recordWithNames", p);
			if (bp.accompliceIdx && *e.ref == killer.placeIdx)
			{
				details += x.T("recordVouch", Params().Set("accomplice", x.NameOf(*bp.accompliceIdx)).Set("killer", x.NameOf(killer)));
			}
		}
		else if (e.kind == "sighting")
		{
			const BlueprintSuspect& s = bp.suspects[*e.ref];
			const Place& w = th.witnesses[bp.sighting->witnessIdx];
			const Params p = with(Params().Set("witness", w.name).Set("name", x.NameOf(s))
				.Set("place", th.alibiPlaces[s.placeIdx].name).Set("time", x.Time(bp.todMin - 1)));
			title = x.T("sightingTitle", Params().Set("witness", w.label).Set("name", x.NameOf(s)));
			summary = x.T("sightingSummary", p);
			details = x.T("sightingDetails", p);
		}
		else if (e.kind == "planted")
		{
			const BlueprintSuspect& s = bp.suspects[*e.ref];
			const PlantedItem& item = content.plantedItems[bp.planted->itemIdx];
			const Params p = with(Params().Set("item", item.name).Set("itemCap", item.cap).Set("itemFull", item.full)
				.Set("initials", InitialsOf(x.NameOf(s))));
			title = x.T("plantedTitle", p);
			summary = x.T("plantedSummary", p);
			details = x.T("plantedDetails", p);
		}
		else if (e.kind == "claim")
		{
			const BlueprintSuspect& s = bp.suspects[*e.ref];
			const Place& w = th.witnesses[bp.claim->witnessIdx];
			const Params p = with(Params().Set("witness", w.name).Set("name", x.NameOf(s)).Set("time", x.Time(bp.claim->min)));
			title = x.T("claimTitle", Params().Set("witness", w.label).Set("name", x.NameOf(s)));
			summary = x.T("claimSummary", p);
			details = x.T("claimDetails", p);
		}
		else if (e.kind == "receipt")
		{
			const Place& place = th.alibiPlaces[bp.receiptPlaceIdx];
			const Params p = with(Params().Set("place", place.name).Set("name", x.NameOf(*e.ref)).Set("time", x.Time(bp.receiptMin)));
			title = x.T("receiptTitle", Params().Set("place", place.label));
			summary = x.T("receiptSummary", p);
			details = x.T("receiptDetails", p);
		}
		else if (e.kind == "document")
		{
			const BlueprintSuspect& s = bp.suspects[*e.ref];
			const Motive& motive = content.motives[s.motiveIdx];
			const Params p = with(Params().Set("name", x.NameOf(s)));
			title = x.F(motive.docTitle, p);
			summary = x.F(motive.docSummary, p);
			details = x.F(motive.doc, p);
		}
		else if (e.kind == "weapon")
		{
			const Weapon& w = th.weapons[*e.ref];
			const Params p = with(Params().Set("weapon", w.label).Set("place", th.alibiPlaces[*e.foundPlaceIdx].name).Set("desc", w.desc));
			title = x.T("weaponTitle", p);
			summary = x.T("weaponSummary", p);
			details = x.T("weaponDetails", p);
			if (bp.tampered && bp.tampered->weaponIdx == *e.ref)
			{
				details += " " + x.F(content.tamperedNote, Params().Set("fake", content.fakeBlood[bp.tampered->fakeIdx]));
			}
		}
		pe.title = title;
		pe.summary = summary;
		pe.details = details;
		pe.puzzle = MakePuzzle(x, e, details);

		if (e.kind == "weapon")
		{
			pe.weapon = PublicEvidence::WeaponInfo{th.weapons[*e.ref].key,
				*e.ref == bp.trueWeaponIdx || (bp.tampered && *e.ref == bp.tampered->weaponIdx)};
			pe.spotId = SpotIdFor(bp, *e.foundPlaceIdx + 1);
		}
		if (e.kind == "autopsy")
		{
			EvidenceModel model;
			model.type = "watch";
			model.min = bp.todMin;
			model.engraving = InitialsOf(victim.name);
			pe.model = model;
		}
		else if (e.kind == "planted")
		{
			static const char* Items[] = {"handkerchief", "glove", "cufflink", "case"};
			EvidenceModel model;
			model.type = "monogram";
			model.item = Items[bp.planted->itemIdx];
			model.initials = InitialsOf(x.NameOf(*e.ref));
			pe.model = model;
		}
		else if (e.kind == "document")
		{
			EvidenceModel model;
			model.type = "letter";
			model.seal = bp.suspects[*e.ref].color;
			pe.model = model;
		}
		if (e.kind == "record")
		{
			std::vector<std::string> ids;
			for (size_t i = 0; i < bp.suspects.size(); ++i)
			{
				const BlueprintSuspect& s = bp.suspects[i];
				if (s.verified && s.placeIdx == *e.ref && !(bp.sighting && static_cast<int>(i) == bp.sighting->suspectIdx))
					ids.push_back(s.id);
			}
			pe.confirms = ids;
		}
		m.evidence.push_back(std::move(pe));
	}

	// ─── Options, plan, briefing ───
	for (const BlueprintSuspect& s : bp.suspects)
	{
		m.motiveOptions.push_back(Option{s.motiveOptionId, L(content.motives[s.motiveIdx].label, lang)});
	}
	const std::vector<int>& rank = content.motiveRank[lang == Lang::En ? 0 : 1];
	std::vector<std::pair<int, Option>> ranked;
	for (size_t i = 0; i < bp.suspects.size(); ++i)
	{
		ranked.emplace_back(rank[bp.suspects[i].motiveIdx], m.motiveOptions[i]);
	}
	std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	m.motiveOptions.clear();
	for (const auto& [r, o] : ranked) m.motiveOptions.push_back(o);
	for (const BlueprintWeapon& w : bp.weapons)
	{
		m.weaponOptions.push_back(Option{w.optionId, L(th.weapons[w.weaponIdx].label, lang)});
	}
	m.accompliceOption = bp.accompliceIdx.has_value();

	m.victim.name = victim.name;
	m.victim.age = bp.victimAge;
	m.victim.occupation = L(victim.occupation, lang);
	m.victim.scene = L(scene.label, lang);
	m.victim.timeWindow = x.Time(bp.startMin) + " \xE2\x80\x93 " + x.Time(bp.startMin + 30); // en dash
	m.victim.windowStart = bp.startMin;
	m.victim.windowEnd = bp.startMin + 30;

	m.plan.layout = bp.plan.layout;
	m.plan.width = bp.plan.width;
	m.plan.height = bp.plan.height;
	m.plan.corridor = bp.plan.corridor;
	for (const PlanRoom& r : bp.plan.rooms)
	{
		PublicPlanRoom room;
		room.id = Room(r.idx);
		room.label = r.idx == 0 ? L(scene.label, lang) : L(th.alibiPlaces[r.idx - 1].label, lang);
		room.kind = r.idx == 0 ? "scene" : "room";
		room.x = r.x;
		room.y = r.y;
		room.w = r.w;
		room.h = r.h;
		room.doorX = r.doorX;
		room.doorY = r.doorY;
		room.walk = r.walk;
		int k = 0;
		for (const BlueprintSpot& sp : bp.spots)
		{
			if (sp.room != r.idx) continue;
			room.spots.emplace_back("spot-" + std::to_string(r.idx) + "-" + std::to_string(k++), L(content.spots[sp.spotIdx].label, lang));
		}
		m.plan.rooms.push_back(std::move(room));
	}

	if (bp.modifier)
	{
		const Modifier& mod = content.modifiers.at(*bp.modifier);
		m.modifier = PublicMystery::ModifierInfo{*bp.modifier, L(mod.label, lang),
			x.F(mod.note, Params().Set("time", x.Time(bp.powerCutMin)))};
	}
	m.synopsis = x.F(th.synopsis, with(Params().Set("occupation", victim.occupation)
		.Set("count", std::to_string(bp.suspects.size()))));
	return m;
}

// ─── Explanation, hints ───

std::string ExplainSolution(const Content& content, const Blueprint& bp, Lang lang)
{
	const Ctx x(content, bp, lang);
	const BlueprintSuspect& killer = x.Killer();
	std::string text = x.T("explanation", Params()
		.Set("killer", x.NameOf(killer))
		.Set("victim", x.th.victims[bp.victimIdx].name)
		.Set("weapon", x.TrueWeapon().name)
		.Set("motive", LowerFirst(content.motives[killer.motiveIdx].label.Get(lang)))
		.Set("place", x.th.alibiPlaces[killer.placeIdx].name));
	if (!bp.accompliceIdx) return text;
	return text + x.T("explanationAccomplice", Params()
		.Set("accomplice", x.NameOf(*bp.accompliceIdx))
		.Set("receiptPlace", x.th.alibiPlaces[bp.receiptPlaceIdx].name));
}

std::vector<std::string> HintsFor(const Content& content, const Blueprint& bp, Lang lang)
{
	static const std::map<std::string, std::string> ClueKind = {{"height", "witness"}, {"coat", "camera"},
		{"voice", "phone"}, {"hand", "smudge"}, {"footwear", "shoeprint"}, {"habit", "trace"}, {"blood", "scrapings"}};
	const Ctx x(content, bp, lang);
	const PublicMystery m = Localize(content, bp, lang);
	const BlueprintSuspect& killer = x.Killer();
	std::vector<std::string> hints;

	std::vector<std::string> verified;
	for (const BlueprintSuspect& s : bp.suspects)
	{
		if (s.verified) verified.push_back(x.NameOf(s));
	}
	hints.push_back(!verified.empty() ? x.T("hintAlibi", Params().Set("names", JoinNames(verified, lang))) : x.T("hintAlibiNone"));

	std::vector<std::string> decoys;
	for (const BlueprintWeapon& w : bp.weapons)
	{
		if (w.weaponIdx != bp.trueWeaponIdx) decoys.push_back(x.th.weapons[w.weaponIdx].name.Get(lang));
	}
	const std::string orWord = lang == Lang::Es ? " ni " : " or ";
	std::string decoyNames;
	if (decoys.size() > 1)
	{
		decoyNames = Join(std::vector<std::string>(decoys.begin(), decoys.end() - 1), ", ") + orWord + decoys.back();
	}
	else
	{
		decoyNames = Join(decoys, "");
	}
	hints.push_back(x.T("hintWeapon", Params().Set("names", decoyNames)));

	// Then the physical clues, strongest first, each against who is still in play.
	std::vector<int> remaining;
	for (size_t i = 0; i < bp.suspects.size(); ++i)
	{
		if (!bp.suspects[i].verified) remaining.push_back(static_cast<int>(i));
	}
	std::vector<std::string> dims = bp.clueDims;
	while (hints.size() < 4 && !dims.empty())
	{
		auto ruledOut = [&](const std::string& d)
		{
			int n = 0;
			for (const int i : remaining) n += bp.suspects[i].traits.at(d) != killer.traits.at(d);
			return n;
		};
		std::stable_sort(dims.begin(), dims.end(), [&](const std::string& a, const std::string& b) { return ruledOut(a) > ruledOut(b); });
		const std::string dim = dims.front();
		dims.erase(dims.begin());
		std::vector<std::string> out;
		std::vector<int> still;
		for (const int i : remaining)
		{
			if (bp.suspects[i].traits.at(dim) != killer.traits.at(dim)) out.push_back(x.NameOf(i));
			else still.push_back(i);
		}
		if (out.empty()) continue;
		std::string title;
		for (const PublicEvidence& e : m.evidence)
		{
			if (e.kind == ClueKind.at(dim)) { title = e.title; break; }
		}
		hints.push_back(x.T("hintClue", Params().Set("title", title).Set("names", JoinNames(out, lang))));
		remaining = still;
	}
	if (hints.size() < 4) hints.push_back(x.T(bp.slip ? "hintSlip" : "hintMotive"));
	if (hints.size() > 4) hints.resize(4);
	return hints;
}

// ─── Confront, search, story ───

std::optional<ConfrontOutcome> Confront(const Content& content, const Blueprint& bp, const std::string& suspectId,
	const std::string& evidenceId, Lang lang)
{
	const int sIdx = bp.FindSuspect(suspectId);
	const BlueprintEvidence* e = bp.FindEvidence(evidenceId);
	if (sIdx < 0 || !e) return std::nullopt;
	const Ctx x(content, bp, lang);
	const BlueprintSuspect& s = bp.suspects[sIdx];
	const BlueprintSuspect& killer = x.Killer();
	Params base;
	base.Set("victim", x.th.victims[bp.victimIdx].name).Set("scene", x.th.scenes[bp.sceneIdx].name);
	auto with = [&](const Params& extra) { return Ctx::With(base, extra); };
	auto fu = [&](const std::string& key, const Params& p) { return Fill(content.FollowUp(key), lang, p); };

	// What an innocent remembers once they open up: a true detail about the killer.
	std::vector<std::string> seenDims;
	for (const std::string& d : bp.clueDims)
	{
		if (d != "blood") seenDims.push_back(d);
	}
	auto seen = [&]() -> std::string
	{
		const size_t slot = static_cast<size_t>(sIdx) % std::max<size_t>(1, seenDims.size());
		if (slot >= seenDims.size()) return fu("aHeard", base);
		const std::string& dim = seenDims[slot];
		return Fill(content.FollowUpSeen(dim), lang, with(Params().Set("value", x.KillerClue(dim))));
	};
	auto frame = [&]()
	{
		const int target = killer.frameTargetIdx ? *killer.frameTargetIdx : (bp.killerIdx == 0 ? 1 : 0);
		return fu("aFrame", with(Params().Set("other", x.NameOf(target))));
	};
	auto refIs = [&](int idx) { return e->ref && *e->ref == idx; };

	ConfrontOutcome o;
	if (e->kind == "document" && refIs(sIdx))
	{
		o.cracked = true;
		o.kind = "admit";
		o.line = x.F(content.motives[s.motiveIdx].admit, base);
		o.followUp = QA{fu("qMore", base), sIdx == bp.killerIdx ? frame() : seen()};
	}
	else if (e->kind == "receipt" && refIs(sIdx) && bp.accompliceIdx && *bp.accompliceIdx == sIdx)
	{
		o.cracked = true;
		o.kind = "confess";
		o.line = x.T("confrontAccomplice", with(Params().Set("killer", x.NameOf(killer))
			.Set("receiptPlace", x.th.alibiPlaces[bp.receiptPlaceIdx].name)));
		o.followUp = QA{fu("qWhy", base), fu("aWhy", with(Params().Set("killer", x.NameOf(killer))))};
	}
	else if (e->kind == "sighting" && refIs(sIdx) && bp.sighting)
	{
		o.cracked = true;
		o.kind = "cleared";
		o.line = x.T("confrontSighting", with(Params().Set("place", x.th.alibiPlaces[s.placeIdx].name)
			.Set("witness", x.th.witnesses[bp.sighting->witnessIdx].name)));
		o.followUp = QA{fu("qWho", base), seen()};
	}
	else if (e->kind == "planted" && refIs(sIdx))
	{
		o.cracked = true;
		o.kind = "cleared";
		o.line = x.T("confrontPlanted", base);
		o.followUp = QA{fu("qWho", base), seen()};
	}
	else if (e->kind == "claim" && refIs(sIdx))
	{
		o.cracked = true;
		o.kind = "cleared";
		o.line = x.T("confrontClaim", with(Params().Set("place", x.th.alibiPlaces[s.placeIdx].name)));
		o.followUp = QA{fu("qWho", base), seen()};
	}
	else if (e->kind == "autopsy" && sIdx == bp.killerIdx && bp.slip)
	{
		o.cracked = true;
		o.kind = "slip";
		o.line = x.T("confrontSlip", base);
		o.followUp = QA{fu("qHeard", base), fu("aHeard", base)};
	}
	else
	{
		static const char* Variants[] = {"deflect0", "deflect1", "deflect2"};
		o.cracked = false;
		o.kind = "deflect";
		o.line = x.T(Variants[(static_cast<size_t>(sIdx) + JsLength(evidenceId)) % 3], base);
	}
	return o;
}

std::optional<SearchResult> SearchSpot(const Content& content, const Blueprint& bp, const std::string& spotId, Lang lang)
{
	// /^spot-(\d+)-(\d+)$/
	const std::string prefix = "spot-";
	if (spotId.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
	const size_t dash = spotId.find('-', prefix.size());
	auto digits = [](const std::string& s) { return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos; };
	if (dash == std::string::npos) return std::nullopt;
	const std::string roomStr = spotId.substr(prefix.size(), dash - prefix.size());
	const std::string kStr = spotId.substr(dash + 1);
	if (!digits(roomStr) || !digits(kStr)) return std::nullopt;
	const int room = std::atoi(roomStr.c_str());
	const int k = std::atoi(kStr.c_str());

	const BlueprintSpot* spot = nullptr;
	int n = 0;
	for (const BlueprintSpot& sp : bp.spots)
	{
		if (sp.room != room) continue;
		if (n++ == k) { spot = &sp; break; }
	}
	if (!spot) return std::nullopt;
	const Theme& th = content.themes.at(bp.theme);

	SearchResult result;
	if (spot->holdsItems)
	{
		std::vector<std::string> names;
		for (const BlueprintEvidence& e : bp.evidence)
		{
			if (e.kind == "weapon" && *e.foundPlaceIdx + 1 == room)
			{
				names.push_back(th.weapons[*e.ref].name.Get(lang));
				result.found.push_back(e.id);
			}
		}
		const std::string list = names.size() == 1 ? names[0] : Join(names, lang == Lang::Es ? " y " : " and ");
		result.text = Fill(content.spotFound, lang, Params().Set("spot", content.spots[spot->spotIdx].name).Set("weapon", list));
	}
	else
	{
		result.text = Fill((room == 0 ? content.spotScene : content.spotNothing)[spot->noteIdx], lang);
	}
	return result;
}

std::vector<StoryEntry> StoryFor(const Content& content, const Blueprint& bp, Lang lang)
{
	const Ctx x(content, bp, lang);
	const Theme& th = x.th;
	const BlueprintSuspect& killer = x.Killer();
	Params base;
	base.Set("victim", th.victims[bp.victimIdx].name)
		.Set("scene", th.scenes[bp.sceneIdx].name)
		.Set("killer", x.NameOf(killer))
		.Set("claimed", th.alibiPlaces[killer.placeIdx].name)
		.Set("weapon", x.TrueWeapon().name);

	struct Beat { int min; std::string key; Params params; };
	std::vector<Beat> ev;
	ev.push_back({bp.startMin, "meeting", {}});
	if (bp.modifier == "blackout") ev.push_back({bp.powerCutMin, "powerCut", {}});
	ev.push_back({bp.todMin - 6, "leaves", {}});
	if (bp.HasEvidenceKind("witness")) ev.push_back({bp.witnessMin, "witness", Params().Set("witness", th.witnesses[bp.witnessIdx].name)});
	if (bp.HasEvidenceKind("camera")) ev.push_back({bp.cameraMin, "camera", Params().Set("source", th.cameras[bp.cameraIdx])});
	if (bp.HasEvidenceKind("phone")) ev.push_back({bp.phoneMin, "phone", {}});
	if (bp.sighting)
	{
		const BlueprintSuspect& s = bp.suspects[bp.sighting->suspectIdx];
		ev.push_back({bp.todMin - 1, "sighting", Params().Set("innocent", x.NameOf(s))
			.Set("place", th.alibiPlaces[s.placeIdx].name).Set("witness", th.witnesses[bp.sighting->witnessIdx].name)});
	}
	ev.push_back({bp.todMin, "murder", {}});
	for (const BlueprintEvidence& e : bp.evidence)
	{
		if (e.kind == "weapon" && *e.ref == bp.trueWeaponIdx)
		{
			ev.push_back({bp.todMin + 3, "hides", Params().Set("place", th.alibiPlaces[*e.foundPlaceIdx].name)});
			break;
		}
	}
	if (bp.tampered)
	{
		ev.push_back({bp.todMin + 4, "tampers", Params().Set("fake", content.fakeBlood[bp.tampered->fakeIdx])
			.Set("decoy", th.weapons[bp.tampered->weaponIdx].name)});
	}
	if (bp.planted)
	{
		ev.push_back({bp.todMin + 5, "plants", Params().Set("innocent", x.NameOf(bp.planted->suspectIdx))
			.Set("item", content.plantedItems[bp.planted->itemIdx].name)});
	}
	if (bp.accompliceIdx)
	{
		ev.push_back({bp.receiptMin, "receipt", Params().Set("accomplice", x.NameOf(*bp.accompliceIdx))
			.Set("place", th.alibiPlaces[bp.receiptPlaceIdx].name)});
		ev.push_back({bp.todMin + 9, "accomplice", Params().Set("accomplice", x.NameOf(*bp.accompliceIdx))});
	}
	ev.push_back({bp.todMin + 8, "returns", {}});
	if (bp.claim)
	{
		ev.push_back({bp.startMin + 45, "claim", Params().Set("innocent", x.NameOf(bp.claim->suspectIdx))
			.Set("witness", th.witnesses[bp.claim->witnessIdx].name)});
	}
	std::stable_sort(ev.begin(), ev.end(), [](const Beat& a, const Beat& b) { return a.min < b.min; });

	std::vector<StoryEntry> story;
	for (const Beat& b : ev)
	{
		story.push_back(StoryEntry{b.min, Fill(content.Story(b.key), lang, Ctx::With(base, b.params))});
	}
	return story;
}

DailyCase DailyCaseFor(const Content& content, const std::string& date)
{
	uint32_t h = 0;
	for (const char16_t c : ToUtf16(date)) h = h * 31u + c;
	return DailyCase{"DAILY-" + date, content.themeKeys[h % content.themeKeys.size()], "Detective"};
}

// ─── JSON (same shape as the TypeScript objects, for parity checks) ───

Json ConfrontOutcome::ToJson() const
{
	Json j = Json::MakeObject();
	j.Set("cracked", cracked);
	j.Set("kind", kind);
	j.Set("line", line);
	if (followUp) j.Set("followUp", QAJson(*followUp));
	return j;
}

Json PublicMystery::ToJson() const
{
	Json j = Json::MakeObject();
	j.Set("caseId", caseId);
	j.Set("seed", seed);
	j.Set("theme", theme);
	j.Set("difficulty", difficulty);
	j.Set("lang", lang);
	j.Set("title", title);

	Json v = Json::MakeObject();
	v.Set("name", victim.name);
	v.Set("age", victim.age);
	v.Set("occupation", victim.occupation);
	v.Set("scene", victim.scene);
	v.Set("timeWindow", victim.timeWindow);
	v.Set("windowStart", victim.windowStart);
	v.Set("windowEnd", victim.windowEnd);
	j.Set("victim", v);

	Json p = Json::MakeObject();
	p.Set("layout", plan.layout);
	p.Set("width", plan.width);
	p.Set("height", plan.height);
	Json rooms = Json::MakeArray();
	for (const PublicPlanRoom& r : plan.rooms)
	{
		Json room = Json::MakeObject();
		room.Set("id", r.id);
		room.Set("label", r.label);
		room.Set("kind", r.kind);
		room.Set("x", r.x);
		room.Set("y", r.y);
		room.Set("w", r.w);
		room.Set("h", r.h);
		Json door = Json::MakeObject();
		door.Set("x", r.doorX);
		door.Set("y", r.doorY);
		room.Set("door", door);
		room.Set("walk", r.walk);
		Json spots = Json::MakeArray();
		for (const auto& [id, label] : r.spots)
		{
			Json sp = Json::MakeObject();
			sp.Set("id", id);
			sp.Set("label", label);
			spots.Push(sp);
		}
		room.Set("spots", spots);
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
	else p.Set("corridor", Json());
	j.Set("plan", p);

	if (modifier)
	{
		Json mod = Json::MakeObject();
		mod.Set("key", modifier->key);
		mod.Set("label", modifier->label);
		mod.Set("note", modifier->note);
		j.Set("modifier", mod);
	}
	else j.Set("modifier", Json());
	j.Set("synopsis", synopsis);

	Json sus = Json::MakeArray();
	for (const PublicSuspect& s : suspects)
	{
		Json o = Json::MakeObject();
		o.Set("id", s.id);
		o.Set("name", s.name);
		o.Set("role", s.role);
		o.Set("age", s.age);
		o.Set("color", s.color);
		o.Set("bio", s.bio);
		o.Set("alibiPlace", s.alibiPlace);
		o.Set("alibiRoomId", s.alibiRoomId);
		o.Set("look", s.look);
		o.Set("alibiStatement", s.alibiStatement);
		Json traitsJ = Json::MakeObject();
		for (const auto& [dim, label] : s.traits) traitsJ.Set(dim, label);
		o.Set("traits", traitsJ);
		Json keysJ = Json::MakeObject();
		for (const auto& [dim, key] : s.traitKeys) keysJ.Set(dim, key);
		o.Set("traitKeys", keysJ);
		Json interview = Json::MakeArray();
		for (const QA& qa : s.interview) interview.Push(QAJson(qa));
		o.Set("interview", interview);
		sus.Push(o);
	}
	j.Set("suspects", sus);

	Json evs = Json::MakeArray();
	for (const PublicEvidence& e : evidence)
	{
		Json o = Json::MakeObject();
		o.Set("id", e.id);
		o.Set("role", e.role);
		o.Set("kind", e.kind);
		o.Set("title", e.title);
		o.Set("summary", e.summary);
		o.Set("details", e.details);
		if (e.event)
		{
			Json ev = Json::MakeObject();
			ev.Set("min", e.event->min);
			if (e.event->roomId) ev.Set("roomId", *e.event->roomId);
			if (e.event->suspectId) ev.Set("suspectId", *e.event->suspectId);
			o.Set("event", ev);
		}
		if (e.puzzle)
		{
			const Puzzle& pz = *e.puzzle;
			Json pj = Json::MakeObject();
			pj.Set("type", pz.type);
			if (pz.type == "uv") pj.Set("key", pz.key);
			else if (pz.type == "torn") pj.Set("pieces", StringsJson(pz.pieces));
			else if (pz.type == "cipher") pj.Set("shift", pz.shift);
			else pj.Set("answer", pz.answer);
			o.Set("puzzle", pj);
		}
		if (e.weapon)
		{
			Json w = Json::MakeObject();
			w.Set("key", e.weapon->key);
			w.Set("residue", e.weapon->residue);
			o.Set("weapon", w);
		}
		if (e.spotId) o.Set("spotId", *e.spotId);
		if (e.model)
		{
			const EvidenceModel& md = *e.model;
			Json mj = Json::MakeObject();
			mj.Set("type", md.type);
			if (md.type == "watch") { mj.Set("min", md.min); mj.Set("engraving", md.engraving); }
			else if (md.type == "monogram") { mj.Set("item", md.item); mj.Set("initials", md.initials); }
			else mj.Set("seal", md.seal);
			o.Set("model", mj);
		}
		if (e.confirms) o.Set("confirms", StringsJson(*e.confirms));
		evs.Push(o);
	}
	j.Set("evidence", evs);

	Json opts = Json::MakeObject();
	opts.Set("motives", OptionsJson(motiveOptions));
	opts.Set("weapons", OptionsJson(weaponOptions));
	opts.Set("accomplice", accompliceOption);
	j.Set("options", opts);
	return j;
}

} // namespace mystery
