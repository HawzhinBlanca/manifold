# Changelog

All notable changes to MANIFOLD are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/); this project uses
work-package milestones rather than semantic versions until first playable.

## [Unreleased]

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
- **25 / 25** automation tests green, headless. Repo is public and professional.
  Remaining phase (World/VFX/Audio polish + real UI) is human-owned — see
  `Docs/IMPLEMENTATION_STATUS.md`.
