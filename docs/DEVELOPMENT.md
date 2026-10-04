# JKA Export – development notes

## Layout

| Part | Files | Max SDK |
|---|---|---|
| Writers | `vendor/g2c` – g2c's `writeMdxa` (GLA) and `writeMdxm` (GLM), C++20 | no |
| Export layer | `src/g2export.h/.cpp` – splitting surfaces, files, cfg, npc | no |
| Window | `src/jkaexport_fenster.cpp` (plain Win32) | no |
| Max layer | `src/jkaexport_max.cpp`, `src/jkaexport_dll.cpp` | yes |

## Formula (from g2c, verified against Raven's GLA)

The engine evaluates W(b) = W(parent) · A(b); the skinning matrix is W = X · B⁻¹. Stored per
frame and bone: root A = X · B⁻¹, otherwise A = B(p) · X(p)⁻¹ · X(b) · B(b)⁻¹, with X = world pose
in the frame and B = bind pose.

## Limits

* Bone offsets in A are stored as 16-bit values with ±511 units range. A contains a rotation
  about the **model origin**, so big models with far-out bones can exceed it (KOTOR rancor at
  scale 1, 330 units tall: one finger 527). The export names bone and sequence; a smaller scale
  fixes it.
* Max 1000 vertices and 32 bones per surface (split automatically), 4 weights per vertex,
  256 bones (rend2).

## Game space (measured on Raven's rancor, wampa, howler)

* Creatures face **−Y**, left is +X.
* In **every frame** `model_root` sits at z = **−24** (NPC bounding box `DEFAULT_MINS_2`); the bind
  pose (GLM) stays on the ground.
* **Winding**: Ghoul2 (Quake 3) treats clockwise triangles as front faces – 99.6 % of Raven's
  creature triangles point against their vertex normals. Triangles are written a, c, b.
* `.npc`: `height` = total height (maxs = height − 24), `width` = half width.

## Bind poses

Bind transforms come from the Skin modifier (`ISkin::GetBoneInitTM`, `GetSkinInitTM`). A GLA has
one bind pose per bone; if a bone is bound differently in two skins, the vertices of the other
skin are re-posed into the GLA bind pose (linear blend with that skin's weights).

## In place

KOTOR attacks push the body almost a body length forward, deaths end displaced. In Jedi Academy
the code moves the NPC, so clips should stay on the spot. Motion bone = `rootdummy`, otherwise
the moving bone with the most descendants (at least a quarter of the skeleton). Its horizontal
position is pinned to the bind pose in every frame and the whole skeleton is shifted with it;
height and limbs are untouched. Result on 22 KOTOR creatures: ≤ 0.03 units drift.

## Sequences

Note track `jka` (otherwise `animations`) on the scene root: one key at the start and one at the
end of each sequence, both with the name. Frame 0 of the GLA = `ROOT` (bind pose). Loops (STAND,
WALK, RUN … or "loop" in the name) drop their last frame, which equals the first.
