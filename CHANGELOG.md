# Changelog

All notable changes to MANIFOLD are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/); this project uses
work-package milestones rather than semantic versions until first playable.

## [Unreleased]

### Presentation — showcase media captured at 1080p (render-verified)
- Bumped the built-in `-ManifoldAutoShot*` capture resolution 1280x720 -> 1920x1080 (the hardcoded
  `HighResShot` calls in `AManifoldGameMode`) and regenerated all six `Docs/media` assets at 1080p, so
  the showcase is crisper/more premium and reflects the current cinematic build (HDRI sky + the
  brighter energy seam). GIF re-encoded at 1000px width to keep it lean (~4 MB). Tests 104/104.

### Visual — dramatic transport-seam money shot (render-verified)
- The "carry it across the seam" arc read as sparse gold dots. Rebuilt it as a brighter, denser
  golden energy beam (beads 24→52) with a wide, hot energy "packet" racing across (falloff 6→4,
  brightness base 1.0→1.15 / packet 1.6→2.6, sweep a touch faster) so the transport reads as energy
  flowing between realms and blooms via post. Render-verified against the dark HDRI sky: clearly more
  cinematic, doesn't blow out, realms still pop. `AManifoldRealmVisualizer::Tick`. Tests 104/104.

### Visual — real deep-space HDRI sky (render-verified)
- Replaced the procedural purple nebula backdrop with a **real public-domain NASA Milky Way**
  all-sky panorama (SVS *Deep Star Maps 2020*): fetched the 4k EXR, tone-mapped + downsized it to a
  332 KB 2k JPG (`Tools/Art/assets/starmap_milkyway_2k.jpg`), imported it as `T_StarMap`, and authored
  an unlit two-sided `M_SkyDome` that emits it on the giant sky shell (`Tools/Art/manifold_materials.py`
  gained `import_texture` + `make_skydome`). `AManifoldRealmVisualizer` now prefers `M_SkyDome` for the
  backdrop and falls back to the procedural `M_Nebula` when the sky asset is absent. Verified by
  offscreen render: dense realistic stars + the galactic band on a deep-black sky read far more
  cinematic than the procedural haze, and the glowing realms/seam pop *harder* against the darker
  backdrop (no wash, clean equirect mapping). Public-domain credit added in `Docs/CREDITS.md`.
  Logic tests unchanged (104/104). First step of the cinematic/premium graphics pass.

### Visual — realm orbs read as luminous energy spheres (render-verified)
- The ratio-realm orbs rendered as flat matte colored *discs*: `M_RealmOrb`'s emissive body sat below
  the post-process bloom threshold, so only the central star (whose whole body is bright) bloomed.
  Retuned the authored material via `Tools/Art/manifold_materials.py` — core `0.7 → 1.0` so the orb
  body itself blooms into a soft coloured halo, fresnel rim `1.4 → 2.4` (kept clearly hotter than the
  core, widened `exp 3.5 → 3.0`) so the edge still reads as a 3D energy shell rather than a uniform
  blown-out disc. Verified by offscreen render (compared against the flat-disc baseline): the whole
  realm cluster now reads as glowing energy spheres, hues + spherical shape preserved, star still
  brightest. No C++ rebuild (material-asset only); logic tests unchanged (104/104).

### Presentation — showcase media regenerated to the glowing-orb build
- Regenerated all six public assets under `Docs/media/` so the README showcase matches the shipping
  look (previously they showed the flat-disc orbs). Captured headless & offscreen via the built-in
  dev flags — `-ManifoldAutoShot` (HUD-on readout → `gameplay-hud.png`; HUD-off beauty → `realms.png`),
  `-ManifoldAutoShotConstellation` (Constellation Lock readout → `constellation.png`),
  `-ManifoldAutoShotTitle` (title card → `title.png`), and `-ManifoldAutoShotSequence` (an 18-frame
  burst → `scene-loop.gif`, assembled with ffmpeg + a palette pass). Every frame was eyeballed before
  it was kept. Renders are silent (audio muted by default).

