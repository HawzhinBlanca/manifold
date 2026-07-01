# Changelog

All notable changes to MANIFOLD are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/); this project uses
work-package milestones rather than semantic versions until first playable.

## [Unreleased]

### Added (depth — Constellation Lock)
A new, harder puzzle mode that replaces "spot the one odd realm" with genuine
cross-domain inference. **56/56 green.**
- **Relation-aware correspondence engine.** The generic N-realm detector matches under a
  per-session structural **relation** instead of only literal-equal ratios: `Exact`
  (literal, the default — reproduces prior behavior byte-for-byte), `OctaveInvariant`
  (equal after dividing out factors of 2, so 3:2, 6:1 and 3:1 all correspond), or
  `Reciprocal` (p:q ~ q:p). `UCorrespondenceSystem::NormalizeRatio` is the single pure
  source of truth for "corresponds", used for the compare, the dedup key and the broadcast
  id. Tests: `MANIFOLD.Correspondence.RelationNormalize`, `…ConstellationOctave`.
- **The Constellation-Lock session.** `UManifoldSlice::SetupConstellation(seed)` builds six
  ratio realms that each show a DIFFERENT surface ratio; a hidden subset (the
  "constellation") actually corresponds under the session's relation, chosen from the seed.
  Under Octave the corresponding realms look unlike each other (3:1, 3:2, 6:1 all collapse
  to one class), so the player must normalize and reason rather than number-spot. Every
  realm realizes its assigned ratio through its own physics (Orbits via a Kepler period
  ratio, Gears via tooth counts, Waves/Rhythm/Harmonics via integers), all detected exactly.
- **The player's verb.** `PlayerLockConstellation(selected)` scores ONLY on an exact-set
  match with the hidden constellation — then the true C(K,2) analogies ignite through the
  same discovery/score/telemetry path as organic play and the session is won; a
  plausible-but-wrong lock burns a probe and scores nothing (each wasted probe costs
  points). Deterministic in the seed; decoys are constructed with mutually-distinct octave
  classes so they never spuriously correspond. Tests: `MANIFOLD.Play.ConstellationLock`,
  `…ConstellationDeterminism`, `…ConstellationOctaveSurface`.

### Hardened (no mocks / placeholders)
An adversarial, codebase-wide incompleteness audit found 16 real stubs/fakes;
all are now fixed with tests (**37/37 green**):
- **Deterministic replay is real:** `FFixedStepSimulation::VerifyReplay` was a canned
  `return true;`. It now folds the RNG stream into a running hash and re-derives it
  from a snapshot — a wrong hash/step is rejected (test proves divergence is caught).
- **Stable structure ids:** Harmonics/Waves/Rhythm queries returned `FGuid::NewGuid()`
  every call; now stable (realm + ratio + version). Shared-structure discovery and
  Orbits→Fluids transport ids are deterministic (were throwaway GUIDs).
- **Truly data-driven detection:** removed the hardcoded `"3:2"` branch (a default spec
  is synthesized as data) and now honors `FCorrespondenceSpec::Tolerance`.
- **Replay honors the recorded transport schedule** (was ignored).
- **Audio actually plays:** `UManifoldSynthComponent` (real `USynthComponent`)
  synthesizes decaying-sine tones from the cues — a discovered 3:2 is an audible
  perfect fifth. No external sound asset. DSP is unit-tested.
- **Real geometry:** the realm visualizer now spawns static-mesh objects (five realms)
  instead of debug-draw lines.

### Audited (whole-codebase sweep + targeted reviews)
Five adversarial passes (an incompleteness audit, three targeted reviews, and a
comprehensive whole-codebase audit) found and fixed **29** real issues total — every
confirmed finding, with none deferred. E.g.:
- Orbits `VerifyDeterminism` compared un-stepped `this` vs a stepped sim (false
  negative) — the only kernel doing so, and the only one with no determinism test;
  fixed + test added.
- Fluids vortex ids, and the profile/replay loaders, hardened for determinism and
  corrupt-input safety; telemetry file-handle leaks closed; the slice resets on reuse.
