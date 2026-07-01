# Changelog

All notable changes to MANIFOLD are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/); this project uses
work-package milestones rather than semantic versions until first playable.

## [Unreleased]

### Added (game layer)
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
- **35 / 35** automation tests green, headless. Repo is public and professional.
  Remaining phase (real art/VFX scenes, bound sound assets, bespoke UMG UI, human
  playtest) is human-owned and needs the editor + a display — see
  `Docs/IMPLEMENTATION_STATUS.md`.