### Audio — muted by default until production (dev/CI/headless are silent)
- The procedural synth previously started on every launch (`AManifoldGameMode::BeginPlay` →
  `Synth->Start()`), so editor Play, packaged runs, and even headless render captures emitted
  discovery chimes + an ambient pad. Added the console variable **`manifold.MuteAudio` (default `1`
  = muted)**; `BeginPlay` now only starts real-time output when it is `0`. The synth component is
  still created, so the guarded `PlayCue` calls remain valid no-ops while muted. Production re-enables
  sound with `manifold.MuteAudio 0` (config / `-manifold.MuteAudio=0` / console). Muting gates only
  the real-time device output — the audio cue/sample **logic** (`UManifoldAudioDirector` /
  `UManifoldSynthComponent`) is unchanged and still fully test-covered. The ship-safe default is
  locked by `MANIFOLD.Audio.MutedByDefault`. **104/104 green** (up from 103).

### Robustness — untrusted-input invariant completed across every public serializer
- The gameplay replay layer bounds untrusted variable-length reads (`SerializeBoundedInt32Array`,
  the `Steps` cap), but the Core `FFixedStepSimulation` snapshot serializer — the deterministic
  replay/snapshot type — still read its `Snapshots` array and each snapshot's `StateData` blob with
  UE's default `TArray` serializer, which pre-allocates `count*element` before reading any element
  (its ~16MB cap is net-archive-only). A crafted count would force a multi-GB OOM. Both reads are now
  bounded against the bytes remaining, byte-for-byte identical to the default layout on save and on
  the valid-load path. Locked by `MANIFOLD.Systems.DeterministicCore.SnapshotDeserializeBounded`.
  **103/103 green** (up from 102). With this, **every public serializer in the codebase uniformly
  upholds the "bound untrusted variable-length reads" invariant.**

### Balance — objective sweep extended to the two hardest modes
- `MANIFOLD.Balance.Sweep` (the objective fairness floor) measured only Classic and non-expert
  Constellation across 128 seeds; the harder modes were spot-tested at a single seed each. The sweep
  now also asserts, across the seed space, that **Expert Constellation** (rule hidden) is solvable on
  every seed (128/128) and that a perfect **Expedition** run clears every level with a positive score
  on every swept base seed (32/32) — so a fairness regression in campaign or hidden-rule play can no
  longer slip through. Both invariants held on first run. Test count unchanged (strengthens the
  existing sweep). Still **102/102 green.**

### Determinism — content-hash the realm in Transport's target id (audit follow-up)
- `Transport()`'s Orbits→Fluids branch derived its "deterministic, traceable" perturbation id from
  `GetTypeHash(FName)` — the process-local name-table index, not reproducible across runs/platforms —
  contradicting the contract that id exists to uphold. Now hashes the realm-id **string** (matching the
  `DetectSharedStructureCorrespondences` convention in the same file). Locked by
  `MANIFOLD.Transport.TargetIdContentHashed`. **102/102 green** (up from 101).

### Robustness — public core determinism type hardened (final audit sweep)
- A final adversarial sweep of the last un-audited surfaces — the **FixedStepSimulation / replay
  core**, **DeterministicRNG**, **Telemetry**, and **AudioDirector** — found **zero live defects** (7
  candidates, all refuted as unreachable / dead-code / unobserved), confirming the correctness surface
  has converged (audit confirmed-rate across the session: 8 → 2 → 1 → 0). The verifiers each
  independently recommended one hardening, now applied to `FFixedStepSimulation` (a public
  `MANIFOLDCORE_API` type whose editor `ClampMin` metas don't constrain direct C++/deserialized values):
  - `Tick()` clamps `FixedDeltaTime` to a positive floor — a non-positive value made the substep loop
    infinite and `InterpAlpha` divide by zero.
  - `VerifyReplay`/`VerifyReplayDetailed` validate the interval before use — reject a target before the
    snapshot or an interval over a 100M cap (no unbounded loop), and compute `StepsVerified` only after
    the guards (removing an `int64` underflow/UB). Locked by
    `MANIFOLD.Systems.DeterministicCore.DegenerateConfigGuarded`. **101/101 green** (up from 100).

### Correctness — Waves hash coverage (all-kernels hash-blind-spot sweep, final)
- After the Orbits (per-body `Mass`) and Fluids (velocity grids + config) hash blind spots, a
  systematic adversarial audit of the **five remaining kernels** (Harmonics, Waves, Rhythm, Gears,
  Circuits) for the same three defect classes (hash coverage, config serialization, untrusted
  `SetState`) found the hash-coverage class in **exactly one** — Waves — and cleared the other four.
  `WavesKernel::Step` advances each wave's phase by `HarmonicNumber * Fundamental` (the frequency is
  derived from `Fundamental` each step, not stored), but `ComputeStateHash` folded only the per-wave
  `HarmonicNumber`/`Phase`/`Amplitude` and omitted `Fundamental` — so two states differing only in
  `Fundamental` (e.g. 220 Hz vs 440 Hz) hashed identically yet diverge on the next step. `Fundamental`
  is serialized/restored and `SetParameter`-mutable, so those states are reachable across save/load or
  a mid-session change. Now folded. Locked by `MANIFOLD.Kernels.Waves.HashCoversFundamental`. **100/100
  green** (up from 99). This completes the hash-coverage sweep across **all seven kernels**.

