# MANIFOLD — Implementation Status & Handoff

_Everything marked ✅ is verified by an automation test that passes headless._

## 1. TL;DR

MANIFOLD is a **playable, deterministic, data-driven, N-realm correspondence game**
with **two play modes**, an objective + scoring + rank, deterministic shareable
replays, a code-level audio layer, key input, a branded HUD, and a professional public
repository + a headless packaging path.

**71 automation tests pass, 0 failures, headless.** The correspondence engine — the
product's core per the design bible — is complete, generalized to N realms, and
**relation-aware** (Exact / Octave / Reciprocal). **Seven** realms share a hidden ratio
across different physical domains (celestial, fluid, acoustic, spatial, temporal,
mechanical, electromagnetic) plus a **decoy**. A session can be Won or Lost, scored/ranked, recorded, and
replayed bit-for-bit. The full standalone Windows build cooks/stages/paks successfully.

## 2. What is implemented (✅ = test-verified)

### Systems / Code (Stream S) — critical path
| WP | What | Test |
|----|------|------|
| ✅ S1 | `IRealmKernel` interface | `MANIFOLD.Systems.RealmKernelInterface` |
| ✅ S2 | Deterministic core: RNG, fixed-step, replay | `MANIFOLD.Systems.DeterministicCore.*` |
| ✅ S3 | Orbits kernel (n-body, resonance, **stable ids**) | `MANIFOLD.Kernels.Orbits.*` |
| ✅ S4 | Fluids kernel (stable fluids, vortex) | `MANIFOLD.Kernels.Fluids.*` |
| ✅ S5 | Correspondence (mapping + detect + validate) | `MANIFOLD.Correspondence.*` |
| ✅ S6 | Transport mechanic | `MANIFOLD.Transport.StateChangeVerification` |
| ✅ S7 | Lazy realization | `MANIFOLD.LazyRealization.*` |
| ✅ S8 | Telemetry / Insight-Rate | `MANIFOLD.Telemetry.InsightRateEvents` |

### Realms (production template, §9) — one hidden ratio across six domains
- ✅ **Orbits** (celestial: mean-motion resonance)
- ✅ **Fluids** (vortex, the transport target)
- ✅ **Harmonics** (acoustic: coupled-oscillator frequency ratio)
- ✅ **Waves** (spatial: string standing-wave harmonics) — `MANIFOLD.Kernels.Waves.*`
- ✅ **Rhythm** (temporal: three-against-two polyrhythm) — `MANIFOLD.Kernels.Rhythm.*`
- ✅ **Gears** (mechanical: exact tooth-count ratio) — `MANIFOLD.Kernels.Gears.*`
- ✅ **Circuits** (electromagnetic: LC resonant-frequency ratio) — `MANIFOLD.Kernels.Circuits.*`

### Correspondence engine
- ✅ **Data-driven** content (D1): `Data/Correspondences/OrbitsFluids.json`.
- ✅ **Generic N-realm** with `QueryAll` (all ratios, matched all-vs-all) and exact-GCD
  integer detection. Any two realms sharing a structure correspond.
- ✅ **Relation-aware** matching via `UCorrespondenceSystem::NormalizeRatio` +
  `SetActiveRelation`: `Exact` (literal), `OctaveInvariant` (÷ factors of 2), `Reciprocal`
  (p:q ~ q:p). `MANIFOLD.Correspondence.RelationNormalize`, `…ConstellationOctave`.

### Game layer — Classic mode
- ✅ **Procedural puzzle variation**: each seed hides a different coprime ratio across the
  realms; same seed reproduces. `MANIFOLD.Play.ProceduralVariation`.
- ✅ **Decoy realm (the moat)**: a red-herring realm the engine refuses to pair — the
  player must discriminate. `MANIFOLD.Play.DecoyRealm`.
- ✅ **Objective / win-state**: Won on target discoveries, Lost on step-budget exhaustion.
  `MANIFOLD.Play.ObjectiveWin`, `…ObjectiveLose`.
- ✅ **Scoring + rank (S/A/B/C/D) + persistent profile**. `MANIFOLD.Play.Scoring`,
  `…Rank`, `…ProfileRoundTrip`.
- ✅ **Deterministic replay**: whole-slice determinism + record/reproduce + on-disk
  round-trip + interactive capture. `MANIFOLD.Play.SliceDeterminism`, `…ReplayRoundTrip`,
  `…CaptureReplay`.
- ✅ **Expedition**: escalating-difficulty campaign to a natural wall. `MANIFOLD.Play.Expedition`.
- ✅ **Audio synthesis**: ratio→interval mapping + a real procedural synth that PLAYS the
  cues. `MANIFOLD.Audio.*`, `MANIFOLD.Play.AudioIntegration`.
- ✅ **Branded UI**: procedural emblem + framed HUD + win/lose banner. `MANIFOLD.UI.Emblem`.

### Game layer — Constellation Lock mode (the harder puzzle)
Every realm shows a *different* surface ratio; a hidden size-K subset corresponds under a
per-session relation (Exact or Octave) the player must **infer and lock exactly**.
- ✅ **The mode + player verb**: `SetupConstellation` / `PickRelation` / `PickConstellation`
  / `PlayerLockConstellation` — an exact-set lock ignites the true C(K,2) analogies and
  wins; a wrong lock burns a probe. `MANIFOLD.Play.ConstellationLock`, `…ConstellationOctaveSurface`.
- ✅ **Determinism + realizability**: seed-deterministic; a **96-seed sweep** proves every
  kernel realizes its ratio and no decoy spuriously corresponds.
  `MANIFOLD.Play.ConstellationDeterminism`, `…ConstellationRealizationSweep`.
