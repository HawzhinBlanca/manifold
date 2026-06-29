# MANIFOLD — End-to-End Build Plan (Agent-Ready)

**One master document for the whole game, broken into self-contained work packages (WPs) you can finish one-by-one, or hand each to a separate AI agent or person.**

This is the source of truth. Every WP is written so that whoever picks it up — you, a Claude Code/Codex agent, or a hired specialist — has the goal, the inputs, the steps, the deliverable, and a **mechanically verifiable acceptance test**. Nothing is "done" until its test passes. That's the rule that lets you parallelize without chaos.

---

## 0. How to use this document

**Two execution modes, same WPs:**
- **Solo, one-by-one:** work the WPs in the order given in §6 (dependency graph). Don't start a WP whose dependencies aren't green.
- **Parallel (agents/people):** assign whole *streams* (§4) to different agents/people using the ownership map in §7. Each owns a folder; they never edit outside it; integration happens on a shared branch on a fixed cadence.

**The WP contract (every package has these):**
`ID · Title · Owner-type · Rig · Inputs · Deliverable · Acceptance test · Depends-on`

**The non-negotiable gate:** a WP merges only when its acceptance test passes in CI (§2 / §11). No exceptions, no "trust me." This is what makes multi-agent work safe.

---

## 1. Scope: what's locked vs. what YOU decide

**Locked (in this doc):** the engine path (UE5), the assemble-don't-model art method, the system architecture, the decomposition method, the vertical-slice WBS, the agent protocol, the verification rules.

**Decision slots — fill from your research (marked ⟐ throughout):**
- ⟐ **Final realm list** and the **specific correspondences** between them (creative — yours).
- ⟐ **Photoreal vs. stylized** per realm (default: photoreal for sky-heavy realms, stylized for dense-organic).
- ⟐ **Target platform(s)** + performance budget (default: PC/Windows-first on the 4090).
- ⟐ **Community-canon / multiplayer** scope and *when* it turns on (default: post-vertical-slice).
- ⟐ **When to bring in people** vs. agents (default: solo+agents through the slice, specialist to finish).

Until you set a slot, the default in parentheses is assumed so nothing blocks.

---

## 2. Vision & design pillars (the bible — do not drift)

- **The cheat:** the game is built on cross-domain analogy. You discover that two realms share one hidden structure, then transport power across the seam. The "aha" *is* the gameplay.
- **Truly unpredictable, by design:** novelty lives at the **seams** (emergent collisions between realms) and in the **community canon** (player-found correspondences). This is the north-star you keep naming — the experience must stay un-pre-computable. *Everyone else builds the predictable path; the moat is that this one can't be fully solved.*
- **Compounding, not saturating:** mastery is fuel — solving one realm gives the eyes to see its structure in the next.
- **Art direction:** lush, believable worlds unified by a single signature **resonance** language (gold light along every shared structure). One magic layer, reused everywhere (§ realm template).
- **Lean rule:** the **correspondence engine is the product**; visuals are a layer. Depth first, beauty second. Don't let art eat the schedule.

> Any WP that conflicts with a pillar is wrong. Pillars beat tasks.

---

## 3. System architecture & repo structure

**Tech:** UE5.x · C++ for kernels/core, Blueprints for glue · data-driven content (JSON/DataAssets) · Niagara VFX · deterministic fixed-step sim.

**Repo layout (ownership boundaries are literal folders):**
```
/Source/Core         → deterministic sim, RNG, replay        [Systems agent]
/Source/Kernels      → one module per realm                  [Systems agent]
/Source/Correspond   → mapping, detection, transport         [Systems agent]
/Source/Telemetry    → Insight-Rate logging                  [Systems agent]
/Content/Realms/<X>  → per-realm environment art             [World owner]
/Content/VFX         → resonance, biolum, ignite (reusable)  [VFX owner]
/Content/Audio       → modes, SFX, adaptive hooks            [Audio owner]
/Data/Correspondences→ the correspondence specs (data)       [Design owner]
/Tools/CI            → build + headless test harness         [Infra owner]
/Docs                → this bible + per-WP briefs            [shared]
```
Rule: an owner edits only their folder(s). Cross-folder needs go through a defined interface, not a reach-in.

---