### Robustness — replay hardening (Correspondence / Gameplay adversarial audit)
- A fresh 6-lens adversarial audit of the previously-unaudited **Correspondence engine + Gameplay +
  GameMode** surfaces (finders each a distinct lens → verifiers told to *refute* every finding and
  reproduce it against the real code) reported 5 candidates; **2 survived**, both fixed here, **99/99
  green** (up from 97). The 3 refuted were correctly killed as dead-code / unobserved (an
  `FName`-hash audio voice never compared across runs) or not-wired-to-untrusted-input.
  - **Unbounded replay `Steps` (compute-DoS).** `FManifoldReplay::Steps` is a raw `int32` in a
    shareable, UNTRUSTED replay, used directly as RunReplay's loop bound — each iteration steps all
    seven kernels + decoy + correspondence detection. The bounded-array guard covered array *counts*
    but not this *scalar*, so a crafted `Steps = 0x7FFFFFFF` turned a ~44-byte file into ~2.1e9
    iterations of full-CPU work. Now `FManifoldReplay::MaxSteps = 1,000,000` (~1000× any real
    session); LoadReplay rejects out-of-range up front and RunReplay clamps defensively. Locked by
    `MANIFOLD.Integration.ReplayStepsBounded`.
  - **Expert flag not persisted in replays (score/rank fidelity).** An expert (hidden-rule)
    constellation win earns +2500, but `FManifoldReplay` didn't carry the expert flag, so RunReplay
    rebuilt the puzzle as non-expert and reproduced a score short by 2500 — breaking the "reproduces
    exactly" contract. The flag is now an append-only **v3** field (forward-migrating: v1/v2 replays
    still load, flag defaults false); RecordConstellationReplay stores it and RunReplay honors it.
    Locked by `MANIFOLD.Integration.ConstellationExpertReplayScore`.

### Correctness — solver config carried through serialize/replay (kernel audit follow-up, final)
- **Per-realm solver config was not part of the serialized state.** Orbits `G`/`Softening`/`bFullNBody`
  and Fluids `Viscosity`/`Diffusion`/`Decay` drive their kernel every step and are mutable via the
  `IRealmKernel::SetParameter` contract, yet `FOrbitsState`/`FFluidsState` didn't serialize them — a
  state saved after a config change reverted to defaults on load and silently diverged the reproduced
  simulation. Config is now mirrored into the state (append-only serialized fields; no persisted
  save/replay embeds realm state, so the format change only touches the in-memory round-trip path),
  synced live↔state at `GetState()`/`SerializeState()`/`SetState()`, and folded into
  `ComputeStateHash` (a config-only divergence is a real divergence the detector must expose — same
  reasoning as the Mass / velocity-grid hash fixes). `SetState` treats the incoming config as
  **untrusted**: Fluids clamps negative `Viscosity`/`Diffusion` to ≥ 0 (the documented NaN →
  out-of-bounds hazard) and sanitizes non-finite `Decay`; Orbits sanitizes non-finite `G`/`Softening`.
  Locked by `MANIFOLD.Kernels.Orbits.ConfigRoundTrips` and `MANIFOLD.Kernels.Fluids.ConfigRoundTrips`
  (round-trip carries config + hash matches; a config-only divergence changes the hash; hostile config
  is sanitized so the next step stays finite). **97/97 green** (up from 95). This closes the last
  queued finding from the adversarial kernel/core audit — **all 8 confirmed defects are now fixed.**