- The audio phase-wrap, the emblem texture/colour, and the CI gate itself were all
  corrected.
- The two "latent" findings were also completed (not deferred): the correspondence
  engine now exposes ALL of a realm's ratios (`QueryAll`, matched all-vs-all), and
  Harmonics/Rhythm detect integer ratios exactly via GCD (no ≤10 cap).

### Added (presentation polish — code-authored)
- **Cosmic backdrop**: a deterministic procedural starfield shell + key/fill scene
  lighting so the realms sit in space and are clearly lit.
- **Cinematic post**: a PostProcessVolume with bloom (gold correspondences / seam /
  stars glow), a soft vignette, and fixed exposure.
- **Ambient soundscape**: a gentle recurring tonic pad under the discovery/transport
  chimes, so the world always hums.
- **Intro title card**: the emblem, wordmark, one-line goal, and controls over the live
  scene, dismissing on the first action.
- **Rank tiers (S/A/B/C/D)** on the retuned score, shown big in the win banner.

### Added (game depth & replayability)
- **Sixth realm — Gears** (mechanical): two meshed gears with P and Q teeth expose the
  EXACT integer ratio P:Q (reduced by GCD). A single ratio now spans six domains —
  celestial, fluid, acoustic, spatial, temporal, mechanical. `MANIFOLD.Kernels.Gears.*`.
- **Capture replay:** an interactively-played session can be captured as a shareable
  replay (seeds + the exact player transport schedule) and reproduced bit-for-bit; the
  GameMode auto-saves a replay of every winning run. `MANIFOLD.Play.CaptureReplay`.
- **Expedition mode:** a campaign of escalating-difficulty levels (rising discovery
  targets) played back to back until one can't be cleared, with a cumulative score and
  a natural difficulty wall (a session surfaces at most 11 discoveries, so target 12
  ends the run). Deterministic in the base seed. `MANIFOLD.Play.Expedition`.
- **Decoy realm (the moat):** a red-herring realm exhibits a deliberately non-matching
  ratio; the correspondence engine refuses to pair it with the true realms, so the game
  can't be won by assuming "everything matches" — the player must discriminate. Shown on
  the HUD and as a set-apart grey cluster in the 3D view. `MANIFOLD.Play.DecoyRealm`.
- **Scoring + persistent high-score profile:** a deterministic session score (discoveries
  + transports + insight + speed bonus) and a versioned `.manifoldprofile` (best score,
  sessions played/won) the GameMode loads and updates — "beat your best" across runs.
  `MANIFOLD.Play.Scoring`, `MANIFOLD.Play.ProfileRoundTrip`.

### Added (game layer)
- **Procedural puzzle variation:** each session picks a coprime ratio (3:2, 4:3, 5:4,
  5:3, 2:1, 5:2, 7:4, 7:5, 8:5, 9:8) deterministically from its seed and configures all
  five realms — the orbital resonance (Kepler), harmonic modes, string harmonics, the
  polyrhythm, and the generated Orbits↔Fluids spec — to that hidden ratio. Different
  seeds hide different ratios (the game can't be pre-solved); the same seed reproduces
  exactly. Test: `MANIFOLD.Play.ProceduralVariation`.
- **Objective / win-state:** a session now Wins on reaching a target number of
  discoveries (optionally gated on Insight Rate) or Loses when a step budget runs
  out — the goal that turns the endless simulation into a game. `FManifoldObjective`,
  `EManifoldSessionState`, `FManifoldSessionSummary`.
- **Deterministic replay:** record a session (seeds + transport schedule + result),
  re-execute it bit-for-bit, and persist it to a versioned `.manifoldreplay` file —
  the "un-pre-computable yet perfectly reproducible" pillar as a shareable artifact.
- **Audio direction (the correspondence you can hear):** `UManifoldAudioDirector`
  maps the game's integer ratios to consonant intervals (a discovered 3:2 rings as a
  perfect fifth), gives each realm a stable mode + tonic, and emits a cue per
  discovery/transport. Asset-free and unit-tested; the pipeline just binds sounds.
