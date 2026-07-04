# MANIFOLD — Art & VFX Direction

> **Design pillar:** _the correspondence engine is the product; visuals are a layer._
> The current build ships a **cinematic cosmic** look, and most of the dressing plan below (§2–§5) is
> now **shipped and render-verified**: a real public-domain NASA Milky Way **HDRI sky** (replacing the
> procedural nebula), glowing fresnel energy-orbs, a dramatic transport-**seam** arc with code-driven
> energy VFX (discovery bursts + seam sparks), **lit brass-PBR gear cogs**, scrolling-energy **wave
> ribbons**, a **density-coloured Fluids vortex**, a **palette-chip HUD** with tiered rank, and a 1080p
> showcase. What remains is the higher-fidelity tier that needs a **display + Fab/Megascans assets +
> Niagara + a human eye** — see the turnkey guide [ART_AAA_HANDOFF.md](ART_AAA_HANDOFF.md).
> Visual verification is **not** blocked here: offscreen GPU rendering works on this
> workstation (the `-ManifoldAutoShot` / `-ManifoldAutoShotSequence` flags capture real frames — see
> the memory note on the RTX 3090 Ti), so the remaining art work is gated on *art direction and
> assets*, not on a display. Nothing here changes gameplay; it is all presentation.
>
> **Status legend below:** ✅ shipped (render-verified) · 🎨 remaining (display + assets + eye).

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

- **Orbits** — ✅ a hot star (`M_Star`) + planets on the orbit-path rings, with dotted orbit trails.
  🎨 remaining: a textured planet (albedo/normal/night-lights) + a Niagara star corona.
- **Fluids** — ✅ density-coloured vortex (dense cells run hot white-cyan, sparse stay palette cyan).
  🎨 remaining: a true translucent/refractive **volume** (Niagara Fluids or a panning-noise volume).
- **Harmonics** — 🎨 concentric standing-wave crystal shells (geometry; Megascans/subsurface) — needs
  a display + assets. Currently the palette bead-pair sized to the ratio.
- **Waves** — ✅ the sine-ribbon mesh now scrolls a bright emissive band along its length (`M_Wave`
  panner over UVs); the hump count already encodes the harmonic, so the *ratio reads in the geometry*.
- **Rhythm** — 🎨 a ring of pulse-emitters firing on the tempo (Niagara burst per beat) — needs the
  editor. Currently the palette bead-pair.
- **Gears** — ✅ the extruded cog now uses lit **brass PBR** (`M_Metal`, metallic 1, roughness 0.35)
  with computed normals. 🎨 remaining: bevels/wear + Fab gear meshes for full photoreal.
- **Circuits** — 🎨 an LC loop + electric arc (Niagara ribbon with jitter) — needs the editor.
  Currently the palette bead-pair.

**Shared motif:** ✅ member realms brighten on discovery via the `PulseFactor` flash, and code-driven
energy VFX (discovery bursts + seam sparks) mark the moment. (Always-on per-realm ambient motes were
tried and reverted — invisible when subtle, clutter when boosted; the event-driven VFX read better.)

## 3. The seam (the money shot)

The correspondence "seam" is the game's signature. When two realms share the hidden ratio:
1. A **golden arc** (`ManifoldPalette::Seam`) links the two realm worlds — a Niagara beam with a
   travelling pulse from source to destination.
2. On transport, a bright bloom-pop at the destination, and the destination realm briefly re-tunes
   toward the shared ratio (drive from the existing transport event).
3. Audio already cues this (discovery chime / transport swell) — sync the VFX peak to the cue.

## 4. Scene, lighting, post

- **Backdrop:** ✅ a real public-domain NASA Milky Way **HDRI sky** (`M_SkyDome` + `T_StarMap`) on the
  inside-out shell, replacing the procedural nebula (kept as a fallback). Dark realistic space; the
  saturated realm colours pop hard against it.
- **Lighting:** ✅ warm-key / cool-fill directional rig + a per-realm palette-coloured point light,
  all feeding a volumetric fog.
- **Post:** ✅ bloom + gentle vignette + fixed exposure + a cool colour grade + DoF (already tuned).
  🎨 remaining: a colour LUT / mild transport-only chromatic aberration if desired.

## 5. UMG / HUD (bespoke pass) — ✅ SHIPPED (C++ canvas)

Done as a C++ HUD-canvas reskin (no UMG-editor needed):
- ✅ A ratio **chip** per realm in its palette colour, in both the Classic readout and the
  Constellation tray, colour-matching the realm's scene orb.
- ✅ Constellation tray: six selectable chips with a bright **selected-outline** state.
- ✅ Tier-coloured **rank reveal** (S gold → D grey) over the procedural emblem + title card.
- ✅ Every string the code surfaces is kept — reskin, not rewire.
- 🎨 remaining (optional): a true UMG pass with animated widgets / fonts (needs the editor).

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
- ✅ Real NASA Milky Way **HDRI sky** (`M_SkyDome`/`T_StarMap`); glowing fresnel energy-orbs.
- ✅ **Lit brass-PBR gear cogs** (`M_Metal` + computed normals); **scrolling-energy wave ribbons**
  (`M_Wave` panner + UVs); **density-coloured Fluids vortex**.
- ✅ Dramatic transport-**seam** arc + code-driven **energy VFX** (discovery bursts + seam sparks).
- ✅ Discovery flash (`PulseFactor`), transport hooks; key/fill lighting + per-realm point lights +
  volumetric fog; bloom/vignette/fixed-exposure/DoF/colour-grade post.
- ✅ **Palette-chip HUD** (both readouts) + tiered rank reveal + procedural emblem + title card;
  1080p showcase media synced to the build.
- ✅ **Headless visual verification** — offscreen GPU render (`-ManifoldAutoShot` /
  `-ManifoldAutoShotSequence`) captures real frames, and even an animated loop, so any dressing pass
  can be checked by eye without an interactive display.

**The remaining §2–§5 work is the higher-fidelity tier** (per-realm photoreal *geometry* — crystal
shells, LC loops, a volumetric fluid; Niagara VFX; Fab/Megascans assets), which genuinely needs a
**display + assets + a human eye** (see [ART_AAA_HANDOFF.md](ART_AAA_HANDOFF.md)). The scriptable
material/colour/HUD slices of §2–§5 are shipped and render-verified.
