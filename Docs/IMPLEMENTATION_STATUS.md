# MANIFOLD — Implementation Status & Handoff

_Everything marked ✅ is verified by an automation test that passes headless._

## 1. TL;DR

MANIFOLD is now a **playable, deterministic, data-driven, N-realm correspondence
game** with a professional, public repository and CI scaffolding.

**25 automation tests pass, 0 failures, headless.** The correspondence engine —
the product's core per the design bible — is complete and generalized to N realms;
a playable shell drives it with an on-screen readout, a debug-draw view of the
realms, and a player transport verb. Four realms now share a 3:2 across different
physical domains.

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

### Realms (production template, §9) — a 3:2 across four domains
- ✅ **Orbits** (celestial: mean-motion resonance)
- ✅ **Fluids** (vortex)
- ✅ **Harmonics** (acoustic: coupled-oscillator frequency ratio)
- ✅ **Waves** (spatial: string standing-wave harmonics) — `MANIFOLD.Kernels.Waves.*`

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
  front end: fixed-cadence session, on-screen readout, debug-draw of realms +
  gold resonance/seam ribbons, console verb `ManifoldTransport`. (Visual output is
  seen at the editor/display; the logic behind it is test-verified.)

## 3. How to play / verify

**Play (at the editor):** open the project, open any empty level, press Play.
`GlobalDefaultGameMode` is `ManifoldGameMode`, so the slice runs with the HUD and
the debug-draw visualizer. The `ManifoldTransport` console command fires the verb.

**Verify (headless):**
```powershell
Tools\CI\run_tests.ps1      # build + all MANIFOLD automation tests (expect 25 Success, 0 Fail)
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

## 5. What still needs a human + art/audio pipeline

The engine is decoupled so these plug into the existing kernels/queries/events:

- **World / VFX** — real realm scenes and the polished magic layer (Niagara
  resonance ribbons, biolum, post) replacing the debug-draw placeholder.
- **Audio** — realm musical modes + chord-resolve on discovery (M4 Max pipeline).
- **UI** — a real HUD/menus replacing the debug text.
- **Playtest with humans** — the numeric gate is coded; a felt playtest is not.

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
