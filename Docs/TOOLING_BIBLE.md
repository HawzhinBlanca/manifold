# MANIFOLD — Production Accelerator & Tooling Bible
### Verified-current edition · last checked 29 June 2026

This is the companion to the **Build Plan**. The Build Plan is *what to do and in what order*. This is *everything else that makes it easier and better* — the exact tools, models, and systems to use, all **verified shipping as of late June 2026**.

**The no-fake / no-dead-end doctrine:** every tool named here is real and available right now. Where a shiny frontier is **not** production-ready (promptable game worlds, UE6), it's flagged in red so you don't waste a week chasing it. When a step is risky, a fallback is given.

---

## 1. The model & agent stack (your coding superpower)

The frontier compressed hard in 2026 — the top models are separated by single points, and **the agent scaffolding now matters more than the model** (the same model swings ~17–21 points just from the harness). So pick a great agent (Claude Code) and a great default model, then route the rest.

**Use, verified June 2026:**

| Role | Pick | Why / notes |
|---|---|---|
| **Primary coding agent** | **Claude Code + Claude Opus 4.8** (`claude-opus-4-8`, $5/$25, 1M ctx) | #1 overall + top coding (88.6% SWE-bench Verified, 69.2% SWE-bench Pro). The default for sustained, multi-file engineering. This is what you're already using. |
| **Volume / daily** | **Claude Sonnet 4.6** ($3/$15, 1M ctx) | ~98% of Opus quality at a fraction of cost — most of your day. |
| **Subagents / quick edits** | **Claude Haiku 4.5** ($1/$5) | Cheapest fast tier; parallel subagents. |
| **Second opinion / CLI agent** | **GPT-5.5** ($5/$30) + **gpt-5.3-codex** | Best agentic-CLI + structured output; good for terminal/DevOps and a different perspective on hard bugs. |
| **Huge-context reads** | **Gemini 3.1 Pro** ($2/$12, 1M; cost ~2× over 200K) | Cheapest flagship; "find all usages across the whole repo" in one pass. |
| **Local bulk (Rig A, free)** | **MiniMax M3** (80.5% SWE-bench, open) / **DeepSeek V4** / **Qwen3.7** | Run across the 2× 3090 Ti (NVLink pools 48 GB) for mass test-gen, refactors, content — no API cost. |

> ⚠️ **Not available:** Claude **Fable 5** (95.0% SWE-bench) and **Mythos 5** (95.5%) are real but **suspended / invite-only** as of now. Don't plan around them. **GPT-5.6 does not exist** — the latest OpenAI flagship is GPT-5.5.

**How to run multiple agents (ties to the Build Plan §7–8):** Claude Code supports subagents; give each a single work package, its owned folder, and its acceptance test. The merge gate (CI) is what makes parallel agents safe. Other agent shells that exist now if you want them: Cursor, Cline, Codex CLI, OpenClaw, ForgeCode, Gemini CLI.

---

## 2. The engine — decided

**Build on Unreal Engine 5.8 (current). Do not wait for UE6.**

**UE 5.8 (released 17 June 2026) gives you, production-ready:**
- **MegaLights** — thousands of dynamic lights with fixed-cost shadows (huge for the resonance/bioluminescence look, cheaply).
- **Lumen** (+ Lumen Lite) — real-time GI; the cinematic lighting that closes the "AAA gap."
- **Mesh Terrain + new PCG tools** — procedurally scatter Megascans rock/foliage instead of hand-placing (your world-assembly engine).
- **Nanite** — drop full-detail meshes, no LOD babysitting.
- **MetaHuman Devkit + Crowd + markerless mocap** — characters and crowds without a mocap stage.
- **Movie Render Graph** — path-traced cinematics on Rig A.

**Version control:** Epic just **open-sourced "Lore,"** their next-gen VCS (announced June 2026) — built for big binary game projects and friendlier than Perforce. Option A: **Lore**. Option B: **Perforce Helix Core** (free ≤5 users, still the safe standard). Either beats plain Git for UE.

