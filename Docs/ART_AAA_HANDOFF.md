# MANIFOLD — Photoreal / AAA Art Handoff

A turnkey guide to take MANIFOLD from its current **stylized cosmic** look to **literal
high-fidelity worlds**. The engineering is done and tested; this is the art lane, and the
final asset+eye work needs a human in the editor. Everything below is specific to this
project, not generic advice.

## Where the visuals are now (shipped)

A polished **stylized cosmic** look, authored entirely headless (see `Tools/Art/`):

- **`Content/Materials/M_RealmOrb`** — unlit energy-sphere material with a Fresnel rim, so
  each realm reads as a glowing 3D sphere. Driven by the `Color` param
  `AManifoldRealmVisualizer::ApplyGlow()` already sets.
- **`Content/Materials/M_Nebula`** — two-sided procedural-noise nebula on a giant inside-out
  backdrop shell (`Backdrop` in the visualizer). Turns the void into a soft violet nebula.
- **`M_Star`, `M_Metal`** — authored, ready to wire (hot star / brass gears).
- Cinematic scaffolding in `AManifoldRealmVisualizer::BeginPlay`: a `APostProcessVolume`
  (DOF, bloom, colour grade, grain, vignette, fixed exposure), a volumetric
  `AExponentialHeightFog`, a coloured point light per realm, and a key/rim directional rig.

Hero: `Docs/media/scene-loop.gif`.

## The one hard constraint

This workstation is **headless** (no attached display; RustDesk). The Unreal **editor GUI
renders blank here** — so material-graph painting, mesh sculpting, Bridge/Fab browsing, and
by-eye asset tuning must be done on a machine **with a real display**. What *is* scriptable
runs headless fine:

- **Authoring materials/scenes** → Unreal Python (`Tools/Art/manifold_materials.py`).
- **Verifying the result** → offscreen game render (`-RenderOffScreen -ManifoldAutoShot`),
  which works headless (that's how every screenshot in this repo was captured).

So: an artist does the sourcing + material/mesh authoring on a display box; the pipeline and
verification are already automated.

## Target: each realm as a literal world

Keep the *logic* — the visualizer spawns geometry and drives it from live kernel state
(ratios, positions). Dress or replace the mesh/material per realm; don't touch the ratio math.

| Realm | Now | Photoreal target | Free source |
|---|---|---|---|
| **Orbits** | gold sphere + star | textured planet (albedo/normal/roughness + emissive night-side city lights) orbiting a bright star with a corona | Fab planet packs; star = emissive sphere + Niagara corona |
| **Fluids** | cyan bead cloud | a real volumetric vortex/fluid | **Niagara Fluids** plugin; or a translucent refractive panning-noise volume |
| **Harmonics / Waves / Rhythm** | colored bead pairs / ribbons | glowing crystal resonators / lit standing-wave strings (subsurface + emissive) | Megascans crystals; custom `M_*` |
| **Gears** | procedural cogs (energy) | machined brass/steel cogs — metallic PBR, bevels, wear | Fab gear meshes + `M_Metal` (authored) + Megascans metal |
| **Circuits** | colored bead pair | LC coil / arc — emissive copper + electric arc | Fab; Niagara ribbon |
| **Seam** | gold bead arc | Niagara beam with a travelling energy pulse | Niagara |

## Step-by-step (on a machine with a display)

1. **Open** the project in the UE 5.8 editor.
2. **Get free assets** from **Fab** (fab.com — free Quixel Megascans + marketplace freebies)
   via the **Fab plugin** / Quixel Bridge. Grab a planet, metal surfaces, crystals; enable
   the **Niagara** and **Niagara Fluids** plugins.
3. **Import** to `/Game/Art/...` (drag-drop in the editor, or script it headless once the
   source files are on disk — extend `Tools/Art/manifold_materials.py` with
   `unreal.AssetImportTask`).
4. **Author materials** — extend `Tools/Art/manifold_materials.py` (the headless library) or
   author in the material editor: planet (PBR + emissive night lights), fluid (translucent +
   refraction), crystal (subsurface), metal (start from `M_Metal`).
5. **Wire into the visualizer** — in `Source/Gameplay/Private/ManifoldRealmVisualizer.cpp`,
   swap the per-realm meshes/materials at the `EmissiveMaterial` / `MakeCog` / `PlaceSphere`
   sites (and `SpawnRealmLight`). Keep the ratio-driven placement.
6. **Environment** — replace `M_Nebula` with an HDRI skybox or a Megascans nebula; add a
   `SkyAtmosphere`, volumetric clouds, and a `SkyLight` sourced from the HDRI.
7. **Lighting/post** — the code already spawns a tuned `PostProcessVolume` + fog; adjust focal
   distance, bloom, exposure, and add a colour LUT.
8. **Verify each step** with `dist/Windows/MANIFOLD.exe -RenderOffScreen -ManifoldAutoShot`
   (single frames) or `-ManifoldAutoShotSequence` (a GIF). Screenshots land in
   `.../Saved/Screenshots/`.

## What this session already built for you

- **Headless material authoring** — `Tools/Art/manifold_materials.py`: create/compile/save
  materials with zero GUI, iterate with **no C++ rebuild** (re-run the script + re-render).
- **The four base materials** in `Content/Materials/` — extend, don't restart.
- **The verify loop** — `-ManifoldAutoShot[Sequence]` renders real frames offscreen for QA.
- **The cinematic scaffolding** — post/fog/lights already in `BeginPlay`, tuned and ready.
- **A hard-won lesson, encoded in the tool's comments:** blind emissive/fog/nebula values
  *overshoot* (the fog once flooded the scene blue, the nebula purple). Author dim, then dial
  up against a render.

## Effort estimate

A skilled UE artist: ~1–2 weeks for a strong photoreal pass (sourcing + materials + scene +
tuning). All engineering hooks — ratio-driven geometry, per-realm colour, transport events,
`PulseFactor` discovery flash — are in place and tested, so the artist only touches
presentation.
