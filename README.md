# 4Strands

Procedural fluid strands, climax fluids and residue for Fallout 4, built as an F4SE plugin on top of the OStim Standalone port. 4Strands replaces both OCum (climax tracking and effects) and OStrands (procedural strands and fluids) with a single plugin that renders everything procedurally instead of using static overlays.

## Status

Scaffold only. What is here:

- `src/fluid/`: the particle fluid solver from OStrands. It has no engine dependencies and builds on its own as the `FourStrandsFluid` library.
- `src/GeometryMath.h`, `src/AtomicFile.h`: helpers from OStrands.
- `src/main.cpp`, `src/FourStimBridge.cpp`: the F4SE plugin entry and the 4Stim connection. It gets 4Stim's thread interface and logs scene start, scene change, speed, climax and stop.
- `extern/4stim/PluginInterface.h`: 4Stim's C++ API header, copied from KeryoWolfe/4Stim.
- The Phase 0 spike: a frame hook, a body vertex probe and one test strand. See [docs/SPIKE.md](docs/SPIKE.md). It builds but hasn't run in game.

Nothing game-facing is ported yet. See [docs/PLAN.md](docs/PLAN.md).

## Building

4Strands builds the same way as 4Stim: [xmake](https://xmake.io) 3.0+ and Visual Studio's C++ tools, with [libxse/commonlibf4](https://github.com/libxse/commonlibf4) as a submodule pinned to the same commit as 4Stim's.

```
git clone --recursive https://github.com/jturnley/4Strands
cd 4Strands
xmake f --toolchain=msvc
xmake build -r
```

Build from PowerShell or a Developer prompt. If a MinGW GCC (for example w64devkit) is on PATH, xmake's package builds pick it up and fail.

Set `XSE_FO4_MODS_PATH` (MO2's mods folder) or `XSE_FO4_GAME_PATH` and `xmake install` copies the plugin there. For now the target is Fallout 4 1.11.240 with the Address Library. 4Stim's Runtime Database support for other game versions lives in 4Stim's own `src/compat`, not in CommonLibF4.

## Credits and license

4Strands is licensed under the GNU GPL v3 (see LICENSE). It is derived from [OStrands](https://gitlab.com/LilRicefield/ostrands) by Saint, also GPLv3, and reimplements features of OCum for Fallout 4.
