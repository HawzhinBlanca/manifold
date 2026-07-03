# MANIFOLD — How to Play

MANIFOLD is a game where the gameplay **is** cross-domain analogy. Several simulated
*realms* run at once, each with its own physics. Hidden inside them is a single shared
structure — one small-integer ratio — expressed differently in each realm. Your job is
to **find the shared ratio, tell the true realms from the decoy, and carry power across
the seam** where two realms correspond.

## The loop

1. **Observe the realms.** Each realm displays the ratio it currently exhibits.
2. **Find the correspondence.** Six realms secretly share the same hidden ratio; a
   **decoy** realm shows a different one. When the physics of two realms line up
   on the same ratio, a correspondence *ignites* at the seam.
3. **Transport.** With a correspondence lit, press **`E`** to carry power across the
   seam (a vortex becomes a world; a resonance becomes a flow).
4. **Compound.** Every discovery and transport raises your **Insight Rate** and score.
5. **Win** by reaching the objective's discovery target; **restart** anytime with `R`.

## Controls

| Key | Action |
|-----|--------|
| `E` | Transport the currently-lit correspondence across the seam (Classic) |
| `R` | Restart the session (a new hidden ratio if the seed changes) |
| `C` | Toggle between **Classic** and **Constellation Lock** modes |
| `1`–`6` | (Constellation) Pick / unpick a realm for your subset |
| `Space` | (Constellation) Lock your selected subset |
| `V` | (Constellation) Pay points to reveal the next realm's membership |
| `X` | Start a Constellation **Expedition** (a campaign of escalating puzzles) |
| `H` | Toggle the controls / help overlay |
| free-fly | Move the camera to observe the realms |

The console commands mirror the keys: `ManifoldTransport` (`E`), `ManifoldRestart`
(`R`), `ManifoldToggleMode` (`C`), `ConstellationToggleRealm <index>` (`1`–`6`),
`ConstellationLock` (`Space`).

## Constellation Lock (the harder mode)

Press **`C`** to switch to Constellation Lock. Now the puzzle inverts: instead of six
realms sharing one obvious ratio and a single decoy, **every realm shows a *different*
surface ratio**, and a hidden subset of them (the *constellation*) truly corresponds —
but under a **rule** that isn't literal equality:

- **Exact** — the corresponding realms share the same ratio outright.
- **Octave** — they match only after dividing out factors of 2, so `3:1`, `3:2` and
  `6:1` all count as the *same* structure even though the numbers look nothing alike.

The active rule is chosen from the seed (and shown on the HUD as a hint). Your job is to
**mentally normalize the six ratios by that rule, decide which realms belong to the
corresponding set, and lock exactly that subset** with `1`–`6` then `Space`. A correct
lock ignites every pairing in the constellation and wins; a wrong guess is a *wasted
probe* that costs points. This is inference and set-selection under a hidden rule — real
cross-domain reasoning, not spotting the odd number out.

**Difficulty & rank.** Octave puzzles (harder to infer than Exact) and *flawless* solves
(no wasted probe) score higher, so the **S → D** rank reflects skill. Press `C` a second
time for **Expert**: the rule itself is hidden (`Rule: ???`), so you must infer whether
it's Exact or Octave from the ratios alone — the full challenge — worth a bonus on a win.
Press `C` once more to return to Classic. Each session is deterministic in its seed and
records as a shareable, reproducible replay. Constellation and Classic keep **separate
best scores** (the two aren't comparable).

**Probe economy (stuck? buy a hint).** Press `V` to pay points and reveal whether the next
realm is `[IN]` the constellation or `[out]` of it. Certainty has a price — spend it
wisely, because reveals (like wrong locks) come straight off your score.

**Constellation Expedition.** Press `X` to play a campaign of subset-hunt puzzles back to
back — difficulty escalates (the rule goes hidden from the third level on), each solve
advances you to the next level, and your score accumulates across the run. Your best
campaign total persists in your profile.

## The realms

| Realm | Domain | How it shows the ratio |
|-------|--------|------------------------|
| **Orbits** | celestial | two planets in a mean-motion resonance (e.g. 3:2) |
| **Fluids** | flow | a vortex you can carry across the seam |
| **Harmonics** | acoustic | two oscillator modes whose frequencies form the ratio |
| **Waves** | spatial | two string harmonics forming the ratio |
| **Rhythm** | time | a polyrhythm (three-against-two = 3:2) |
| **Gears** | mechanism | two meshed gears whose tooth counts form the ratio (exact) |
| **Circuits** | electromagnetism | two resonant LC tanks whose frequencies form the ratio |
| **Decoy** | — | a **red herring**: a ratio that deliberately does *not* match |

Six of these (Orbits, Harmonics, Waves, Rhythm, Gears, Circuits) always share the
session's hidden ratio; the Decoy never does. The correspondence engine refuses to pair the decoy with
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
than the last (2, 4, 6, …). A single session can surface at most **16** discoveries
(one seam correspondence + the C(6,2)=15 cross-domain analogies among the six true
realms), so the run has a natural difficulty wall — it ends the first time you can't
clear a level.
Your expedition score is the sum of the levels you cleared.

## Running it

- **Play (editor):** open the project and press Play — `ManifoldGameMode` is the
  default, so the branded HUD, the realms (as real geometry), and the audio all come up.
- **Standalone build:** run `Tools/CI/package.ps1`, then launch `dist/Windows/MANIFOLD.exe`.
- **Verify (headless):** `Tools/CI/run_tests.ps1` builds and runs the full automation
  suite.
