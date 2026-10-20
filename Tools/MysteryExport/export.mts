/**
 * Exports the web game's case content and a golden reference for the C++ port.
 *
 *   cd <web repo>/server && npx tsx <this repo>/Tools/MysteryExport/export.mts <web repo> <this repo>
 *
 * Writes:
 *   <this repo>/Content/Data/Mystery/content.json   all case text and theme data (committed)
 *   <this repo>/Saved/MysteryGolden/golden.ndjson   one line per test case, everything the
 *                                                    TypeScript engine produces (regenerated, not committed)
 *
 * The content is exported from the TypeScript source itself, so both games share their text exactly.
 * Two things that depend on the JavaScript runtime are precomputed here instead of ported:
 * the locale-aware order of motive labels, and the portrait look of each suspect name.
 */
import { mkdirSync, writeFileSync, createWriteStream } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webRoot, gameRoot] = process.argv.slice(2);
if (!webRoot || !gameRoot) {
  console.error('usage: export.mts <web repo> <game repo>');
  process.exit(2);
}
const src = (p: string) => pathToFileURL(join(webRoot, 'server', 'src', p)).href;

const common = await import(src('content/common.ts'));
const themes = await import(src('content/themes.ts'));
const engine = await import(src('mysteryEngine.ts'));

// ─── Content ────────────────────────────────────────────────────────────────

type T = { en: string; es: string };
const LANGS = ['en', 'es'] as const;

/** For each language, the rank of every motive label in localeCompare order. */
const motiveRank: Record<string, number[]> = {};
for (const lang of LANGS) {
  const order = common.MOTIVES.map((_: unknown, i: number) => i)
    .sort((a: number, b: number) => common.MOTIVES[a].label[lang].localeCompare(common.MOTIVES[b].label[lang], lang));
  motiveRank[lang] = common.MOTIVES.map((_: unknown, i: number) => order.indexOf(i));
}

const themesOut: Record<string, unknown> = {};
for (const key of themes.THEME_KEYS) {
  const th = themes.THEMES[key];
  themesOut[key] = {
    ...th,
    suspects: th.suspects.map((s: { name: string; role: T }) => ({ ...s, look: themes.portraitLook(s.name) })),
  };
}

const content = {
  version: 1,
  source: 'GIlbertoRCP/MysteryOTF server/src/content (exported by Tools/MysteryExport/export.mts)',
  traits: common.TRAITS,
  traitDimLabels: common.TRAIT_DIM_LABELS,
  leadDims: common.LEAD_DIMS,
  analystDims: common.ANALYST_DIMS,
  motives: common.MOTIVES,
  motiveRank,
  causes: common.CAUSES,
  text: common.TEXT,
  modifiers: common.MODIFIERS,
  tamperedNote: common.TAMPERED_NOTE,
  fakeBlood: common.FAKE_BLOOD,
  spots: common.SPOTS,
  spotNothing: common.SPOT_NOTHING,
  spotScene: common.SPOT_SCENE,
  spotFound: common.SPOT_FOUND,
  followUp: common.FOLLOW_UP,
  story: common.STORY,
  plantedItems: common.PLANTED_ITEMS,
  themeKeys: themes.THEME_KEYS,
  themes: themesOut,
};
const contentDir = join(gameRoot, 'Content', 'Data', 'Mystery');
mkdirSync(contentDir, { recursive: true });
writeFileSync(join(contentDir, 'content.json'), JSON.stringify(content, null, 1) + '\n');

// ─── Golden reference: the same seeds the web game's test uses ──────────────

const DIFFICULTIES = ['Novice', 'Detective', 'Mastermind'];
const CASES_PER_COMBO = 40;
const goldenDir = join(gameRoot, 'Saved', 'MysteryGolden');
mkdirSync(goldenDir, { recursive: true });
const out = createWriteStream(join(goldenDir, 'golden.ndjson'));

let count = 0;
for (const theme of themes.THEME_KEYS) {
  for (const difficulty of DIFFICULTIES) {
    for (let i = 0; i < CASES_PER_COMBO; i++) {
      const seed = `TEST-${theme}-${difficulty}-${i}`;
      const bp = engine.buildBlueprint(difficulty, theme, seed);
      const perLang: Record<string, unknown> = {};
      for (const lang of LANGS) {
        perLang[lang] = {
          mystery: engine.localize(bp, lang),
          explanation: engine.explainSolution(bp, lang),
          hints: engine.hintsFor(bp, lang),
          story: engine.storyFor(bp, lang),
          // Every clue shown to every suspect, in blueprint order.
          confront: bp.evidence.map((e: { id: string }) =>
            bp.suspects.map((s: { id: string }) => engine.confront(bp, s.id, e.id, lang))),
          search: Object.fromEntries(bp.spots.map((sp: { room: number }, n: number) => {
            const k = bp.spots.slice(0, n).filter((x: { room: number }) => x.room === sp.room).length;
            const id = `spot-${sp.room}-${k}`;
            return [id, engine.searchSpot(bp, id, lang)];
          })),
        };
      }
      out.write(JSON.stringify({ seed, theme, difficulty, blueprint: bp, ...perLang }) + '\n');
      count++;
    }
  }
}

// A few daily cases, to pin down dailyCase's hashing.
const daily = ['2026-01-01', '2026-10-19', '2027-02-28'].map(d => ({ date: d, ...engine.dailyCase(d) }));
out.write(JSON.stringify({ daily }) + '\n');
out.end();
await new Promise(resolve => out.on('finish', resolve));
console.log(`exported content and ${count} golden cases`);