## 4. The seven streams (the lanes you assign)

| Stream | Code | What it produces | Best owner |
|---|---|---|---|
| Infrastructure | **I** | repo, version control, CI, agent protocol | Agent or you (first) |
| Systems / Code | **S** | kernels, core, correspondence, transport, telemetry | Coding agent(s) |
| World / Environment | **W** | assembled realm scenes | You + asset/AI |
| VFX / Magic Layer | **V** | resonance, bioluminescence, ignite, post | VFX agent or you |
| Design / Content | **D** | the correspondences, onboarding, tuning | You |
| Audio | **A** | realm modes, SFX, adaptive audio | You (your domain) |
| Production / QA | **P** | gates, playtests, metrics | You |

---

## 5. PHASE 1 — Vertical Slice (fully decomposed)

**Goal of the phase:** prove the loop is fun and that the Insight Rate compounds — with two realms (**Orbits + Fluids**) and one correspondence between them. This is the only phase specified to the task; later phases reuse these templates (§10).

### Stream I — Infrastructure
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| I1 | Version control + UE5 project skeleton + naming/folder conventions | Agent/You | B | Clean repo, empty project | Fresh clone opens in UE5, builds, runs empty level | — |
| I2 | CI + headless test harness (the merge gate) | Agent | A | Automated build + test runner | A sample failing test blocks merge; passing one allows it | I1 |
| I3 | Agent workspace protocol (ownership map, integration cadence) | You | — | 1-page protocol in /Docs | Two agents run in parallel with zero cross-folder edits | I1 |