- ✅ **Reproducible replay**: record/reproduce/save/load (format v2). `MANIFOLD.Play.ConstellationReplay`.
- ✅ **Difficulty/precision scoring**: Octave and flawless solves outscore Exact/probed.
  `MANIFOLD.Play.ConstellationScoring`.
- ✅ **Expert tier**: the rule is hidden — the player infers Exact vs Octave from the
  ratios. `MANIFOLD.Play.ConstellationExpert`.
- ✅ **Probe economy**: pay score (`[V]`) to reveal a realm's membership (IN/out), idempotent
  per realm. `MANIFOLD.Play.ConstellationReveal`.
- ✅ **Expedition campaign**: N escalating puzzles, one cumulative score, deterministic.
  `MANIFOLD.Play.ConstellationExpedition`.
- ✅ **Audible feedback**: a wrong lock buzzes; a solve resolves in a victory fanfare.
  `MANIFOLD.Play.ConstellationAudio`.
- ✅ **Separate best scores** per mode (v2 profile). `MANIFOLD.Play.ProfilePerMode`.

### Playable shell + gameplay
- ✅ **Interactive session** + **vertical-slice gate (P2)** (treatment beats control).
  `MANIFOLD.Play.InteractiveSession`, `…VerticalSliceGate`.
- **AManifoldGameMode / AManifoldHUD / AManifoldRealmVisualizer / AManifoldPlayerController**
  — the runnable front end: a play-mode toggle, fixed-cadence Classic sim, the Constellation
  readout, real static-mesh geometry + starfield + scene lighting + post, a live synth, and
  the `[E]`/`[R]`/`[C]`/`1`–`6`/`[Space]` verbs (Enhanced Input, built in code). Visual/audio
  output is seen/heard at the editor/display; the logic behind it is test-verified.

## 3. How to play / verify

**Play (at the editor):** open the project, open any empty level, press Play.
`GlobalDefaultGameMode` is `ManifoldGameMode`. Classic: `[E]` transport, `[R]` restart.
Press `[C]` to switch to **Constellation Lock**: `1`–`6` pick realms, `[Space]` locks,
`[R]` rolls a new puzzle.

**Verify (headless):**
```powershell
Tools\CI\run_tests.ps1      # build editor target + all MANIFOLD tests (expect 71 Success, 0 Fail)
Tools\CI\package.ps1        # produce a standalone Windows build under dist\Windows
```
Close the Unreal Editor first (Live Coding holds a build lock).

## 4. Repository & CI (professional)

- Public: **https://github.com/HawzhinBlanca/manifold** (default branch `main`).
- Proper UE `.gitignore`, **Git LFS** for binaries, `.gitattributes`, `.editorconfig`.
- README, LICENSE (proprietary), CONTRIBUTING, CHANGELOG.
- `.github/`: CI workflow (self-hosted UE runner), PR template, issue forms, CODEOWNERS.
- Branch protection on `main`: linear history, no force-push/deletion, conversation
  resolution required.
- `Tools/CI/setup-runner.ps1` registers a self-hosted runner (labels
  `self-hosted, windows, unreal`).

## 5. What is real in code vs. what needs an artist

Everything below the mechanics is **real, working code** — not mocks or placeholders
(verified by adversarial audits, §6):
- Deterministic replay actually re-derives and compares hashes (catches divergence).
- Structure ids are stable; detection is data-driven, relation-aware, and honors tolerances.
- **Audio is synthesized and played** in real time (`UManifoldSynthComponent`).
- The visualizer spawns **real static-mesh geometry** for every realm + a procedural
  starfield, scene lighting, and cinematic post.

What remains is genuinely an *art-direction* effort that needs a DCC pipeline + a display:
- **Art materials / VFX** — Megascans/Nanite scenes, Niagara ribbons, bespoke materials/post.
- **Recorded/authored audio** — richer instruments/MetaSounds over the working procedural tones.
- **Bespoke UMG UI** — an art-directed HUD/menus replacing the on-screen text (incl. an
  in-game Constellation tutorial; the rules are documented in `Docs/GAMEPLAY.md`).
- **Bespoke startup map** — swap the minimal engine boot map for an art-directed `.umap`.
- **Playtest with humans** — the numeric gate is coded; a felt playtest is not.

## 6. Adversarial audits

Multiple multi-agent adversarial passes swept `Source/` for stubs, canned returns,
ephemeral ids, hardcoded fallbacks, "data-driven" code that ignored its data, and
presentation that produced no effect: an incompleteness audit + three targeted reviews +
a whole-codebase audit found and fixed **29** real issues (none deferred), and a
5-dimension review of the Constellation Lock feature returned **zero** confirmed findings.
See `CHANGELOG.md` for the itemized lists.

## 7. Known notes

- Adding realms is near-free (template + generic engine): add a kernel, register it, and
  any shared ratio auto-corresponds — as Waves/Rhythm/Gears demonstrated.
- One earlier commit (`052cbaa`) contains a benign AndroidFileServer dev token in history;
  the plugin is disabled so it no longer regenerates. History rewrite optional.

## 8. Git

Branch `main` (pushed to origin). Recent history (most recent first):

```
9444181 feat(score): grade Constellation Lock by difficulty and precision
cf63123 feat(replay): reproducible/shareable Constellation Lock replays (format v2)
2ed6ca3 feat(game): make Constellation Lock reachable & playable (mode toggle + input + HUD)
3cefa66 test(gameplay): 96-seed Constellation realization sweep
84250a2 feat(gameplay): Constellation Lock puzzle mode (infer relation, pick subset)
54b7be6 feat(engine): relation-aware correspondence (Constellation Lock foundation)
… earlier: six realms, generic N-realm engine, objective/replay/audio/input, repo + CI
```