**Bleeding edge (optional):** an **experimental MCP plugin** lets Claude operate *inside* the UE project (assemble levels, adjust lighting via prompts). Try it for tedious setup; keep it experimental.

> ⚠️ **UE6 is NOT a now-tool.** Early Access ~end of 2027, stable ~2028–29. It will move scripting to **Verse** (and eventually deprecate Blueprints, with conversion tools) and make AI **MCP-native** (Claude/Gemini/Codex first-class). Net: build on 5.8 today; Blueprints + C++ are fine for years; you'll migrate later with tooling. Don't stall waiting for it.

---

## 3. The AI content pipeline, by asset type (all verified, with where it runs)

**[A]** = Rig A farm (self-host, free/unlimited) · **[Cloud]** = paid API/web.

### Concept art & textures
- **FLUX.2** (Black Forest Labs, open weights) **[A]** — the self-host winner; great consistency. Run in **ComfyUI** on the farm, batch overnight. Use **FLUX Kontext** for edits. *This is your free, unlimited concept + texture engine.*
- **Nano Banana Pro** (Google) **[Cloud]** — top photoreal + best text rendering, 4K, when you want a hero frame.
- **Seedream 4.5/5** **[Cloud]** — image-to-image editing with up to ~10 reference inputs (style-match assets to your bible).
- **Midjourney** **[Cloud]** — best pure artistic quality for mood.
- Convert any concept/texture image → PBR material with **Substance 3D Sampler**.

### 3D assets (the realm props)
- **Hunyuan3D** (Tencent, open) **[A]** — self-host on the farm, **unlimited & free**, closed-source-grade geometry + PBR. *Your bulk prop workhorse.*
- **Rodin Gen-2** (Hyper3D, 10B params) **[Cloud]** — best quality, clean quad topology, PBR, T/A-pose — for **hero assets**. Free to generate, pay to download.
- **Tripo v3 / Meshy 6** **[Cloud]** — fast, **auto-rigging + animation presets** — for characters/creatures that need to move.
- **TRELLIS 2** (Microsoft, free) **[A/Cloud]** — Gaussian-splat + mesh from one image; great for **previz**.
- **Habit:** always **quad-remesh** a raw AI mesh to a sane polycount before it touches UE. Hero assets still get a Blender cleanup pass.

### Skies & environments
- **Blockade Labs Skybox AI** **[Cloud]** — 360° skyboxes; does ~90% of the Orbits realm by itself.
- **World Labs "Marble"** **[Cloud]** — explicit-3D scene generation, useful for previz/blockout inspiration (not final geometry).

### Video (trailers / pitch reels — later)
- **Veo 3.1** (Google) **[Cloud]** — best cinematic + native synchronized audio (48 kHz), 4K. *Hero trailer.* (Note: mandatory SynthID watermark on output.)
- **Kling 3.0** **[Cloud]** — 4K/60fps, 15-sec clips, storyboard multi-shot, motion-control camera paths, lip-sync.
- **Runway Gen-4.5** **[Cloud]** — best manual control (motion brush, camera, character consistency); also ships the **GWM-1** world model.
- **Open / self-host [A], Apache-2.0 (free, commercial-clean):** **Wan 2.7**, **HunyuanVideo 1.5**, **LTX-2.3** — iterate trailers on the farm at no cost.
- ⚠️ **Avoid Sora 2 for new pipelines** — OpenAI is discontinuing it (app retired Apr 26 2026; API ends Sept 24 2026).

### Audio (you're already strong here)
- **ElevenLabs Eleven v3** **[Cloud]** — top TTS/VO and SFX; **Scribe v2** for transcription.
- **Stable Audio 2.5** **[Cloud]** — commercially-safe music/sfx sketches.
- The adaptive **harmonic** score (each realm a mode; a correspondence resolves a chord) is best authored in your own pipeline — this is a differentiator, keep it hand-crafted.

