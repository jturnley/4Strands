# Phase 0 spike

The spike answers two questions in game before any real porting: **can 4Strands read body vertex data at runtime**, and **can it draw a strand mesh whose vertices it moves every frame**. The code is `src/Spike*.cpp`, `src/BodyProbe.cpp` and `src/SkinLayout.h`, switched by `[Spike]` in `4Strands.ini`. It builds, but none of it has run in game yet.

Several engine layouts it relies on aren't in CommonLibF4, so the spike checks them and logs what it finds instead of trusting them. Every read through a guessed layout goes through `Skin::SafeCopy`, which returns false instead of crashing on a bad pointer.

## What it does

- **Frame hook** (`FrameHook.cpp`): replaces `PlayerCharacter::Update` (vtable slot 0xCF) and calls the spike after the original. Phase 1 keeps this as the frame driver.
- **Body probe** (`BodyProbe.cpp`): runs on the player once their 3D is loaded after a save loads or a new game starts, and on every actor when a 4Stim scene starts. For each skinned geometry it logs the type, vertex count and vertex layout. For the largest one it reads:
  - the `BSSkin::Instance` and `BoneData`, using F4SE's layout (`SkinLayout.h`). The bone count and the skin-to-bone transform count must match, or the layout is wrong.
  - the CPU copy of the vertices (`BSGraphics::VertexBuffer::data`). This is the main question: LooksMenu's BodyGen relies on it, but it may be dropped after upload.
  - six vertices, skinned to world space with the bones' current transforms. Each should land a few units from its main bone.
- **Strand** (`SpikeStrand.cpp`): a 24-segment, 4-sided tube hung between `LArm_Hand` and `RArm_Hand`, with a sag that bobs over time.
  - It is made with the engine's `CreateDynamicTriShape`. The first argument (`StrandDynamicFlags`) is unknown and defaults to 0.
  - It takes copies of the alpha and shader properties from a vanilla mesh (`StrandTemplate`, default `BldGlassGlow01.nif`, a white glow).
  - It finds BSDynamicTriShape's vertex pointer by scanning the fields after BSTriShape for one that points at the positions it was created with. It writes nothing if no field matches.
  - It is attached to the player's 3D root and written every frame in root space. Writes don't take the shape's lock yet.

## How to test

1. Build: `xmake build` from PowerShell. From Git Bash, w64devkit's GCC on PATH breaks the spdlog package build, and Defender flags CMake's GCC test programs.
2. Install the DLL and `4Strands.ini` (`xmake install` with `XSE_FO4_MODS_PATH` set) on FO4 1.11.240 with F4SE and Address Library.
3. Load a save with a TWB female or A-Body male player, in third person. Optionally start a 4Stim scene.
4. Read `Documents/My Games/Fallout4/F4SE/4Strands.log`.

## What to look for

| Log line | Good | Bad, and what it means |
|---|---|---|
| `frame hook installed` | present | missing or a crash at load: wrong vtable slot |
| `skin: N bones ... M skin-to-bone transforms` | N = M, both under a few hundred | "layout doesn't check out": `SkinLayout.h` offsets are wrong for this build |
| `renderer data: ... CPU copy` | a non-null pointer | "NO CPU COPY": skin sampling has to read the NIF from disk instead (Phase 1 work) |
| `vertex i: ... N units away` | under about 30 units, world positions near the actor | huge distances: the skinning math or the transform convention is wrong |
| `strand: created BSDynamicTriShape` | present | another type: try other `StrandDynamicFlags` |
| `strand: dynamic vertex data at +X, stride S` | present | "no field points at the vertex positions": the fields line has the raw values to work from |
| `strand: property 1 ... BSEffectShaderProperty` | present | template missing: set `StrandTemplate` to another unskinned effect mesh |
| In game | a thin glowing strand between the hands that sags and bobs | invisible: check the bound, the shader flags, or the template's alpha. A crash on draw: the shader doesn't fit the vertex layout |

Report the log lines back so the findings can go into BODIES.md and PLAN.md.
