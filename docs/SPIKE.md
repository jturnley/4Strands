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

## Findings

### Run 1 (2026-10-31, vanilla player in a Vault 111 suit)

- **CPU vertex copy: yes.** `VertexBuffer::data` holds the vertices, and `dataSize` is exactly vertices × stride (10,270 × 32 for the hands). Body sampling can read vertices at runtime without going to the NIF on disk.
- **BSSkin layout checks out:** 38 bones, 38 world transforms and 38 skin-to-bone transforms on the hands.
- **Skinned vertex layout:** desc `0005B00050430208`, 32-byte stride, half-precision positions, flags vertex | UV | normal | tangent | skinned, skinning data at byte 20.
- **Not yet right:** the sampled vertices landed 70+ units from their bones, and most of their weights didn't decode. Most bones logged no name. The probe ran on the first frame after the load, when the 3D root was still near (0, 0, 0), so the transforms were probably stale. The spike now waits 2 seconds after the 3D appears, and logs raw skinning bytes and per-bone details to settle it.
- **Crash in the strand:** the call through CommonLibF4's `CreateDynamicTriShape` reached the short overload (MSVC orders adjacent overloaded virtuals in reverse), which read the vertex count as a pointer. The strand now calls vtable slot 8 directly. The other overload pairs in `IRendererResourceManager` (`CreateTriShape`, `CreateDynamicLineShape`, `CreateMeshLODTriShape`) probably have the same problem in CommonLibF4.

### Run 2 (same save, no crash)

- **Weights decode correctly as half floats** (for example 0.819 + 0.169 + 0.012), with byte bone indices after them.
- **Most entries in `BSSkin::Instance::bones` are null.** Only `RArm_Hand` had a node on the hands mesh. `worldTransforms` has a valid pointer for every bone: the node's own `world` where a node exists, and separate storage otherwise. Skinning has to use `worldTransforms`, and the probe now does.
- **The 3D root was near the origin** at (13.7, −16.0, 1.9) while the player stood at (−89244, 90900, 8955). Run 3 contradicts this (the root matched the actor exactly), so it was a transient state, not a coordinate offset. Still unexplained.
- **The strand is a BSDynamicTriShape**, desc `0002B00004030205`: 20-byte stride, flags vertex | UV | normal | colors. It has heap pointers at +180 and +190, but neither starts with the creation positions, so the strand stayed off. The spike now dumps both and the vertex buffer's CPU copy, and `StrandDynamicOffset` / `StrandDynamicStride` can force a field for testing.

### Run 3 (A-Body male player, nude, no crash)

- The player has 15 skinned A-Body pieces (`BaseMaleBody:0Full` with 18,360 vertices, plus `Virtual*` collision shapes such as `VirtualPenis` and `VirtualAnus`) and 13 BSDynamicTriShape head parts (head, hair, eyes). The head parts are full precision, with different layouts (stride 24-32).
- The body's skin has 55 bones, including `Penis_04_SMP`. Every vertex's weights sum to 1.00. The 3D root matched the actor's position.
- **Skinning convention still unknown:** with `world = boneWorld × skinToBone × v`, vertices landed 25-107 units from their main bone. The local positions run from about z = 0 at the chest to z = −60 at the groin, so mesh space seems to sit high on the body. The spike now logs three conventions per vertex (as stored, rotation transposed, inverted) to find the one that keeps vertices next to their bones.
- **Strand:** the creation positions only reach the static vertex buffer, as half floats. `+180` points at unrelated data, and `+190` points at zeroed memory. The spike now uses the first field whose zeroed run is at least 100 × 16 bytes, as float4 positions, and logs that guess. If that's not the per-frame buffer, the strand stays invisible or the game crashes on draw.

### Run 4 (same player, no crash): body reading answered

- **Skinning works with the rotation transposed:** `world = boneWorld × (Rᵀ·v + t)`, with R and t from `BoneData`'s skin-to-bone transform. Under that convention every sampled vertex is 1.3-14.5 units from its main bone in bone space. The groin vertex lands 2.2 units from `Penis_04_SMP` in world space. Blended vertices land 15-34 units from their single main bone, which fits body surface spread over several bones. The as-stored and inverted conventions are 15-112 units off. So FO4's `BoneData` rotation is the transpose of what CommonLibF4's `NiTransform` math expects. OStrands' Body.cpp skin sampling ports with that one change.
- **Phase 0 question 1 is answered: yes.** Body vertices, weights, bones and their live world transforms can all be read at runtime from the CPU copy, with no NIF reading.
- **Strand:** `+190` holds only 40 zero bytes, so the size check refused it and nothing was written. With flags 0, `CreateDynamicTriShape` doesn't seem to allocate a per-frame buffer. Next the spike reads the engine's own BSDynamicTriShapes (head, hair, eyes) to find the field that holds their vertices.

### Run 5 (player plus a female NPC in a 4Stim scene)

- **Dynamic positions are float3, 12 bytes per vertex.** On the engine's own BSDynamicTriShapes, `+170` (u32) is exactly vertices × 12: 676 × 12 = 0x1FB0 on `Beard01`, 4645 × 12 = 0xD9BC on `HairFemale01`, 421 × 12 = 0x13BC on `HairLong01_hairline`. `+198` holds the same size rounded up to 16. `+180`, `+188`, `+190` and `+1A0` hold pointers, and `+1A8` holds a second size whose meaning is unclear.
- **The spike's strand has 0 at `+170` and no `+188` pointer**, so `CreateDynamicTriShape` with flags 0 never set up per-frame positions. The next run reads each pointer field on the engine's shapes as float3s to find the one that holds them.
- The matching against the vertex buffer's first position was useless: the beard's first vertex is (0, 0, 0), which matched zeroed memory everywhere and wrote 2.5 MB of log. Dynamic shapes' vertex buffers may not hold positions at all (the hair's first position read as NaN).
- **Crash at 12:28, probably 4Stim's:** an `EXCEPTION_BREAKPOINT` in Havok's `hknpThreadSafetyCheck::acquireReadAccess`, from `bhkNPCollisionObject::SetMotionType` called by 4Stim.dll on a job thread (`Main::UpdateNonRenderSafeAITasks`). No 4Strands code is on the stack. The probe had run 2 seconds earlier and only reads. Running a 4Stim scene with 4Strands disabled would confirm it.
