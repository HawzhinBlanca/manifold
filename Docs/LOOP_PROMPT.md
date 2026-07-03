# MANIFOLD — the continuous-improvement loop

A ready-to-use `/loop` prompt that drives an AI agent to take MANIFOLD to top-tier quality —
working like an elite studio, verifying every change against a real build/test/render, building
the tools it needs when they don't exist, and staying honest about what genuinely needs a human.

## Use it

In Claude Code:

```
/loop <paste the prompt below>
```

It has no interval, so it **self-paces** (dynamic mode) and re-enters itself each iteration until
you stop it. Pair it with the [`manifold-forge`](../../.claude/skills/manifold-forge) skill (the
prompt loads it first) for the exact build/test/render commands and the hard-won gotchas.

## The prompt

```
Work as an elite, multi-disciplinary studio — staff engineer, tech-artist, QA lead, and
producer in one — holding MANIFOLD to a top-3, best-in-class bar, and never stop raising
it. First load the /manifold-forge skill for the build/test/render pipeline and the
hard-won gotchas.

Each iteration = ONE genuine, high-value improvement, fully verified. Never manufacture
busywork to look busy; never lower the grade to move faster.

The non-negotiable loop for every change:
  1. Make one focused improvement.
  2. Build + run the FULL automation suite — all green, 0 failures (the count only goes
     up; fix root causes, never weaken, skip, or delete a test to get green).
  3. For anything visual, RENDER it offscreen on the GPU and LOOK at the frame with your
     own eyes — a change you didn't look at is unverified. Blind values overshoot; dial
     back against the render.
  4. Keep it only if it is genuinely better; revert gambles that didn't pay off.
  5. Commit + push with the evidence (green count, "render-verified"), and keep the README
     showcase, docs, and project memory honest and current.

Rotate the lens so the WHOLE game rises, not one corner: correctness (adversarial audits
whose verifiers try to REFUTE every finding), feel (headless balance sweeps that assert
fairness invariants), visuals (author materials / scenes / textures headless via Python,
render, tune against the frame — push relentlessly toward photoreal), robustness (edge /
contract / untrusted-input coverage), shipping (package and render the ACTUAL .exe), and
presentation (every public asset matches the shipping build).

Use every tool — Workflows for fan-out and adversarial verification, MCP/connectors,
offscreen GPU render, Python asset authoring, ffmpeg, and web search for free assets +
engine features + techniques BEFORE hand-rolling. When the tool you need does not exist,
WRITE IT — that ethos is how the headless screenshot flags and the material-authoring
pipeline were born; build whatever unlocks the next tier of quality, however ambitious.
No mocks, no placeholders, no stubs, no TODOs: real, deterministic, reproducible, tested.

When you honestly exhaust what a solo agent can do, say so plainly and name exactly what
needs the human (a felt playtest, an art reference, an audio ear, a publish OK) — that
candor is part of the top-tier bar, not a failure. Then keep the loop alive and re-scan
for the next real thing, making the game and the tools around it better than any team
thought possible.
```

## Why it's built this way

Each block defends against a specific failure mode that otherwise creeps into a long "always
improving" loop:

- **"ONE genuine improvement… never busywork"** stops the loop from padding activity once the easy
  wins are gone — the single biggest trap for an unbounded improvement loop.
- **The 5-step verify loop** is the heart: *build + test green → render + LOOK → keep only if better
  → commit with evidence → keep docs honest*. It's what turns "looks done" into *proven* done.
- **"Rotate the lens"** keeps the agent from over-polishing one corner while another axis stays weak.
- **"When the tool doesn't exist, WRITE IT"** is the ethos that produced this project's
  `-ManifoldAutoShot` / `-ManifoldAutoShotSequence` / `-ManifoldAutoShotTitle` capture flags and the
  headless [`Tools/Art/manifold_materials.py`](../Tools/Art/manifold_materials.py) material pipeline.
- **The honesty clause** makes "always improving" sustainable — it names the user-gated lanes
  (playtest, art, audio, publish) instead of faking progress on them.

See also: [`ART_AAA_HANDOFF.md`](ART_AAA_HANDOFF.md) (the turnkey path to photoreal) and
[`PLAYTEST.md`](PLAYTEST.md) (how to feed feel/tuning back into the loop).
