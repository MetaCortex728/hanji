# SPDX-License-Identifier: Apache-2.0
"""Creates the plugin's assets: the sprite quad mesh and the lit and unlit sprite materials.

Run through the sample project (see unreal/hanji-unreal.ps1 -Mode Assets). The assets are committed, so
this only needs to run again when it changes.

Material parameters, used by UHanjiSpriteComponent:
  Atlas    texture   the clip table's atlas
  FrameUV  vector    (u, v, width, height) of the cell in the atlas, 0..1
  Mirror   scalar    1 draws the cell flipped left to right
Both materials are masked (alpha 0.5), two-sided and flagged "Has Pixel Animation", which tells TSR that
pixels change without motion vectors, so animated frames do not ghost.
"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
LOG = []


def log(message):
    LOG.append(str(message))
    unreal.log("[hanji] " + str(message))


def fresh(path):
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)


def make_quad():
    path = "/Hanji/Meshes/SM_HanjiQuad"
    fresh(path)
    mesh = unreal.HanjiAssetLibrary.create_quad_mesh_asset(path)
    if mesh is None:
        raise RuntimeError("could not build the quad mesh")
    EAL.save_asset(path, only_if_is_dirty=False)
    log("saved " + path)


def node(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def make_material(name, unlit):
    folder = "/Hanji/Materials"
    path = folder + "/" + name
    fresh(path)
    m = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT if unlit else unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )
    m.set_editor_property("has_pixel_animation", True)

    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    u = node(m, unreal.MaterialExpressionComponentMask, -1200, -60)
    for channel, on in (("r", True), ("g", False), ("b", False), ("a", False)):
        u.set_editor_property(channel, on)
    v = node(m, unreal.MaterialExpressionComponentMask, -1200, 60)
    for channel, on in (("r", False), ("g", True), ("b", False), ("a", False)):
        v.set_editor_property(channel, on)
    MEL.connect_material_expressions(uv, "", u, "")
    MEL.connect_material_expressions(uv, "", v, "")

    flipped = node(m, unreal.MaterialExpressionOneMinus, -1000, -120)
    MEL.connect_material_expressions(u, "", flipped, "")
    mirror = node(m, unreal.MaterialExpressionScalarParameter, -1000, -220)
    mirror.set_editor_property("parameter_name", "Mirror")
    mirror.set_editor_property("default_value", 0.0)
    pick = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -60)
    MEL.connect_material_expressions(u, "", pick, "A")
    MEL.connect_material_expressions(flipped, "", pick, "B")
    MEL.connect_material_expressions(mirror, "", pick, "Alpha")
    cell_uv = node(m, unreal.MaterialExpressionAppendVector, -600, 0)
    MEL.connect_material_expressions(pick, "", cell_uv, "A")
    MEL.connect_material_expressions(v, "", cell_uv, "B")

    frame = node(m, unreal.MaterialExpressionVectorParameter, -800, 260)
    frame.set_editor_property("parameter_name", "FrameUV")
    frame.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    offset = node(m, unreal.MaterialExpressionComponentMask, -600, 200)
    for channel, on in (("r", True), ("g", True), ("b", False), ("a", False)):
        offset.set_editor_property(channel, on)
    size = node(m, unreal.MaterialExpressionComponentMask, -600, 320)
    for channel, on in (("r", False), ("g", False), ("b", True), ("a", True)):
        size.set_editor_property(channel, on)
    MEL.connect_material_expressions(frame, "", offset, "")
    MEL.connect_material_expressions(frame, "", size, "")
    scaled = node(m, unreal.MaterialExpressionMultiply, -400, 60)
    MEL.connect_material_expressions(cell_uv, "", scaled, "A")
    MEL.connect_material_expressions(size, "", scaled, "B")
    atlas_uv = node(m, unreal.MaterialExpressionAdd, -250, 120)
    MEL.connect_material_expressions(scaled, "", atlas_uv, "A")
    MEL.connect_material_expressions(offset, "", atlas_uv, "B")

    atlas = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -50, 0)
    atlas.set_editor_property("parameter_name", "Atlas")
    atlas.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    MEL.connect_material_expressions(atlas_uv, "", atlas, "UVs")

    colour = unreal.MaterialProperty.MP_EMISSIVE_COLOR if unlit else unreal.MaterialProperty.MP_BASE_COLOR
    MEL.connect_material_property(atlas, "RGB", colour)
    MEL.connect_material_property(atlas, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(m)
    EAL.save_asset(path, only_if_is_dirty=False)
    log("saved " + path + (" (unlit)" if unlit else " (lit)") + ", has pixel animation: " + str(m.get_editor_property("has_pixel_animation")))


make_quad()
make_material("M_HanjiSprite", unlit=False)
make_material("M_HanjiSpriteUnlit", unlit=True)
log("done")