- **Enhanced Input:** `AManifoldPlayerController` builds its InputAction /
  InputMappingContext in code — `[E]` transports the lit correspondence, `[R]`
  restarts the session. HUD now shows objective, session state, and the last cue.
- **Fifth realm — Rhythm** (polyrhythm): three-against-two is a 3:2 in the domain of
  time. A single 3:2 now spans **five** domains (celestial, fluid, acoustic, spatial,
  temporal), all through the generic N-realm engine.
- **Packaging:** `Tools/CI/package.ps1` produces a standalone Windows build
  (BuildCookRun). The build boots into the GameMode, which runs the whole slice.

### Fixed (CI gate)
- The test harness silently under-reported: it failed to parse (a reserved `-Verbose`
  switch), invoked UBT through the wrong `dotnet` and built the game (not editor)
  target, and filtered to aspirational placeholder test names while omitting the real
  Integration/Play/Waves/Harmonics tests. It now builds the editor target via UE's
  bundled toolchain, runs every `MANIFOLD.*` test, and gates on the parsed report.
  Real suite: **35** tests (was reporting a filtered 25).

### Added
- **Systems stream (S1–S8):** deterministic core (PCG RNG, fixed-step, replay),
  Orbits kernel (velocity-Verlet n-body + resonance), Fluids kernel (Stam stable
  fluids + vortex detection), correspondence system, transport mechanic, lazy
  realization, and Insight-Rate telemetry — all acceptance-tested.
- **Vertical-slice loop:** `UManifoldSlice` ties the realms + correspondence +
  transport + telemetry into a headless playable loop, with a no-correspondence
  control build proving the Insight-Rate moat.
- **D1 data-driven content:** correspondences authored in JSON
  (`Data/Correspondences/OrbitsFluids.json`); loader + validation.
- **Harmonics realm:** third `IRealmKernel`, proving the realm production template
  scales.
- **N-realm correspondence engine:** `RegisterRealm` + shared-structure detection —
  any two realms exposing the same ratio correspond (orbital 3:2 ⇄ harmonic 3:2).
- **Professional repo setup:** UE `.gitignore`, Git LFS `.gitattributes`, README,
  LICENSE, CONTRIBUTING, `.editorconfig`, GitHub CI workflow and templates.

### Fixed
- Build against UE 5.8: `bUseUnity`, `BuildSettingsVersion.V7`, automation-flag and
  `UTEST_*` API drift, container serialization operators, and the two missing
  `IMPLEMENT_MODULE` declarations (startup crash).
- Simulation correctness: Orbits energy baseline + resonance ratio convention;
  Fluids determinism check + strongest-vortex query.

### Added (playable slice)
- **Playable shell**: `AManifoldGameMode` (fixed-cadence session) + `AManifoldHUD`
  (live readout) + `AManifoldRealmVisualizer` (debug-draw of realms + gold
  resonance/seam ribbons) + console verb `ManifoldTransport`. Set as the project's
  default GameMode.
- **Interactive session** with a player-driven transport verb; **stable resonance
  ids** (deterministic, fix the ephemeral-GUID dedup bug).
- **Vertical-slice gate (P2)**: treatment vs. control Insight-Rate go/no-go, in code.
- **Fourth realm — Waves** (string standing-wave harmonics); a 3:2 now spans four
  domains, surfaced live in the slice via the generic N-realm engine.

### Added (repo / CI)
- Branch protection on `main` (linear history, no force-push/deletion, conversation
  resolution). `Tools/CI/setup-runner.ps1` for one-command self-hosted runner
  registration. AndroidFileServer plugin disabled (stops dev-token regeneration).

### Status
- **51 / 51** automation tests green, headless. Repo is public and professional.
  Remaining phase (real art/VFX scenes, bound sound assets, bespoke UMG UI, human
  playtest) is human-owned and needs the editor + a display — see
  `Docs/IMPLEMENTATION_STATUS.md`.