---

## 4. The world-model frontier — watch, don't build on (yet)

This is the most important "no fake / no dead-end" call, because it's the most tempting trap and it lands right on your theme.

- **Genie 3 / Project Genie** (Google DeepMind) — real-time interactive worlds from a prompt: **11B model, 720p, 24 fps, only minutes of coherence, 60-sec exploration limit, Google AI Ultra (US, 18+) only.** It is an **experimental prototype, explicitly not a game engine or production tool.**
- **Runway GWM-1** (Dec 2025), **World Labs Marble** (explicit 3D) — same story: research/creative frontier.
- **The honest verdict:** there is **no tool today that turns a prompt into a persistent, optimized, shippable open-world game.** Even Epic's own leadership says engines won't be superseded by promptable world models. 
- **What this means for you:** ✅ Your UE5 **assemble-and-build** method is the correct, non-dead-end path. ✅ Your concept (ever-generating, never-fully-predictable worlds) is *exactly* where the frontier is heading — great timing and a great pitch. ❌ Do **not** try to build MANIFOLD on Genie/world-models now. Revisit in the UE6 era (2027+) as a possible generative layer, and prototype with it for fun/marketing only.

---

## 5. Making it deeply engaging — the durable way (not the predatory way)

You said "addictive." The right target is **durable, meaningful engagement** — the kind chess, Dwarf Fortress, and The Witness have — **not** manipulative compulsion. The lasting kind is also the non-predatory kind, and it's what your design already aims at. Build for these, and explicitly **avoid** dark patterns (variable-ratio loot pulls, FOMO timers, manufactured grind, pay-to-skip-pain).