### Stream S — Systems / Code
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| S1 | `RealmKernel` interface (state, step, query, serialize) | Agent | B | Interface + dummy kernel | Unit test instantiates dummy, steps with seed | I1 |
| S2 | Deterministic core (seedable RNG, fixed-step, replay) | Agent | B | Core module | Same seed → identical state-hash over N steps; replay reproduces a session | S1 |
| S3 | Orbits kernel (Keplerian/n-body, resonance ratios exposed) | Agent | B | Kernel module | Deterministic stable orbits; resonance query returns correct ratios | S1,S2 |
| S4 | Fluids kernel (flow/vortex, feedback params, flow-structure query) | Agent | B/A | Kernel module | Deterministic; flow-structure output matches mapping fixture | S1,S2 |
| S5 | Correspondence system (data-driven mapping + detect + validate) | Agent | B | Module + API | Given Orbits↔Fluids data, flags valid, rejects invalid (fixtures) | S3,S4 |
| S6 | Transport mechanic (re-express A's solution in B via mapping) | Agent | B | Module | Defined transport produces the specified state change in B (test) | S5 |
| S7 | Lazy realization (detail-on-observation from params) | Agent | B | Module | Same region+seed → identical detail twice; memory bounded | S2 |
| S8 | Insight-Rate telemetry (discovery/transport events → log) | Agent | B | Module + schema | Scripted playthrough emits correct events to log | S5,S6 |

### Stream W — World / Environment
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| W1 | Concept bible + LUT for Orbits & Fluids (reuse our frames) | You | A | 1-page bible + LUT each | Bible names palette, mood, resonance placement | — |
| W2 | Orbits scene: AI skybox + Megascans platform + hero prop | You | A→B | Assembled level | Matches bible at fixed camera; ≥ target fps on 4090 | W1,V4 |
| W3 | Fluids scene: water material + Megascans rock/cliff | You | B | Assembled level | Matches bible; runs at target fps | W1,V2,V4 |
| W4 | Hero-prop pipeline (AI-3D → Blender retopo → Substance → UE) | You/Agent | A→B | 2–3 game-ready props | Under tri-budget, PBR material, matches bible | W1 |

### Stream V — VFX / Magic Layer (build reusable, once)
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| V1 | Resonance ribbon system (Niagara/spline emissive, parametric) | VFX/You | B | Reusable VFX asset | Drops into any scene, follows spline/data, blooms | I1 |
| V2 | Bioluminescence material function (emissive + glow particles) | VFX/You | B | Reusable material fn | Applied to water/flora, reads like concept | I1 |
| V3 | Correspondence-ignite burst (fires on discovery event) | VFX/You | B | VFX + trigger | Triggers from S8 event; visible flash | V1,S8 |
| V4 | Volumetric light + cinematic post-process preset (per realm) | VFX/You | B | Post preset | Grading matches concept frame | W1 |

### Stream D — Design / Content
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| D1 | The Orbits↔Fluids correspondence spec (the shared structure, what transports, fairness) | You | — | Spec + the data file S5/S6 consume | Playtesters report "I could've seen it" in hindsight | — |
| D2 | Onboarding/teaching flow for the slice | You | — | Flow doc + in-level beats | New player hits first correspondence within target time | D1 |
| D3 | Insight-Rate target + no-correspondence control build | You | — | Metric def + control variant | Control build exists; thresholds written | S8 |

### Stream A — Audio
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| A1 | Realm musical modes (Orbits, Fluids) + correspondence chord-resolve | You | Mac | Stems + cue map | Each realm a mode; discovery resolves a chord | — |
| A2 | SFX + adaptive audio hooks | You | Mac/B | SFX + hooks | Discovery/transport events trigger audio via S8 | A1,S8 |

### Stream P — Production / QA
| ID | Work package | Owner | Rig | Deliverable | Acceptance test | Dep |
|---|---|---|---|---|---|---|
| P1 | Vertical-slice gate definition (go/no-go criteria) | You | — | Gate doc | Criteria are numeric + agreed | D3 |
| P2 | Playtest round + metrics readout → gate decision | You | — | Report + decision | N testers; Insight-Rate vs control collected | all |

---

## 6. Execution sequence & dependency graph

**Critical path (must be serial):**
`I1 → S1 → S2 → {S3, S4} → S5 → S6 → D1 → integrate → P2 (gate)`

**Runs fully in parallel with the critical path:**
- **VFX lane:** V1, V2, V4 can start right after I1 (only need the project). V3 waits for S8.
- **World lane:** W1 immediately; W2/W3 once V4 (post preset) and the relevant V assets exist.
- **Audio lane:** A1 immediately; A2 once S8 exists.
- **Infra:** I2, I3 right after I1.

**Integration points (where lanes meet):**
1. Systems + Telemetry (S8) ↔ VFX ignite (V3) and Audio (A2).
2. Kernels (S3/S4) + Scenes (W2/W3) → playable realm.
3. Everything → P2 gate.

> One person can walk the critical path while up to **four agents** run the parallel lanes. That's your "multiple agents" answer.

---

## 7. Multi-agent / multi-person assignment map

| Owner | Stream(s) | Folder(s) they own | Never touch |
|---|---|---|---|
| **Agent-Sys** | S (all) | /Source/* | /Content/* |
| **Agent-VFX** | V (all) | /Content/VFX | /Source/* |
| **Agent-Infra** | I | /Tools/CI | gameplay code |
| **You** | D, A, P + drive W | /Data, /Content/Realms, /Content/Audio, /Docs | — |
| **Specialist (later)** | finishing pass | by agreement | — |

Collision rule: cross-boundary need → define/extend an **interface** (a header, a DataAsset schema, an event), then each side codes to it. Agents request interface changes via a /Docs note, you approve, then both proceed.

---

## 8. Agent Work-Package brief template (copy-paste to spin up any WP)

```
ROLE: You are implementing a single work package for MANIFOLD. Read /Docs/bible.md first.
WP: <ID — Title>
GOAL: <one sentence from the table>
CONTEXT/INPUTS: <interfaces, data files, the concept bible/LUT, dependency outputs>
CONSTRAINTS:
  - Edit ONLY these folders: <owned folders>. Touch nothing else.
  - Respect the design pillars (§2). Deterministic where the core requires it.
  - Lean: smallest correct implementation. No speculative features.
STEPS: <the brief steps from the WP>
DELIVERABLE: <the artifact>
ACCEPTANCE TEST (must pass in CI before you report done):
  <the exact command / assertion / hash / fps check from the table>
DONE = test green in CI AND no edits outside owned folders. If blocked by a
missing interface, write the request to /Docs/interface-requests.md and stop.
```
This single template is how you hand *any* row in §5 to an agent or a person.

---

## 9. Realm Production Template (clone this per realm — the repeating unit)

Every new realm after the slice is the **same five WPs**, cloned and renamed:
1. **S-kernel** — the realm's simulation (acceptance: deterministic + exposes its structure for mappings).
2. **W-scene** — assemble (skybox/terrain + Megascans + hero props) to the realm's concept bible.
3. **V-apply** — apply the reusable magic layer (resonance/biolum/post) tuned to the realm.
4. **D-correspondence** — author at least one correspondence linking it to an existing realm (the actual content).
5. **A-mode** — its musical mode + the chord-resolve for its correspondences.

⟐ For each realm you add to your list, copy these five rows, fill the specifics, assign. That's how the whole game scales — and how the rest of the WBS "writes itself" without me inventing fake tasks.

---

## 10. Phases 2–5 at template level (how the rest expands)

The method above *is* the plan for the whole game. Later phases add WP **types**, not new methods:

- **Phase 2 — Production:** clone the realm template ×(8–12) ⟐; add the **generative content pipeline** (S-type: AI proposes realms/correspondences → human-curate) and **content tooling** (I/Tools). Gate: cost-per-realm halved.
- **Phase 3 — Early Access:** add the **community-canon service** (S-type backend: submit → validate → fold into /Data) and live-ops tooling; ship on Steam EA. Gate: player-found correspondences entering canon.
- **Phase 4 — Launch 1.0:** **optimization pass** (perf budgets, Nanite/LOD/streaming), **console** target ⟐, marketing. Gate: review/commercial KPIs.
- **Phase 5 — Live Growth:** creator/UGC realm tools (the realm template, exposed to players), sustained generative + community expansion.

Each bullet expands into WP rows using §8 + §9 when you reach it. Don't pre-write them now — write them one phase ahead, so they reflect what you learned in the gate before.

---

## 11. Definition of Done & verification rules

- **Code WP:** acceptance test green in CI; deterministic where required; only owned folders touched.
- **Art WP:** matches the realm's concept bible at the reference camera; within tri/perf budget.
- **VFX WP:** reusable (parametric), fires from the correct event, reads like the concept.
- **Design WP:** the correspondence passes the **fairness check** (hindsight "I could've seen it") in playtest.
- **Audio WP:** triggered by real game events, not hand-synced.
- **Phase gate:** numeric criteria met (esp. Insight Rate positive vs. control) → go/no-go, in writing.

**Golden rule for agents and people alike:** *nothing is done because it looks done. It is done when its test is green.*

---

## Appendix A — Tool stack (per stream)
- **Engine/World:** UE5.x, Fab/Quixel Megascans, Blender, Substance 3D (Painter/Designer/Sampler), Blockade Labs Skybox AI.
- **AI (Rig A):** ComfyUI + FLUX.1-dev/SDXL (concept, textures); Hunyuan3D-2 / TRELLIS / InstantMesh (props); Wan/Hunyuan-Video (trailers, later); local LLMs via vLLM/Ollama across NVLink.
- **Code:** Claude Code / Codex CLI; local LLMs for bulk tasks.
- **Infra:** Perforce Helix Core (free ≤5) or Diversion/Anchorpoint; CI harness for headless UE tests.
- **Audio (Mac):** your existing pipeline.

## Appendix B — Risk register (top 6)
1. Correspondence "surprising-yet-fair" hard → instrument Insight Rate from slice; gate kills early.
2. Art eats schedule (solo death) → assemble-don't-model; one realm at a time; depth-first.
3. Multi-agent collisions → folder ownership + CI gate + interface-request flow (§7,§8).
4. Perf debt from assembled scenes → budget at Phase 4; slice can run looser.
5. Photoreal-organic too hard solo → stylize dense realms ⟐.
6. Scope creep across phases → write WPs one phase ahead only.

## Appendix C — Glossary
Synthesist · Realm · Correspondence · Transport · Lazy realization · Deterministic core · Insight Rate · Canon · Magic layer · Resonance language. (Definitions in /Docs/bible.md.)

---

*Hardware: Rig A = TR 3990X / 2× RTX 3090 Ti NVLink / 256 GB (the AI + render farm); Rig B = RTX 4090 / ~192 GB (build + real-time); + M4 Max (audio). Engine: Unreal Engine 5. ⟐ = your decision slot. Correct any wrong assumption and the affected WPs update.*
