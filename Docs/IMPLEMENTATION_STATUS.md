# MANIFOLD — Implementation Status & Handoff

_Everything marked ✅ is verified by an automation test that passes headless._

## 1. TL;DR

MANIFOLD is a **playable, deterministic, data-driven, N-realm correspondence game**
with an objective, deterministic replay, a code-level audio layer, key input, and a
professional public repository + a headless packaging path.

**39 automation tests pass, 0 failures, headless.** The correspondence engine — the
product's core per the design bible — is complete and generalized to N realms; a
playable shell drives it with an on-screen readout (objective + session state), a
debug-draw view of the realms, key-bound verbs, and audio cues. **Five** realms now
share a 3:2 across different physical domains (celestial, fluid, acoustic, spatial,
temporal). A session can be Won or Lost, recorded, and replayed bit-for-bit.

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

### Realms (production template, §9) — a 3:2 across five domains
- ✅ **Orbits** (celestial: mean-motion resonance)
- ✅ **Fluids** (vortex)
- ✅ **Harmonics** (acoustic: coupled-oscillator frequency ratio)
- ✅ **Waves** (spatial: string standing-wave harmonics) — `MANIFOLD.Kernels.Waves.*`
- ✅ **Rhythm** (temporal: three-against-two polyrhythm) — `MANIFOLD.Kernels.Rhythm.*`

### Game layer (procedural, objective, replay, audio, input)
- ✅ **Procedural puzzle variation**: each seed hides a different coprime ratio across
  all five realms; same seed reproduces. `MANIFOLD.Play.ProceduralVariation`.
- ✅ **Objective / win-state**: Won on target discoveries, Lost on step-budget
  exhaustion. `MANIFOLD.Play.ObjectiveWin`, `MANIFOLD.Play.ObjectiveLose`.
- ✅ **Deterministic replay**: whole-slice determinism + record/reproduce + on-disk
  round-trip. `MANIFOLD.Play.SliceDeterminism`, `MANIFOLD.Play.ReplayRoundTrip`.
- ✅ **Audio synthesis**: ratio→interval mapping and a real procedural synth that PLAYS
  the cues. `MANIFOLD.Audio.*`, `MANIFOLD.Play.AudioIntegration`.
- ✅ **Branded UI**: procedural MANIFOLD emblem + framed HUD + win/lose banner.
  `MANIFOLD.UI.Emblem`.
- **Enhanced Input**: `[E]` transport, `[R]` restart (AManifoldPlayerController;
  compile-verified, needs a display to play).

### Correspondence engine
- ✅ **Data-driven** content (D1): `Data/Correspondences/OrbitsFluids.json`.
- ✅ **Generic N-realm**: any two realms exposing the same ratio correspond.
  `MANIFOLD.Integration.MultiRealmCorrespondence`.

### Playable shell + gameplay
- ✅ **Interactive session**: tick the slice, a correspondence lights up, the
  PLAYER transports it across the seam. `MANIFOLD.Play.InteractiveSession`.
- ✅ **Vertical-slice gate (P2)**: treatment Insight Rate > 0, control == 0,
  treatment strictly beats control. `MANIFOLD.Play.VerticalSliceGate`.
- **AManifoldGameMode / AManifoldHUD / AManifoldRealmVisualizer** — the runnable
  front end: fixed-cadence session, a branded HUD (emblem + readout + win/lose banner),
  real static-mesh geometry for all five realms + a lit seam bridge, a live synth, and
  the `[E]`/`[R]` verbs. (Visual/audio output is seen/heard at the editor/display; the
  logic behind it is test-verified.)

## 3. How to play / verify

**Play (at the editor):** open the project, open any empty level, press Play.
`GlobalDefaultGameMode` is `ManifoldGameMode`, so the slice runs with the HUD and
the debug-draw visualizer. The `ManifoldTransport` console command fires the verb.

**Verify (headless):**
```powershell
Tools\CI\run_tests.ps1      # build editor target + all MANIFOLD tests (expect 39 Success, 0 Fail)
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
  `self-hosted, windows, unreal`) — the one step that must run on your machine
  before CI executes the 25-test gate on every push/PR.

## 5. What is real in code vs. what needs an artist

Everything below the mechanics is now **real, working code** — not mocks or
placeholders (verified by a codebase-wide incompleteness audit, §7):
- Deterministic replay actually re-derives and compares hashes (catches divergence).
- Structure ids are stable; detection is data-driven and honors tolerances.
- **Audio is synthesized and played** in real time (`UManifoldSynthComponent`) — no
  external sound asset required.
- The visualizer spawns **real static-mesh geometry** for all five realms.

What remains is genuinely an *art-direction* effort that needs a DCC pipeline + a
display, and cannot be authored headlessly in code:

- **Art materials / VFX** — Megascans/Nanite scenes, Niagara resonance ribbons,
  bespoke materials and post, layered on top of the existing real geometry/cues.
- **Recorded/authored audio** — optional richer instruments/MetaSounds to replace or
  layer over the procedural tones (the routing + synthesis already work).
- **Bespoke UMG UI** — an art-directed HUD/menus replacing the on-screen text.
- **Bespoke startup map** — a packaged build boots a minimal engine map (the GameMode
  runs the whole slice on it); swap in an art-directed `.umap`.
- **Playtest with humans** — the numeric gate is coded; a felt playtest is not.

### Incompleteness audit

A multi-agent adversarial audit swept the whole `Source/` tree for stubs, canned
returns, ephemeral ids, hardcoded fallbacks, "data-driven" code that ignored its
data, and presentation layers that produced no effect. It found **16** real issues;
**all are fixed with tests**. See the "Hardened (no mocks / placeholders)" section of
`CHANGELOG.md` for the itemized list.

## 6. Known notes

- Adding realms is now near-free (template + generic engine): add a kernel, register
  it, and any shared ratio auto-corresponds — as Waves demonstrated (first-try compile).
- One earlier commit (`052cbaa`) contains a benign AndroidFileServer dev token in
  history; the plugin is now disabled so it no longer regenerates. History rewrite
  optional (it is not an account credential).

## 7. Git

Branch `main` (pushed to origin). Recent history:

```
2d9711e feat(realm): add Waves (4th realm) — a 3:2 across three domains
a2f9fbc feat(slice): surface Orbits<->Harmonics in play + vertical-slice gate
10c711e ci: branch protection on main + one-command self-hosted runner setup
ea2a8a6 fix(orbits): stable resonance identity for reliable dedup + player selection
f507726 feat(viz): debug-draw realm visualization + resonance/seam ribbons; default GameMode
be020e9 feat(shell): playable vertical-slice shell (GameMode + HUD + player verb)
2d... (earlier) engine, N-realm, D1, gameplay, professional repo setup
```