**The levers that create the good kind:**
- **The aha cadence** — tune so a discoverable correspondence is *always* within reach, but the *next* one is never certain. That two-axis band (always something to find; never sure what) is the whole craft. Your **Insight Rate** metric is the literal dial.
- **Flow** — match challenge to the player's growing skill; the difficulty comes from the subtlety/number of correspondences, not twitch. Accessibility layers keep the door open without dulling mastery.
- **Mastery that compounds** — solving a realm grants *eyes*, not just points; the player feels themselves getting smarter. That's the most durable hook there is.
- **The forever-engine** — the community canon means there's always new, human-made structure to discover. This is your retention engine — earned, not manipulated.
- **Earned surprise** — emergent seam-collisions give stories the player tells others. Word of mouth (see Dwarf Fortress's "infinite tail") is the healthiest growth loop.

> Measure engagement by *depth and return-for-the-right-reasons* (Insight Rate, sessions that end satisfied), not by time-on-device. A game people respect outlasts a game that traps them.

---

## 6. The integrated pipeline (how it all flows)

```
            ┌─ Claude Code + Opus 4.8 (+ local MiniMax M3 on Rig A) ─┐
SYSTEMS:    │  kernels · deterministic core · correspondence · etc.  │  → CI gate → merge
            └────────────────────────────────────────────────────────┘   (runs in parallel)

WORLD:  idea → FLUX.2 concept [A] → UE5.8 greybox [B] → assemble:
        Blockade sky + Megascans/Fab terrain (PCG) + Hunyuan3D props [A] + Rodin hero [Cloud]
        → MAGIC LAYER (Niagara resonance + biolum + MegaLights + Lumen + post) [B]
        → looks like the concept ✔

AUDIO:  realm mode + chord-resolve (your pipeline) + ElevenLabs SFX → events from telemetry

PROVE:  playtest → Insight Rate vs no-correspondence control → vertical-slice GATE → go/no-go

SHOW:   Movie Render Graph path-trace [A] + Veo 3.1 [Cloud] → trailer
```

---

## 7. Producibility boosters & fallbacks (the "never stall" list)

- **One realm, fully beautiful, before any others.** Orbits first (sky-heavy = cheapest AAA). One stunning realm beats eight grey ones.
- **Greybox proves fun before art spend.** Art is the expensive layer; never build it on an unproven loop.
- **Assemble, don't model.** Megascans/Fab + PCG. Fallback if an AI-3D hero asset is weak → buy a Fab marketplace asset; if photoreal-organic (forest) is too hard solo → **stylize** that realm.
- **Mechanical verification, always.** Nothing merges until its test is green in CI. This is what lets agents run in parallel without rot.
- **Route models by task; self-host bulk on Rig A.** Keeps quality high and API bills near zero.
- **Budget performance at Phase 4, not Phase 1.** A "really close" slice can run looser; optimize when you target launch/console.
- **Licensing hygiene:** open-weights to lean on commercially — **FLUX.2** (open), **Hunyuan3D**, **TRELLIS 2** (MIT), video **Wan 2.7 / HunyuanVideo 1.5 / LTX-2.3** (Apache-2.0). Always confirm a tool's current commercial terms before shipping an asset; **Veo** outputs carry a SynthID watermark.
- **Hand to a specialist to *finish*, not to *start*.** Finishing a proven slice is far cheaper and lower-risk.

---

## 8. Cost & licensing snapshot (rough, June 2026)

| Item | Cost | Note |
|---|---|---|
| Claude Code + Opus 4.8 | $5/$25 per 1M tok (caching cuts input ~90%) | your main spend; Sonnet 4.6 for volume |
| Local models (FLUX.2, Hunyuan3D, Wan/LTX, MiniMax M3) | **$0** | run on Rig A; this is most of your generation |
| Rodin / Meshy / Tripo (3D) | free-gen to ~$10–120/mo | hero assets & rigged characters only |
| Veo 3.1 / Kling (video) | ~$0.03–0.50/sec or sub | trailers only; iterate free on self-hosted Wan/LTX |
| UE 5.8 | free under $1M revenue | royalties after |
| Megascans/Fab | free for UE | your asset firehose |
| Lore / Perforce | free | version control |

Your farm flips most "AI tool" costs to **electricity** — a real structural advantage. Reserve paid cloud for hero frames, hero 3D, and final trailers.

---

## 9. Watch list (what will change the plan)

- **UE6** (EA ~late 2027): Verse, MCP-native AI in-engine, portable "smart assets." Plan to migrate, not to wait.
- **Claude Fable 5 / Mythos 5** returning to availability → instant coding-ceiling upgrade; swap in when live.
- **World models maturing** (Genie 4-class, World Labs) → possible generative layer for MANIFOLD post-UE6.
- **Open-weights closing the gap** → even cheaper local bulk on your farm.

---

## Sources & verification (checked 29 June 2026)
Model landscape: Artificial Analysis / LMArena / SWE-bench Pro (Scale) coverage, June 2026 — Opus 4.8 (May 28), Fable 5 GA June 9 (suspended), GPT-5.5 (Apr 23), Gemini 3.1 Pro (Feb 19), open-weights pack. Engine: Epic "State of Unreal 2026," 17 June 2026 — UE 5.8 release, MegaLights/Mesh Terrain/PCG, Lore VCS, UE6 EA late 2027, MCP integration. 3D: Rodin Gen-2, Meshy 6, Tripo v3, Hunyuan3D, TRELLIS 2 (June 2026 round-ups). Image/Video: FLUX.2, Nano Banana Pro, Seedream; Veo 3.1, Kling 3.0, Runway Gen-4.5, Wan 2.7/HunyuanVideo 1.5/LTX-2.3 (Apache-2.0); Sora 2 discontinuation (Apr/Sept 2026). World models: DeepMind Project Genie (Jan 29 2026, AI Ultra, 720p/24fps/60s), Genie 3, Runway GWM-1, World Labs Marble.

*All financials/specs are planning references for greenlight + build discussion and should be re-checked at purchase — this space moves monthly. ⟐ = your decision in the Build Plan.*
