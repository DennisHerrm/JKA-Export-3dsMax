# JKA Export for 3ds Max

Export a skinned character or creature from **3ds Max 2016–2027** straight into a **Star Wars
Jedi Knight: Jedi Academy** Ghoul2 model – GLM, GLA, `animation.cfg`, `animevents.cfg`, skin
file, textures and an NPC file – ready to spawn in the game.

> Unofficial fan project – see the [disclaimer](#disclaimer).

## What it writes

```
<base>\models\players\<name>\<name>.gla          skeleton + all animation frames
<base>\models\players\<name>\model.glm           meshes
<base>\models\players\<name>\animation.cfg       sequences (name, first frame, count, loop, fps)
<base>\models\players\<name>\animevents.cfg      footsteps (AEV_FOOTSTEP)
<base>\models\players\<name>\model_default.skin  surface → texture
<base>\models\players\<name>\*.tga               textures from the materials
<base>\ext_data\npcs\<name>.npc                  optional; in game: npc spawn <name>
```

`<base>` is a `base` folder of the game (or your asset folder that you pack into a `.pk3`).

## How it works

- **Skeleton**: every node of the chosen hierarchy that is not a skinned mesh (bones, helpers,
  rigid meshes) plus all bones used by Skin modifiers; on top a `model_root`, like Raven's models.
- **Meshes**: the mesh below the Skin modifier, bind pose and weights from the Skin modifier
  (up to 4 weights). Rigid meshes are bound 100 % to their own bone. Surfaces over 1000 vertices or
  32 bones are split automatically (Ghoul2 limits).
- **Animations**: from a note track on the scene root named `jka` (otherwise `animations`) with a
  key at the start and at the end of each sequence, carrying its name (`BOTH_WALK1` …). Without a
  note track the whole time range becomes `BOTH_STAND1`. Footsteps come from a note track
  `events` (keys containing "footstep").
- **Game space** (measured on Raven's own creatures): models face −Y, every frame is lowered by
  24 units (the NPC bounding box starts 24 units below the origin), triangles are written
  clockwise. Option **In place** keeps the body on the spot in every frame (the game moves the NPC,
  not the animation).

The quantized GLA/GLM writers come from **g2c**, a re-implementation of Raven's Carcass, verified
against the original game files. Exported files were checked by decoding them the way the engine
does and comparing with the Max scene: bones within 0.06, vertices within 0.05 units.

Works together with [KOTOR Import](https://github.com/DennisHerrm/KOTOR-Import-3dsMax): import a
KOTOR creature (*Jedi Academy ready* on), *KOTOR Animations → Select JKA set → Selected to
timeline*, then *JKA Tool → Export GLM/GLA*.

## Requirements

- Windows 10/11 64-bit, 3ds Max 2016–2027
- Microsoft Visual C++ runtime 14.50 or newer (the Setup installs it)

## Installation

1. Download `JkaExport-<version>-Setup.exe` from the [releases](../../releases) and run it
   (close 3ds Max first). **Or** extract the ZIP and run `Install.bat`.
2. Start 3ds Max: menu **JKA Tool → Export GLM/GLA**, or *File → Export* → *Jedi Academy Ghoul2 (*.glm)*.

Not code-signed yet – Windows SmartScreen may warn; check the download with `SHA256SUMS.txt`.

## Usage

1. Select any node of the character, open *JKA Tool → Export GLM/GLA*.
2. Model name (folder name, lower case), base folder, scale (1.0 = Max units are game units;
   big models may need 0.5 – the format allows bone offsets up to ±511 units, the export tells
   you when it had to clamp), *Faces +Y: turn 180* if your model looks along +Y, NPC class.
3. *Export*. The report lists bones, frames, sequences, split surfaces and warnings; the log is in
   `Downloads\Export jka.log`.

MAXScript: `JkaExportCpp.exportModel "<node>" "<name>" "<base folder>" 1.0 -1 "CLASS_HOWLER"`
(turn: −1 = automatic, 0, 1; empty class = no NPC file), `JkaExportCpp.lastReport()`.

Not yet: tags (`*r_hand`, `*driver` …), several LODs, shaders for alpha textures.

## Building from source

Visual Studio 2022/2026 with C++ and CMake, plus the 3ds Max SDKs. `BUILD.bat` builds every
installed SDK year and installs the package. Details: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Disclaimer

Unofficial, free fan project. It is **not** made by, affiliated with, endorsed or sponsored by
Raven Software, Activision, Lucasfilm Ltd., The Walt Disney Company or Autodesk. Star Wars is a
trademark of Lucasfilm Ltd. / The Walt Disney Company; Jedi Academy belongs to its respective
owners; 3ds Max is a trademark of Autodesk. No game data is included.

## License

GPL-3.0 with an additional permission for linking with the Autodesk 3ds Max SDK – see
[LICENSE](LICENSE) and [LICENSE-EXCEPTION.md](LICENSE-EXCEPTION.md). The Ghoul2 file layout
follows Raven's `mdx_format.h` as released with the Jedi Academy source code (GPLv2).
