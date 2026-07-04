# MANIFOLD — headless material authoring library
#
# Author, compile, and save Unreal materials with NO editor GUI. This is the pipeline
# this project uses to build the game's look on a headless box (the editor GUI renders
# blank here; offscreen game rendering + this Python both work headless).
#
# Run:
#   & "<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<proj>/MANIFOLD.uproject" \
#       -ExecutePythonScript="Tools/Art/manifold_materials.py" -unattended -nullrhi \
#       -nosplash -nopause -stdout
#
# Iterate a material by editing this file and re-running — changing a *material asset*
# needs NO C++ rebuild, just re-run this + re-render (-ManifoldAutoShot). C++ only needs
# rebuilding if you change which material the visualizer *loads*.
#
# Gotcha learned the hard way: blind emissive/fog/nebula values OVERSHOOT badly (fog
# flooded the scene blue, the nebula flooded it purple). Author dim, then verify by
# render and dial up — never trust the first value.

import unreal

_tools = unreal.AssetToolsHelpers.get_asset_tools()
_mel = unreal.MaterialEditingLibrary
PKG = "/Game/Materials"


# ---- tiny node helpers ------------------------------------------------------
def new_material(name):
    p = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        unreal.EditorAssetLibrary.delete_asset(p)
    return _tools.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew())

