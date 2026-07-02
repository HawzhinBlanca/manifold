<div align="center">

# MANIFOLD

**A game where the gameplay *is* cross-domain analogy.**
Discover that two simulation realms secretly share one hidden structure — then transport power across the seam.

[![Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-black)](https://www.unrealengine.com/)
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue)]()
[![Tests](https://img.shields.io/badge/automation%20tests-83%2F83%20green-brightgreen)]()
[![Determinism](https://img.shields.io/badge/simulation-deterministic-informational)]()
[![License](https://img.shields.io/badge/license-Proprietary-red)](LICENSE)

</div>

---

## What is MANIFOLD?

You play a **Synthesist**. Each *realm* is a living simulation — orbital mechanics,
fluid dynamics, vibrating harmonics — with its own laws. The "aha" of the game is
realizing that two utterly different realms share the **same underlying structure**
(an orbital `3:2` resonance *is* a harmonic `3:2` ratio *is* …), and using that
correspondence to carry a solution from one world into another.

The north star: **the experience must stay un-pre-computable.** Novelty lives at the
**seams** between realms and in player-found correspondences. The moat is that the
game can't be fully solved.

> **Design pillar:** _the correspondence engine is the product; visuals are a layer._

📖 New here? Read **[How to Play](Docs/GAMEPLAY.md)** — the loop, controls, the five
realms + decoy, scoring, and expedition mode.

## Status

**Playable, deterministic, and fully code-complete** — no mocks or placeholders in
code (verified by a codebase-wide adversarial audit). What remains is art direction.

- ✅ **83 / 83** automation tests pass, headless, 0 failures
- ✅ Deterministic fixed-step simulation (bitwise-reproducible; real replay verification)
- ✅ **Seven** realms (Orbits, Fluids, Harmonics, Waves, Rhythm, Gears, Circuits) sharing
  one hidden ratio across celestial / fluid / acoustic / spatial / temporal / mechanical /
  electromagnetic domains, via a generic N-realm engine — plus a **decoy** realm the engine
  refuses to false-match
- ✅ **Two play modes** — *Classic* (find the shared ratio, carry it across the seam) and
  **Constellation Lock**: every realm shows a *different* surface ratio and you must infer
  the session's hidden rule (Exact or **Octave-equivalent**) and lock the exact
  corresponding subset — cross-domain reasoning, not number-spotting
- ✅ **Constellation depth**: an **Expert** tier (the rule itself is hidden — infer it), a
  **probe economy** (pay score to reveal a realm's membership), a **Constellation
  Expedition** campaign, difficulty/precision **scoring + S–D rank**, and **separate
  best scores** per mode
- ✅ Game loop with an **objective** (win/lose) and **deterministic, shareable replays**
  (both modes reproduce bit-for-bit from their seed)
- ✅ **Audio you can hear** — procedural synthesis; a discovered `3:2` sounds a perfect
  fifth; a wrong lock buzzes and a solved constellation resolves in a bright fanfare
- ✅ **Real 3D geometry** for every realm + a **branded HUD** with a procedural emblem
- ✅ **Enhanced Input** (`[E]` transport, `[R]` restart, `[C]` cycle mode, `1`–`6` pick,
  `[Space]` lock, `[V]` reveal) + a **packaged standalone build** (`Tools/CI/package.ps1`
  → `dist/Windows/`)
- 🎨 Art direction (Megascans/Nanite scenes, Niagara VFX, bespoke materials/UMG, a hand-
  authored map) layers on top — see [`Docs/IMPLEMENTATION_STATUS.md`](Docs/IMPLEMENTATION_STATUS.md)

## Architecture

Deterministic C++ kernels behind a common interface; data-driven correspondence
content; ownership boundaries are literal folders.

| Module | Type | Responsibility |
|--------|------|----------------|
| `MANIFOLDCore` | Runtime | `IRealmKernel` interface, deterministic RNG / fixed-step / replay, lazy realization |
| `MANIFOLDKernelsOrbits` | Runtime | Velocity-Verlet n-body, Keplerian elements, resonance detection |
| `MANIFOLDKernelsFluids` | Runtime | Stam stable-fluids solver, vortex detection |
| `MANIFOLDKernelsHarmonics` | Runtime | Coupled oscillators, integer frequency-ratio structure |
| `MANIFOLDKernelsWaves` | Runtime | String standing waves, harmonic-index ratio structure |
| `MANIFOLDKernelsRhythm` | Runtime | Polyrhythms, small-integer tempo ratio structure |
| `MANIFOLDKernelsGears` | Runtime | Meshed gear trains, exact tooth-count ratio structure |
| `MANIFOLDKernelsCircuits` | Runtime | Resonant LC tanks, frequency-ratio structure (electromagnetism) |
| `MANIFOLDCorrespond` | Runtime | Data-driven mapping, detection, transport, generic N-realm engine (relation-aware: Exact / Octave / Reciprocal) |
| `MANIFOLDTelemetry` | Runtime | Insight-Rate event logging |
| `MANIFOLDGameplay` | Runtime | `UManifoldSlice` — the playable vertical-slice loop |

Every realm implements `IRealmKernel` (`Initialize → Step → Query → Serialize`) and
exposes its structure for mappings. Correspondences are authored as data
(`Data/Correspondences/*.json`), not hardcoded.

## Build & test

**Prerequisites:** Unreal Engine 5.8, Visual Studio 2022 C++ toolchain (MSVC 14.4x),
.NET Framework 4.8 SDK, Git LFS.

```powershell
# 1. Build the editor target (compiles all runtime modules + tests)
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    MANIFOLDEditor Win64 Development -Project="$PWD\MANIFOLD.uproject" -WaitMutex

# 2. Run the full automation suite headless (no GPU required)
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\MANIFOLD.uproject" -ExecCmds="Automation RunTests MANIFOLD; Quit" `
    -unattended -nullrhi -nosplash -nopause -stdout
```

Expected: `83 Success, 0 Fail`. Close the Unreal Editor first (Live Coding holds a
build lock). The CI harness `Tools/CI/run_tests.ps1` wraps both steps.

## Repository layout

```
Source/
  Core/            deterministic sim, RNG, replay, kernel interface   [Systems]
  Kernels/<Realm>/ one module per realm                              [Systems]
  Correspond/      mapping, detection, transport, N-realm engine     [Systems]
  Telemetry/       Insight-Rate logging                              [Systems]
  Gameplay/        vertical-slice orchestration                      [Systems]
Data/Correspondences/  the correspondence specs (data)              [Design]
Content/               UE assets (via Git LFS)                       [World/VFX]
Tools/CI/              build + headless test harness                 [Infra]
Docs/                  build plan, tooling bible, status/handoff     [shared]
```

Rule: an owner edits only their folder(s); cross-folder needs go through an
interface, not a reach-in.

## Documentation

- [`Docs/IMPLEMENTATION_STATUS.md`](Docs/IMPLEMENTATION_STATUS.md) — authoritative status & handoff
- [`Docs/PLAYTEST.md`](Docs/PLAYTEST.md) — how to playtest for feel + what to report (feedback → tuning knobs)
- [`MANIFOLD_Build_Plan.md`](MANIFOLD_Build_Plan.md) — the end-to-end, work-package build plan
- [`MANIFOLD_Tooling_Bible.md`](MANIFOLD_Tooling_Bible.md) — tool stack
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — workflow, conventions, verification gate

## License

Proprietary — all rights reserved. See [LICENSE](LICENSE).