### Correctness — orbital-element NaN guard (kernel audit follow-up)
- **`UOrbitsKernel::ComputeOrbitalElements` fed unclamped cosines into `FMath::Acos`** for the
  inclination, ascending-node, and argument-of-periapsis angles. A perfectly equatorial orbit has
  its angular-momentum vector aligned with +Z, so `hVec.Z/h` is `1.0` in exact arithmetic — but float
  rounding can nudge it a hair past ±1, and `Acos` of that is `NaN`, which then poisons the element,
  the period / mean-motion built on it, and the resonance math downstream. All three cosines are now
  clamped to `[-1,1]`, matching the already-clamped true-anomaly path in the same function. Locked by
  `MANIFOLD.Kernels.Orbits.OrbitalElementsFinite`, which asserts every orbital element stays finite
  across the edge cases that drive the cosines to their ±1 extremes (planar prograde/retrograde,
  near-circular, eccentric+inclined), at t=0 and after 20 steps. **95/95 green** (up from 94).

### Correctness — determinism-hash coverage (adversarial kernel/core audit)
- A 4-lens adversarial audit (determinism / detection / serialization / numerical) of the seven
  physics kernels + deterministic core, whose verifiers were told to *refute* every finding against
  the real code, confirmed **8** real defects. The two highest-leverage — blind spots in the
  divergence detector itself, plus an untrusted-input hardening — are fixed here, **94/94 green**
  (up from 91):
  - **`UFluidsKernel::ComputeStateHash` folded only the density grid `d`**, omitting the velocity
    fields `u`/`v` that drive advection and vortex detection every step. Two states differing *only*
    in velocity (byte-identical density) hashed identically, so a real simulation divergence slipped
    past `VerifyDeterminism` and the replay comparator. Now folds `u` and `v` with distinct per-grid
    multipliers. Locked by `MANIFOLD.Kernels.Fluids.HashCoversVelocity`.
  - **`UOrbitsKernel::ComputeStateHash` omitted per-body `Mass`**, yet mass drives the next step's
    accelerations (`Masses[j]/Masses[i]`). Two states with identical positions/velocities but a
    different mass hashed identically. Now folds `Mass`. Locked by `MANIFOLD.Kernels.Orbits.HashCoversMass`.
  - **`UFluidsKernel::SetState` trusted an untrusted `GridSize`.** `GridSize` and the grid arrays are
    serialized independently, so a corrupt/hostile snapshot could carry a `GridSize` inconsistent with
    the array lengths (next `Step` indexes out to `(Size+2)^2` → out-of-bounds read/write) or a huge
    `GridSize` whose int32 `(Size+2)^2` overflows. `SetState` now clamps `GridSize` to `[1, MaxGridSize]`
    and adopts the grids only when all three arrays are exactly `(GridSize+2)^2`; otherwise it falls
    back to a clean zeroed field at the clamped size. Valid self-consistent states are copied verbatim.
    Locked by `MANIFOLD.Kernels.Fluids.SetStateRejectsMalformedGrid`.
  - Both remaining findings are now fixed: the unclamped `Acos` (see *orbital-element NaN guard*
    above) and the config-serialization gap (see *solver config carried through serialize/replay*
    above). **Every confirmed finding from this audit is remediated.**

