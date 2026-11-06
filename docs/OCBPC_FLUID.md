# fo4-ocbpc's fluid, compared with ours

Notes from reading [ReidenXerx/fo4-ocbpc](https://github.com/ReidenXerx/fo4-ocbpc) (2026-11-05):
`CBPSSE-RD/FluidSim.h/.cpp` (the solver, 1,060 lines) and `CBPSSE-RD/Fluid.h/.cpp` (the game side,
1,890 lines).

## Licence and lineage

- **It's OStrands too.** Both files say they follow OStrands (LilRicefield, GPL-3):
  - `FluidSim` is OStrands' position-based fluid solver (Macklin and Müller, 2013), "its constants and its choices"
    kept.
  - `Fluid` is OStrands' FluidManager (emitters, reservoirs, domains, drawing).
- **Licence:** GPL-3, with an additional permission for linking with F4SE (fo4-ocbpc's README, section 7). 4Strands
  is GPL-3 too, so its code can be used with credit to ReidenXerx as well as LilRicefield.
- **Portability:** the solver is game-free. It needs only `NiPoint3`, works in metres in a domain frame of its own
  (z up, the floor the plane z = 0), and has offline tests (`tests/fluid`). The game side is written for FO4
  1.10.163 and F4SE 0.6.23, so it would need reworking for our CommonLibF4 / 1.11.240 build.

## The solver: the same core, tuned differently

Both have the same structure:
- **Particles:** position, velocity, life and a `link` to the next particle of their run, so threads stretch and snap.
- **Anchors:** a run's newest particle held to its source.
- **Bodies:** capsules with end velocities.
- **Holes:** places capsules don't push in.
- **Neighbours:** a hash grid.
- **Fluid:** cohesion, viscosity and a density constraint.
- **Contact:** sticky contacts with grip and friction, and particles carried by the surface they touch.

| parameter | 4Strands (ours) | fo4-ocbpc | their note |
|---|---|---|---|
| iterations | 4 | 4 | |
| cohesion | 4 | 15 | |
| viscosity | 40 | 50 | |
| stretch | 3 | 5 | how fast a stretched link's rest length follows it, per second |
| friction | 30 | 60 | "OStrands 30; 60 holds a landed shot" (their tests) |
| grip | 0.6 | 0.5 | "OStrands 0.4"; holds while the push along the skin is under grip (m/s²) |

So our values aren't OStrands' everywhere either (grip 0.6 against OStrands' 0.4). Worth checking ours against
OStrands' source before tuning.

### What they added that we don't have

- **Canals** (`struct Canal { a, b, r, va, vb }`, `Solver::Contain`, `CanalCarrier`, `Particle::canal`):
  - **Containment:** "the inside of an opening: fluid in it stays in, but for the open entrance". Each canal is a
    capsule from an open entrance (`a`) to a closed deep end (`b`). A particle inside it is held within its radius and
    short of its end.
  - **Entry:** a particle crossing the entrance can join it (`a_join`).
  - **Drag:** `canalDrag` (3000 per second) is flow lost along the walls, so what's left inside seeps out slowly.
  - **This is what the uterus fill needs.** SoftBody FO4 already exports its canal through `SoftBodyFO4_GetCanal`:
    the ring centres and radii, the cervix, the os, and the uterus's centre, axis, length and width. A chain of canal
    capsules can be built from that every frame:
    - entrance to each ring, then on to the cervix;
    - then a narrow one through the os;
    - then a closed one for the uterus.
- **His shaft as a canal pusher** (`Capsule::shaft`): the shaft's capsules push fluid inside a canal, and a body's
  capsules don't.
- **Slime strings** (`strings` 24 at once, `slime` 3 per second, `kSkinString`): a particle leaving a body it lay on
  stays tied to that body by a string that gives way slowly.
- **Film drag** (`film` 20): friction × (1 + film × thin²). A thin film (a lone particle) clings, while a glob runs.
  Their note: "sticky liquid, not a gel".
- **Skin cohesion** (`skinCohesion` 0.2): cohesion on a body is a fifth of what it is in the air, so a running glob
  leaves its thin edge behind.
- **Drawing helpers:**
  - `Density`: the particles as a smooth field, with a bulk test.
  - `Wrap`: a blob's skin along a set of directions.
  - `Drape`: a smear on a body as one sheet. An n × n grid over the capsule most of the smear touches, lifted to the
    fluid's height and sunk under the skin at its edges.
  - `VolumeRadius`: the sphere that holds a number of particles' volume.

## The game side (`Fluid.cpp`)

- **Climax inside her** (vagina 0, anus 1, mouth 2):
  - **Reservoir:** most of each pulse goes into a reservoir for that opening. `overflow` (0.5) of each pulse backs out
    round his shaft at once, as particles that her canal holds.
  - **Capacity:** the reservoir fills to its capacity, and more spills while he's in.
- **Pulling out** (OStrands' Leak):
  - **Gush:** `kGushShare` 0.35 of the reservoir comes out at `kGushRate` 40.
  - **Seep:** the rest seeps out over about a minute. While he's in it seeps at `kSeep` 0.03; after he's out it leaks
    at `kLeak` 0.12.
  - **Pose:** the leak is faster when she stands or moves, and slower when the opening faces up.
- **Mouth:** it swallows the fluid or lets it run out. `MouthState` reports to faces: 0 nothing, 1 full, 2 running out,
  3 swallowing.
- **Canals per opening:** rebuilt every frame, since they move with her. Depths in game units are vagina 6.5, anus
  8.5, mouth 4.0.
  - **Plugged:** as wide as his shaft plus `kCanalAround` (2.5) spacings.
  - **Out:** it closes to `kCanalShut` (vagina 0.65, anus 0.55, mouth 0.7) over `kCanalClose` (1.2 s), squeezing out
    what no longer fits.
  - **Lost for a frame:** an opening not found that frame keeps its last canal, since particles hold its index.
- **Holes:** an opening has `kHoleRadius` 1.5 and his tip `kNozzleHole` 1.0 (units). An opening sits inside the lower
  belly capsule, so a hole there keeps that capsule from pushing fluid out of the opening.
- **Drawing (as OStrands):**
  - **Blobs:** crowded particles are drawn as blobs: a sphere mesh with a bone per vertex (`Blob00`..`47.nif`) wrapped
    round them.
  - **Ropes:** linked runs are drawn as ropes, a strand tube (`Rope00`..`31.nif`).
  - **Drops:** lone particles are drawn as drops.
  - **Fallback:** without the meshes it falls back to its own static shots and drips.

## What this means for 4Strands

1. **The core matches.** Porting OStrands' solver (`src/fluid`) remains the right base. fo4-ocbpc confirms the
   method works in FO4.
2. **Add canal containment to our `Solver`.** This is the piece the SoftBody uterus needs. SoftBody FO4 supplies the
   canal geometry, so there are no guessed depths here, unlike fo4-ocbpc's `kCanalDepth`. On a climax inside her:
   - **Emit:** emit at his tip inside the canal.
   - **Os:** call `SoftBodyFO4_SetOsOpen` while particles pass the os.
   - **Fill:** count the particles in the uterus capsule and report it as `SoftBodyFO4_SetUterusFill`.
   - **After he pulls out:** let the gush and the seep take them back out of the vagina, where they become ordinary
     skin and floor fluid.
3. **A visible fill needs particles inside, not just a reservoir.** fo4-ocbpc's reservoir is a number past its
   canal's capacity, and the camera inside her wouldn't show it. Either:
   - keep all of it as particles (costlier; cap the count); or
   - keep a reservoir and draw the uterus's contents as one blob sized from it with `VolumeRadius`.
4. **Worth taking for skin residue:** film drag and skin cohesion (thin films stay, globs run), slime strings, and
   `Drape` for smears.
5. **Tune against both.** Compare our `Parameters` with OStrands' originals and fo4-ocbpc's tested values (friction
   60, cohesion 15) before the in-game pass.
