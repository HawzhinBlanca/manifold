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

### Status
- **20 / 20** automation tests green, headless. Presentation layer (World/VFX/Audio/
  interactive UI) is the next, human-owned phase — see `Docs/IMPLEMENTATION_STATUS.md`.
