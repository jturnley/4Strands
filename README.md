# 4Strands

Procedural fluid strands, climax fluids and residue for Fallout 4, built as an F4SE plugin on top of the OStim Standalone port. 4Strands replaces both OCum (climax tracking and effects) and OStrands (procedural strands and fluids) with a single plugin that renders everything procedurally instead of using static overlays.

## Status

Scaffold only. What is here:

- `src/fluid/`: the particle fluid solver from OStrands. It has no engine dependencies and builds on its own as the `FourStrandsFluid` library.
- `src/GeometryMath.h`, `src/AtomicFile.h`: helpers from OStrands. GeometryMath needs CommonLibF4's `RE::NiPoint3`.
- `src/main.cpp`: an F4SE plugin entry stub.

Nothing game-facing is ported yet. See [docs/PLAN.md](docs/PLAN.md).

## Building

1. Install Visual Studio with C++ tools and vcpkg (set `VCPKG_ROOT`).
2. Clone a CommonLibF4 fork that matches your Fallout 4 build into `extern/CommonLibF4`.
3. `cmake --preset release` then `cmake --build --preset release`.

Without CommonLibF4, only the fluid library builds.

## Credits and license

4Strands is licensed under the GNU GPL v3 (see LICENSE). It is derived from [OStrands](https://gitlab.com/LilRicefield/ostrands) by Saint, also GPLv3, and reimplements features of OCum for Fallout 4.
