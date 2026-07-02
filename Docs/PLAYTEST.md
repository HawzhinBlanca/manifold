# MANIFOLD — Playtest Checklist

The logic is fully headless-tested (82/82). What a machine *can't* judge is **feel**. This
is the one thing that needs you at the controls. Play a few sessions, then jot answers to
the questions below — each maps to a concrete knob I can turn from your feedback.

## Quantitative baseline (headless, `MANIFOLD.Balance.Sweep`)

Before your feel pass, a 128-seed sweep measures the *objective* half of balance (the numbers,
not the feel). It runs in CI and asserts fairness/discrimination invariants; run it standalone
to see the live distribution (`Automation RunTests MANIFOLD.Balance.Sweep`). Current baseline:

| Metric | Result | Read |
|---|---|---|
| Classic discoveries / seed | min 16, max 17, **all 128 reach full realization** | fair across seeds |
| Classic decoy collisions | **0 / 128** | the red herring never false-matches |
| Constellation solvable | **128 / 128** | every generated puzzle is winnable |
| Constellation relation split | **Exact 64 / Octave 64** | rule inference is a true coin, not skewed |

**One imbalance found + fixed by this sweep:** the Classic score was being **swamped by the
insight term**. `GetInsightRate()` is discovery-events per *sim-second*, whose denominator is
arbitrary across realm mixes, so it reached ~1060 and `rate*100` added ~106k — dwarfing the
16k of discoveries and collapsing **every** Classic run to rank S. Fixed by clamping insight to
a modest, discovery-subordinate bonus (≤250). Classic score is now discovery-driven (~16.5k at
the ceiling) and the rank curve differentiates: D (0–2 discoveries) · C (3–4) · B (5–6) · A (7–8)
· S (9–16). Locked by a regression invariant so it can't silently regress.

**Two threshold calls the data can't make — they need your feel (see Q6, Q11):**
- A ceiling Classic run (16 discoveries) is S, and S already starts at ~9 of 16. Should S demand
  closer to full realization, or is "strong = S" right? → knob: `RankForScore` (9000/7000/5000/3000).
- A *flawless* Exact K=3 constellation solve lands at rank **C** (4500); Octave at **B** (6500).
  Perfect play of the easy relation earning only a C may feel punishing → knob: the flawless/Octave
  bonuses in `GetScore`, or the rank thresholds.

## Launch

- **Editor:** open `MANIFOLD.uproject`, press Play. `ManifoldGameMode` is the default.
- **Standalone:** run `Tools/CI/package.ps1`, then `dist/Windows/MANIFOLD.exe`.

Controls: `[E]` transport · `[R]` restart / new puzzle · `[C]` cycle mode
(Classic → Constellation → Constellation-Expert → Classic) · `1`–`6` pick a realm ·
`[Space]` lock · `[V]` pay to reveal a realm · `[X]` start a Constellation Expedition.

## What to evaluate (and the knob behind it)

### Classic
1. Does the seam / transport read clearly — do you understand *why* a correspondence lit?
2. Is spotting the shared ratio among the realms too easy / too hard?
   → knob: `UManifoldSlice::PickSharedRatio` ratio pool; the decoy is intentionally obvious.

### Constellation Lock (the core of this pass)
3. **Octave inference** — when the rule is Octave (3:1 ≡ 3:2 ≡ 6:1), does normalizing the six
   ratios in your head feel *doable but demanding*, or opaque/frustrating?
   → knobs: the octave families in `ManifoldSlice.cpp` (`kFamilyMembers`), and whether the
     HUD hint should show the rule name (it does, except in Expert).
4. **Constellation size** — is K=3 (pick 3 of 6) the right amount of search?
   → knob: `SetupConstellation(..., ConstellationSize)` and `PickConstellation`.
5. **Probe / reveal economy** — do the costs feel fair? A wrong lock is −250; a paid reveal
   (`[V]`) is −500; a flawless solve is +1500; Octave is +2000; Expert +2500.
   → knob: `UManifoldSlice::GetScore` constants.
6. **Rank thresholds** — do S/A/B/C/D land where they should for good/bad play?
   → knob: `UManifoldSlice::RankForScore` (9000/7000/5000/3000).

### Expert & Expedition
7. Expert (rule hidden) — inferring Exact-vs-Octave from the ratios: fun challenge or a coin
   flip? → knob: the two relations `PickRelation` chooses between.
8. Expedition (`[X]`) — does the 5-level ramp (Expert from level 3) build well? Too long/short?
   → knobs: `AManifoldGameMode::ExpeditionLevels` / `ExpeditionBaseSeed`, and the per-level
     difficulty schedule in `RunConstellationExpedition` / `StartSession`.

### Feel (all modes)
9. Audio — does the octave-inference "click" land with the discovery chime / victory fanfare /
   failure buzz? Is the ambient pad pleasant?
10. Readability — is the HUD legible; do you always know what to press next?
11. **Rank payoff** — after a run you're proud of, does the grade you earn feel *deserved*? In
    particular: a flawless Exact (easy-relation) constellation solve currently earns a C — too
    harsh? And does a near-perfect Classic run reliably reach S, or does S come too easily?
    → knobs: `GetScore` flawless/Octave bonuses and `RankForScore` thresholds (the baseline
      above shows exactly where these currently land).

## Report template

Paste this back filled in and I'll turn it into concrete tuning commits:

```
Mode(s) played:
Sessions:
Too easy / too hard (which mode, why):
Octave inference felt (1=opaque .. 5=great):
Probe/reveal costs felt (too cheap / right / too dear):
Rank I earned vs. how I felt I played:
Confusing moments:
Audio/feel notes:
The single thing that would most improve it:
```

## What's next after your feedback

Everything below the mechanics is real, working code. The remaining lane is
**art direction** (Megascans/Nanite scenes, Niagara VFX, bespoke UMG) — that needs your
asset pipeline and a display. Point me at assets or a target look and I'll wire them against
the verified logic. See `Docs/IMPLEMENTATION_STATUS.md` §5.