### Craft-quality pass (multi-lens review — coverage gaps + genuine simplifications)
- A craft-lens review (a different lens than the correctness audits: test-coverage completeness and
  code duplication, not bugs) surfaced four genuinely-untested behaviours and several verbatim
  duplications; all addressed, **91/91 green** (up from 87):
  - **Coverage — four untested behaviours now locked:**
    - `MANIFOLD.Kernels.Rhythm.ApproxRatioSearch` — the approximate small-integer ratio search and its
      MaxDeviation reject gate (every prior Rhythm test used integer tempos, so that whole branch was
      untested): 2.5:1 resolves to 5:2, and a near-irrational ~√2:1 is correctly rejected.
    - `MANIFOLD.Audio.HashFallbackVoices` — the deterministic hash voice every non-hand-tuned realm
      (Rhythm/Gears/Circuits) relies on: in-range, stable within a run, and genuinely distinct per realm.
    - `MANIFOLD.Play.InteractiveConstellationExpedition` — the GameMode-driven campaign end to end (level
      ramp, expert-from-level-3, running-score accumulation, and the `FMath::Max` bank into the profile
      best), a separate path from the already-covered static expedition so the two can't silently
      diverge. Backs up/restores the on-disk profile to stay hermetic.
    - `MANIFOLD.Play.ModeToggleCycle` — the `[C]` key's full Classic → Constellation → Expert → Classic
      cycle, including clearing the expert flag on the return to Classic.
  - **Simplifications (behaviour-identical, each pinned by an existing test):**
    - Removed **seven** verbatim `StepMultiple` overrides across the kernels; they duplicated the
      identical `IRealmKernel::StepMultiple` default, now simply inherited (pinned by
      `MANIFOLD.Systems.KernelStepMultipleEquivalence`).
    - Hoisted the copy-pasted `HashDouble` bit-cast lambda (six identical copies) into one shared
      `ManifoldHashDoubleBits` in `RealmKernel.h` — bit-identical output, pinned by the state-hash
      round-trip test.
    - Extracted the three-times-duplicated realm-glow material block into
      `AManifoldRealmVisualizer::ApplyGlow` (the starfield folds in too); verified pixel-identical via a
      headless render.

### Hardening (input / mode / audio — adversarial audit; whole-codebase coverage complete)
- The final un-audited surface (GameMode mode-switching/lifecycle, PlayerController input, HUD draw,
  ToneSynth audio) was adversarially audited — completing adversarial coverage of the entire codebase.
  Four defects confirmed (eight weaker claims refuted); all fixed, the expedition state leak locked by
  `MANIFOLD.Play.ExpeditionExit`:
  - **Restart `[R]` during a Constellation Expedition left a corrupt half-state** (high). `ManifoldRestart`
    only called `StartSession`, which re-generated the *current* level with the accumulated running
    total still banked, the banner still up, and no way out. Restart now leaves the expedition cleanly.
  - **Mode-toggle `[C]` didn't exit an active expedition** (medium): the flag/score leaked into Classic
    and silently resumed the old campaign on a later switch back. Toggle now leaves the expedition too.
    (Both fixed via a shared `ExitExpedition()` the generic reset verbs call.)
  - **ToneSynth `CachedSampleRate` data race** (low): written on the audio thread, read on the game
    thread outside the lock — now `std::atomic<int32>`.
  - **`[X]` restarting a campaign mid-run discards banked score** (low) — retained as the documented
    intent of the campaign-start verb (the clean exits are `[R]`/`[C]`).
  **87/87 green.**

### Hardening (gameplay glue — adversarial audit)
- An adversarial audit of `UManifoldSlice` (scoring / profile / constellation / transport) + Telemetry
  confirmed four defects (verifiers refuted eight weaker claims); all fixed, the two medium ones
  locked by `MANIFOLD.Play.ScoreOverflowSafe` and `MANIFOLD.Integration.LostDoesNotSetBest`:
  - **Speed-bonus int32 overflow (UB).** `(StepBudget − CurrentStep) * 10` overflowed int32 for a
    near-`INT32_MAX` budget passed via the unclamped public `SetObjective`, and could poison the
    persisted best. Now computed in int64 and capped.
  - **A Lost session could overwrite a Won best.** `RecordSessionInProfile` updated the best on
    `Score > best` with no win check; a high-discovery *loss* overwrote a legitimate best and reported
    "new best!". Best updates are now gated on `State == Won`.
  - **Replay lied about a stale-vortex transport (low).** `DoTransportPendingVortex` discarded
    `Transport()`'s result and `PlayerRequestTransport` always returned true; they now report whether
    a transport actually fired.
  - **Leaked telemetry file handle (low).** `RecordConstellationReplay` never called
    `ShutdownTelemetry` (held the log open until GC); now closed like every other session exit.
  **86/86 green.**

