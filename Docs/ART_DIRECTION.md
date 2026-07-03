# MANIFOLD — Art & VFX Direction

> **Design pillar:** _the correspondence engine is the product; visuals are a layer._
> The current build ships a **cinematic cosmic** look — authored energy-sphere materials, a
> procedural nebula, volumetric lighting, and film-grade post (a real step past the original flat
> placeholders). This document is the plan to push it further toward full photoreal — executable by
> you, an artist, or me against the verified logic (turnkey guide: [ART_AAA_HANDOFF.md](ART_AAA_HANDOFF.md)).
> Visual verification is **not** blocked here: offscreen GPU rendering works on this
> workstation (the `-ManifoldAutoShot` / `-ManifoldAutoShotSequence` flags capture real frames — see
> the memory note on the RTX 3090 Ti), so the remaining art work is gated on *art direction and
> assets*, not on a display. Nothing here changes gameplay; it is all presentation.

## 0. North star

One image should communicate the whole game: **seven small worlds, each obeying its own law,
and a golden thread that lights up when two of them turn out to share the same hidden ratio.**
Legibility beats spectacle. A player must always be able to (a) tell the realms apart, (b) read
each realm's current ratio, and (c) see the seam ignite. Every art decision serves those three
reads.

## 1. Colour — DONE (code), palette is the contract

The realm palette is now **colorblind-safe** and centralised in
[`Source/Gameplay/Public/ManifoldPalette.h`](../Source/Gameplay/Public/ManifoldPalette.h), locked
by the automation test `MANIFOLD.Art.PaletteColorblindSafe` (simulates deuteranopia / protanopia /
tritanopia and asserts every realm stays distinguishable). It uses the **Okabe-Ito** palette:

| Realm | Okabe-Ito | Hex | Domain cue |
|---|---|---|---|
| Orbits | Blue | `#0072B2` | deep space |
| Fluids | Sky Blue | `#56B4E9` | water |
| Harmonics | Reddish Purple | `#CC79A7` | resonance |
| Waves | Bluish Green | `#009E73` | oscillation |
| Rhythm | Orange | `#E69F00` | pulse |
| Gears | Yellow | `#F0E442` | brass/mechanism |
| Circuits | Vermillion | `#D55E00` | current |
| _Decoy_ | desaturated grey | `#363638` | "not one of the family" |

**Rule for all future art:** a realm's identity colour comes from `ManifoldPalette`, never a
literal. Materials tint from it; VFX emit in it; the HUD label uses it. This keeps the accessibility
guarantee intact.

## 2. Per-realm visual language (Niagara + materials)

Each realm is a floating "world" ~1–2 m across. Replace the placeholder procedural meshes with a
signature look per realm; the mesh the game already builds (gear cog, wave ribbon, orbit rings) is
the anchor — dress it, don't replace the logic.

- **Orbits** — a small star (emissive, `ManifoldPalette::Star`) with 1–3 planets on the orbit-path
  rings the visualizer already draws. Niagara: sparse star-twinkle; a faint trail per planet.
- **Fluids** — a translucent volume with a vortex; use a panning noise material + a Niagara ribbon
  for the vortex core. Fresnel rim in Sky Blue.
- **Harmonics** — concentric standing-wave shells; emissive pulses on the beat of the detected ratio.
- **Waves** — the existing sine-ribbon mesh, upgraded with a scrolling emissive gradient whose
  wavelength = the harmonic number (so the *ratio is legible in the geometry*).
- **Rhythm** — a ring of pulse-emitters firing on the tempo; Niagara burst per beat.
- **Gears** — the existing extruded cog, brass PBR material (metallic 1, roughness ~0.35), meshing
  teeth; a second ghost-gear shows the ratio partner.
- **Circuits** — an LC loop; an electric arc (Niagara ribbon with jitter) whose frequency = the tank
  ratio.

**Shared motif:** when a realm is a *member* of the current correspondence, its rim/emissive gains a
subtle gold pulse (the visualizer already exposes a `PulseFactor` discovery flash — drive the
material emissive from it).

## 3. The seam (the money shot)

The correspondence "seam" is the game's signature. When two realms share the hidden ratio:
1. A **golden arc** (`ManifoldPalette::Seam`) links the two realm worlds — a Niagara beam with a
   travelling pulse from source to destination.
2. On transport, a bright bloom-pop at the destination, and the destination realm briefly re-tunes
   toward the shared ratio (drive from the existing transport event).
3. Audio already cues this (discovery chime / transport swell) — sync the VFX peak to the cue.

## 4. Scene, lighting, post

- **Backdrop:** the procedural starfield stays; consider a very dark, slightly blue nebula gradient
  (cheap sky material) so the saturated realm colours pop.
- **Lighting:** the warm-key / cool-fill directional pair is already in code. Add a subtle per-realm
  point light in the realm's palette colour for local grounding.
- **Post:** bloom (already on) tuned so only emissive gold + star highlights bloom; gentle vignette;
  fixed exposure (already set) so the scene never flickers. Optional: a mild chromatic aberration on
  transport only.

## 5. UMG / HUD (bespoke pass)

The HUD is functional (branded readout, mode card, help overlay). A bespoke pass:
- A ratio "chip" per realm in its palette colour, showing `p:q`, with a lock/member state.
- A constellation tray for Constellation Lock: six chips, selectable, with a clear "locked" state.
- Title card & rank reveal (S/A/B/C/D) with the emblem already generated procedurally.
- Keep every string the code already surfaces (`ManifoldHUD` / `DrawConstellationReadout`); this is
  reskin, not rewire.

## 6. Asset pipeline (what I need from you)

To execute §2–§5 against the verified logic I need, in priority order (note: a display is **not**
required — offscreen GPU rendering already lets me capture and verify frames headlessly via
`-ManifoldAutoShot`, so I can iterate on the look against the verified logic):
1. **Target look references** — even a few mood images or a colour/finish direction per realm.
2. **Any bespoke assets** you want used (Megascans, custom meshes, Niagara systems, fonts, SFX).
   Absent those, I can build entirely from procedural + engine-content primitives to a "stylised
   minimal" look that ships.

Give me any subset and I'll wire it; the logic, palette, hooks (`PulseFactor`, transport events,
per-realm colour), **and the headless offscreen-render path for verifying the result** are already
in place and tested.

## 7. What is already done in code

- ✅ Colorblind-safe centralised palette + regression test (§1).
- ✅ Procedural meshes per realm (gear cog, wave ribbon, orbit rings, spheres).
- ✅ Discovery flash (`PulseFactor`), seam render, transport bloom hooks.
- ✅ Starfield, key/fill lighting, bloom/vignette/fixed-exposure post.
- ✅ Branded HUD, mode cards, help overlay, procedural emblem, title card.
- ✅ **Headless visual verification** — offscreen GPU render (`-ManifoldAutoShot` /
  `-ManifoldAutoShotSequence`) captures real frames, and even an animated loop, so any dressing pass
  can be checked by eye without an interactive display.

The remaining work in §2–§5 is **dressing**, gated on your art direction (the render path for
verifying it is already in place).