def vparam(mat, name, default, x=-900, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n

def c3(mat, lc, x=-900, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    n.set_editor_property("constant", lc)
    return n

def const(mat, v, x=-900, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", v)
    return n

def fresnel(mat, exp, base, x=-900, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, x, y)
    n.set_editor_property("exponent", exp)
    n.set_editor_property("base_reflect_fraction", base)
    return n

def mul(mat, a, b, x=-400, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    _mel.connect_material_expressions(a, "", n, "A")
    _mel.connect_material_expressions(b, "", n, "B")
    return n

def add(mat, a, b, x=-400, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionAdd, x, y)
    _mel.connect_material_expressions(a, "", n, "A")
    _mel.connect_material_expressions(b, "", n, "B")
    return n

def lerp(mat, a, b, alpha, x=-300, y=0):
    n = _mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    _mel.connect_material_expressions(a, "", n, "A")
    _mel.connect_material_expressions(b, "", n, "B")
    _mel.connect_material_expressions(alpha, "", n, "Alpha")
    return n

def finish(mat):
    _mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())


# ---- the game's material set ------------------------------------------------
def make_realm_orb(core=1.0, rim=2.4, fres_exp=3.0):
    """Unlit glowing energy sphere. Fresnel rim => a sphere reads 3D, not a flat disc.
    Driven by a 'Color' vector param the visualizer's ApplyGlow() already sets.
    Tuned against the render: the orbs read as FLAT matte discs at the old 0.7/1.4 because the
    body sat below the bloom threshold (only the star, whose whole body is bright, bloomed). Core
    raised 0.7->1.0 so the orb body itself blooms into a soft coloured halo, rim 1.4->2.4 (kept
    clearly hotter than the core) so the fresnel edge still reads as a 3D energy shell rather than
    a uniform blown-out disc. The central star stays brightest via its own M_Star + a 1.9x glow."""
    m = new_material("M_RealmOrb")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = vparam(m, "Color", unreal.LinearColor(1, 1, 1, 1), -900, -50)
    fr = fresnel(m, fres_exp, 0.04, -900, 240)
    scale = add(m, mul(m, fr, const(m, rim, -650, 320), -500, 260), const(m, core, -650, 440), -300, 300)
    _mel.connect_material_property(mul(m, col, scale, -120, 40), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m

def make_nebula(brightness=0.5, scale=0.00016):
    """Two-sided unlit procedural nebula for a giant inside-out backdrop shell.
    KEEP IT DIM — this overshoots into a solid purple wall very easily."""
    m = new_material("M_Nebula")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    noise = _mel.create_material_expression(m, unreal.MaterialExpressionNoise, -700, 0)
    noise.set_editor_property("scale", scale)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.15)
    noise.set_editor_property("output_max", 1.0)
    noise.set_editor_property("turbulence", True)
    deep = c3(m, unreal.LinearColor(0.004, 0.006, 0.018), -500, -160)
    mid = c3(m, unreal.LinearColor(0.020, 0.010, 0.045), -500, -20)
    hot = c3(m, unreal.LinearColor(0.045, 0.016, 0.060), -500, 140)
    neb = lerp(m, lerp(m, deep, mid, noise, -260, -80), hot, mul(m, noise, noise, -400, 200), -120, 0)
    _mel.connect_material_property(mul(m, neb, const(m, brightness, -260, 260), 60, 60), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m

def make_star(core=1.1, rim=2.4):
    """Unlit hot star — brighter core + rim than a realm orb."""
    m = new_material("M_Star")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = vparam(m, "Color", unreal.LinearColor(1, 0.85, 0.45, 1), -900, -50)
    fr = fresnel(m, 2.2, 0.15, -900, 240)
    scale = add(m, mul(m, fr, const(m, rim, -650, 320), -500, 260), const(m, core, -650, 440), -300, 300)
    _mel.connect_material_property(mul(m, col, scale, -120, 40), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m

def make_metal(roughness=0.30, glow=0.20):
    """Lit metallic PBR for the gear cogs (brass/steel). 'Color' = base colour."""
    m = new_material("M_Metal")
    col = vparam(m, "Color", unreal.LinearColor(0.85, 0.7, 0.25, 1), -900, -50)
    _mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    _mel.connect_material_property(const(m, 1.0, -500, 150), "", unreal.MaterialProperty.MP_METALLIC)
    _mel.connect_material_property(const(m, roughness, -500, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    _mel.connect_material_property(mul(m, col, const(m, glow, -500, 360), -250, 60), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def import_texture(name, rel_src, pkg="/Game/Sky", srgb=True):
    """Import a source image (repo-relative to the .uproject dir) as a Texture2D asset.
    Returns the loaded asset (or None if the source is missing)."""
    src = unreal.Paths.combine([unreal.Paths.project_dir(), rel_src])
    if not unreal.Paths.file_exists(src):
        unreal.log_warning("MANIFOLD: sky source not found: " + src)
        return None
    dest = pkg + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        unreal.EditorAssetLibrary.delete_asset(dest)
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = pkg
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    _tools.import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(dest)
    if tex:
        tex.set_editor_property("srgb", srgb)
        # Equirectangular: longitude wraps (U), latitude clamps (V) so the poles don't bleed.
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        unreal.EditorAssetLibrary.save_asset(dest)
    return tex

def make_skydome(brightness=0.85):
    """Real deep-space backdrop: an unlit, two-sided sphere material that emits a public-domain
    NASA Milky Way equirectangular panorama (mostly black sky + the galactic band), sampled on the
    giant inside-out backdrop shell. Dimmed slightly so the void stays dark and the realms/seam pop.
    Falls back to the procedural M_Nebula if the texture import failed (source image missing)."""
    tex = import_texture("T_StarMap", "Tools/Art/assets/starmap_milkyway_2k.jpg")
    if tex is None:
        unreal.log_warning("MANIFOLD: skipping M_SkyDome (no star texture); M_Nebula remains the backdrop")
        return None
    m = new_material("M_SkyDome")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    samp = _mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0)
    samp.set_editor_property("texture", tex)  # uses the mesh UV0 (the engine UV-sphere is equirect)
    _mel.connect_material_property(mul(m, samp, const(m, brightness, -500, 250), -180, 0),
                                   "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def build_all():
    made = [make_realm_orb(), make_nebula(), make_star(), make_metal()]
    sky = make_skydome()
    if sky:
        made.append(sky)
    unreal.log("MANIFOLD MATERIALS BUILT: " + ", ".join(x.get_name() for x in made))


# Executed when run via -ExecutePythonScript.
build_all()