### Hardening (Fluids kernel — adversarial audit)
- A focused multi-agent audit of the previously un-audited kernel physics + mesh builders confirmed
  two real Fluids-kernel defects (both fixed, locked by `MANIFOLD.Kernels.Fluids.RobustParamsAndUniqueVortexIds`):
  - **Negative diffusion params → NaN → out-of-bounds.** `SetParameter("Viscosity"/"Diffusion", …)`
    accepted negatives unchecked; a negative diffusion drove the Diffuse implicit-solve denominator
    `1 + 4a` to zero → NaN, which bypassed Advect's range clamps (NaN compares false to both bounds)
    and cast to an out-of-bounds array index (crash in shipping). Params are now clamped to `>= 0`,
    and Advect snaps any non-finite advection coordinate back to its cell.
  - **Vortex stable-id collision.** The id quantized position by a fixed 50 units — coarser than the
    ~31.25-unit grid spacing (`1000/Size`) — so two *distinct* vortices in adjacent cells collapsed to
    one `FGuid`, breaking the unique-structure-id contract. IDs are now quantized per grid cell.
  **84/84 green.**

### Hardening (correspondence core — adversarial audit)
- **Locked the load-bearing octave-decoy distinctness contract.** An adversarial audit of the
  matching core (`NormalizeRatio` / `DetectSharedStructureCorrespondences` / constellation
  generation) confirmed the engine is sound — no reachable false-match, ambiguous/unsolvable puzzle,
  or non-determinism — and surfaced one latent contract weakness: `NormalizeRatio(OctaveInvariant)`
  is order-sensitive (unlike `Reciprocal`). Investigation showed this order-sensitivity is
  **intentional and load-bearing**: the octave decoys depend on it (`4:3 → "1:3"` must stay distinct
  from the base-3 member class `3:1`; the audit's suggested "canonicalize order" fix would have
  collapsed decoys onto member classes and broken discrimination). Documented the directional
  contract in `NormalizeRatio` and locked it with `MANIFOLD.Correspondence.OctaveDecoyDistinctness`
  (asserts every family collapses, families stay distinct, and no decoy lands on a member class).
  **83/83 green.**

## [0.9.0] — 2026-07-02 — "Correspondence Engine Complete"

_First tagged release. The entire code-implementable game is built, verified (82/82 automation
tests, headless), and shippable: seven realms + a decoy, a relation-aware N-realm correspondence
engine, two full play modes (Classic + Constellation Lock) with Expert and a playable Expedition
campaign, deterministic shareable replays, scoring/ranks/per-mode profiles, procedural audio +
starfield + emblem + branded HUD, Enhanced Input, and a headless packaging path. Save/load is
forward-migrating and hardened against untrusted replay files; the realm palette is colorblind-safe.
The remaining pre-1.0 lane is the bespoke art/VFX pass (see `Docs/ART_DIRECTION.md`), which needs a
display + art direction._

### Art / Accessibility (colorblind-safe realm palette + direction doc)
- **Realm colours are now colorblind-safe and centralised.** The seven realm colours moved from
  ad-hoc literals in the visualizer (which had near-identical teal Waves/Circuits and blue
  Orbits/Fluids pairs) into a single source of truth,
  [`ManifoldPalette.h`](Source/Gameplay/Public/ManifoldPalette.h), using the **Okabe-Ito**
  colorblind-safe palette (sRGB→linear). Locked by `MANIFOLD.Art.PaletteColorblindSafe`, which
  simulates deuteranopia / protanopia / tritanopia (Machado 2009 matrices) and asserts every realm
  + the decoy stays pairwise-distinguishable. Measured min separation clears the bar under all
  conditions (tightest: deuteranopia 0.083, Waves vs Decoy). The game asks players to tell realms
  apart by colour, so this is a real accessibility requirement, not decoration.
- **[`Docs/ART_DIRECTION.md`](Docs/ART_DIRECTION.md)** — a concrete, executable plan for the bespoke
  art/VFX pass (per-realm Niagara/material language, the seam "money shot", scene/lighting/post, UMG
  reskin, and the asset-pipeline inputs still needed). The remaining visual work is dressing, gated
  on a display + art direction; the logic, palette, and render hooks are in place. **82/82 green.**

