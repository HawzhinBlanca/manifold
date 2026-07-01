# MANIFOLD — How to Play

MANIFOLD is a game where the gameplay **is** cross-domain analogy. Several simulated
*realms* run at once, each with its own physics. Hidden inside them is a single shared
structure — one small-integer ratio — expressed differently in each realm. Your job is
to **find the shared ratio, tell the true realms from the decoy, and carry power across
the seam** where two realms correspond.

## The loop

1. **Observe the realms.** Each realm displays the ratio it currently exhibits.
2. **Find the correspondence.** Four realms secretly share the same hidden ratio; one
   realm (the **decoy**) shows a different one. When the physics of two realms line up
   on the same ratio, a correspondence *ignites* at the seam.
3. **Transport.** With a correspondence lit, press **`E`** to carry power across the
   seam (a vortex becomes a world; a resonance becomes a flow).
4. **Compound.** Every discovery and transport raises your **Insight Rate** and score.
5. **Win** by reaching the objective's discovery target; **restart** anytime with `R`.

## Controls

| Key | Action |
|-----|--------|
| `E` | Transport the currently-lit correspondence across the seam |
| `R` | Restart the session (a new hidden ratio if the seed changes) |
| free-fly | Move the camera to observe the realms |

The console command `ManifoldTransport` does the same as `E`; `ManifoldRestart` as `R`.

## The realms

| Realm | Domain | How it shows the ratio |
|-------|--------|------------------------|
| **Orbits** | celestial | two planets in a mean-motion resonance (e.g. 3:2) |
| **Fluids** | flow | a vortex you can carry across the seam |
| **Harmonics** | acoustic | two oscillator modes whose frequencies form the ratio |
| **Waves** | spatial | two string harmonics forming the ratio |
| **Rhythm** | time | a polyrhythm (three-against-two = 3:2) |
| **Gears** | mechanism | two meshed gears whose tooth counts form the ratio (exact) |
| **Decoy** | — | a **red herring**: a ratio that deliberately does *not* match |

Five of these (Orbits, Harmonics, Waves, Rhythm, Gears) always share the session's
hidden ratio; the Decoy never does. The correspondence engine refuses to pair the decoy with
the true realms — so you can't win by assuming everything matches. You have to
*discriminate*.

## Every session is different

The hidden ratio is chosen deterministically from the session **seed** (3:2, 4:3, 5:4,
5:3, 2:1, 5:2, 7:4, 7:5, 8:5, 9:8). Different seeds hide different ratios, so the game
can't be pre-solved; the same seed always reproduces the same session (great for
sharing a run or a replay).

## Score & profile

Your score rewards what you surfaced and how efficiently:

```
score = discoveries × 1000 + transports × 250 + round(InsightRate × 100) + speed bonus
```

The **speed bonus** applies when you win under a step budget. Each session is graded
**D → C → B → A → S** (at 3000 / 5000 / 7000 / 9000 points); a full clean run earns an
**A** or **S**. Your **best score** and sessions played/won persist between runs in a
`.manifoldprofile` file — beat your best.

## Expedition

Expedition mode strings sessions into a campaign: each level demands more discoveries
than the last (2, 4, 6, …). A single session can surface at most **11** discoveries
(one seam correspondence + ten cross-domain analogies among the five true realms), so
the run has a natural difficulty wall — it ends the first time you can't clear a level.
Your expedition score is the sum of the levels you cleared.

## Running it

- **Play (editor):** open the project and press Play — `ManifoldGameMode` is the
  default, so the branded HUD, the realms (as real geometry), and the audio all come up.
- **Standalone build:** run `Tools/CI/package.ps1`, then launch `dist/Windows/MANIFOLD.exe`.
- **Verify (headless):** `Tools/CI/run_tests.ps1` builds and runs the full automation
  suite.