### Balance (data-driven playtest — headless sweep + a scoring fix it found)
- **`MANIFOLD.Balance.Sweep`** drives both modes across 128 seeds, logs the difficulty/scoring/
  economy distributions (via `UE_LOG`, capturable from a standalone `-stdout` run), and asserts
  the objective fairness/discrimination invariants a shipping build must hold. Baseline: Classic
  discoveries are fair (16–17/seed, all reach full realization), the decoy never false-matches,
  every constellation is solvable (128/128), and the Exact/Octave rule split is a perfect 64/64.
- **Fix the sweep found — Classic score was insight-swamped.** `GetInsightRate()` is discovery-
  events per *sim-second* (arbitrary denominator across realm mixes), reaching ~1060; `rate*100`
  added ~106k, dwarfing the ~16k of discoveries and collapsing **every** Classic run to rank S.
  Clamped insight to a modest, discovery-subordinate bonus (≤250) and capped the (negligible)
  transport term at the discovery count. Classic score is now discovery-driven (~16.5k at ceiling)
  and the rank curve differentiates (D 0–2 · C 3–4 · B 5–6 · A 7–8 · S 9–16 discoveries). Locked
  by a `notDiscDominated == 0` regression invariant. `Docs/PLAYTEST.md` records the baseline + the
  two rank-threshold *feel* calls that still need the human. **81/81 green.**

### Security (untrusted-input hardening — replay deserialization DoS)
- **Shareable replays are untrusted input; a crafted file can no longer OOM-crash the game.**
  A focused multi-agent adversarial audit of the save/load parsing surface confirmed one
  high-severity defect (and correctly refuted two over-claims). `LoadReplay` deserializes the
  `TArray<int32>` fields `TransportSteps`/`LockSelection` via UE's default `TArray::operator<<`,
  which reads an attacker-controlled `int32` count prefix and **pre-allocates `count*sizeof(T)`
  before reading any element** — and UE's 16 MB safety cap applies *only to net archives*, so a
  non-net `FMemoryReader` bypasses it. A 32-byte replay claiming a count of `0x7FFFFFFF` forced an
  ~8 GB allocation → fatal OOM *before* the existing `IsError()` guard could run. Fixed with
  `FManifoldReplay::SerializeBoundedInt32Array`, which rejects any count that can't be backed by
  the bytes actually remaining in the file (a tight, exact bound) *before* allocating. Write path
  is byte-for-byte identical, so the on-disk format is unchanged. `LoadProfile` is unaffected
  (scalar fields only). Test: `MANIFOLD.Integration.MaliciousReplayRejected` feeds hostile-count
  payloads (both `TArray` fields) and asserts graceful rejection, plus a legit replay to prove the
  guard doesn't over-reject. **80/80 green.**

### Fixed (save robustness — forward migration across format bumps)
- **Older saves migrate forward instead of being wiped.** The profile/replay loaders used an
  all-or-nothing `Version != current` check, so shipping any update that bumped the save format
  (v1→v2→v3 already did this twice) would silently discard a returning player's career bests.
  Git history confirms the fields are strictly append-only, so an older save is a valid *prefix*
  of the current layout. A new `SerializeVersioned(Ar, version)` reads exactly the fields present
  at the file's on-disk version and defaults the newer ones — the player's progress survives the
  update. Newer-than-current files are still rejected (unknown layout); the truncated-file
  `IsError()` guard is intact. Applied to both `FManifoldProfile` and `FManifoldReplay` (a v1
  Classic-only replay now still plays back). Test: `MANIFOLD.Integration.SaveForwardMigration`
  hand-builds real v1 / v2 / future payloads and asserts each path. **79/79 green.**

### Added (kernel contract coverage — complete)
- **`Reset()` contract test:** `MANIFOLD.Systems.KernelReset` exercises the last untested
  `IRealmKernel` method across all **7** kernels — init → add → step → hash, then `Reset()`,
  re-add → re-step, and asserts the hash reproduces. This proves `Reset()` returns each kernel
  to a clean, reproducible post-init state with no leftover state leaking across a reuse. With
  the earlier serialization round-trip and `StepMultiple == N × Step` tests, every method on the
  realm-kernel contract now has dedicated coverage. **78/78 green.**

### Added (integration coverage)
- **Mixed-mode career test:** `MANIFOLD.Integration.MixedCareerProfile` composes a Classic
  win and a Constellation win onto one profile and asserts the separate per-mode bests,
  combined session counts, and a full save/load round-trip all hold together — integration
  coverage the per-feature unit tests didn't have. **71/71 green.**

### Added (seventh realm — Circuits)
- **Circuits** (electromagnetism): two resonant LC tanks whose frequencies form the exact
  integer ratio P:Q — the same abstract structure as an orbital P:Q, in charge oscillation.
  One hidden ratio now spans **seven** domains. New `MANIFOLDKernelsCircuits` module wired
  into Classic as a sixth sharing realm (the discovery ceiling rises to 1 seam + C(6,2)=15
  = 16; the expedition wall and decoy-inflation checks were retuned). Tests:
  `MANIFOLD.Kernels.Circuits.{ResonanceRatio,ExactRatio,Deterministic}`. **70/70 green.**

### Fixed (whole-codebase adversarial audit — determinism)
A fresh multi-agent audit of the (much larger) codebase confirmed **2** latent defects,
both now fixed with regression tests (**66/66 green**):
- **Content-stable structure ids.** The "stable, deterministic" structure ids hashed
  `GetTypeHash(FName)` — the process-local name-table index, not the string bytes — so
  they were reproducible only within one process, not across runs/builds/platforms. Now
  hashed from the realm-id *string* in `CorrespondenceSystem` and all six kernels.
  Gameplay was unaffected (in-run dedup keys on the ratio string), but any persisted id
  would have silently mismatched. Test: `MANIFOLD.Correspondence.StableIdContentStable`.
- **Complete session reset.** `bAutoTransportOnIgnite` was the one field `Setup` never
  reset, so a slice reused after a replay leaked the flag. Both setups now restore the
  default; callers set it after `Setup`. Test: `MANIFOLD.Play.SliceReuseResetsAutoTransport`.

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
- **Reachable & playable.** A play-mode toggle (`EManifoldPlayMode`) lets the GameMode run
  either Classic or Constellation; `[C]` switches modes and rolls a fresh puzzle seed,
  `[1]`–`[6]` pick realms, `[Space]` locks, `[R]` starts a new puzzle. The HUD gains a
  Constellation readout — the six surface ratios, the current pick, the active rule hint,
  wasted probes, objective and a "CONSTELLATION LOCKED" banner with rank. A 96-seed
  realization sweep (`MANIFOLD.Play.ConstellationRealizationSweep`) proves every kernel
  realizes its assigned ratio and no decoy spuriously corresponds across the seed space.
  A 5-dimension adversarial review returned zero confirmed findings.
- **Shareable & reproducible.** A constellation session records as a `.manifoldreplay`
  (format bumped to v2: mode + constellation size + locked subset) and reproduces
  bit-for-bit from its seed on a fresh slice — `RecordConstellationReplay` /
  `RunReplay` (mode-aware). Test: `MANIFOLD.Play.ConstellationReplay` (record → reproduce
  → save/load → reproduce).
- **Meaningful rank.** Constellation scoring now grades difficulty and precision: an
  Octave solve (the rule is harder to infer) earns a bonus over an Exact solve, a flawless
  (no wasted probe) solve earns a bonus, and every wrong lock costs points — so the S/A/B/
  C/D rank actually reflects skill. Test: `MANIFOLD.Play.ConstellationScoring`.
- **Expert mode (the full design).** Cycling `[C]` a second time hides the rule itself
  (`Rule: ???`) — the player must infer whether the session is Exact or Octave from the
  ratios alone, the design's complete inference challenge, worth a win bonus. Same puzzle,
  no hint. Test: `MANIFOLD.Play.ConstellationExpert`. The whole game still cooks into a
  shippable standalone build (packaging SUCCESS); two adversarial audits (feature + cross-
  module integration) returned zero findings. **60/60 green.**

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
- **87 / 87** automation tests green, headless. Repo is public and professional.
  Remaining phase (real art/VFX scenes, bound sound assets, bespoke UMG UI, human
  playtest) is human-owned and needs the editor + a display — see
  `Docs/IMPLEMENTATION_STATUS.md`.
