#pragma once

#include <cstdint>
#include <span>

namespace FourStrands::Film
{
	// The fluid film on her skin (the method is DripSim's, by AntiArc, GPL-3: a plugin-owned texture put in place of an
	// overlay's, rewritten as the fluid changes). SoftBody FO4's body carries the overlays: "SoftBodyFilm" (her body) and
	// "SoftBodyFilmVulva", copies of her skin drawn over it as an alpha-blended decal with Materials\SoftBodyFO4\Film.bgsm,
	// whose diffuse is a transparent placeholder. This puts a 2048 x 2048 RGBA texture of our own in the placeholder's
	// place: everything on the skin's UVs (the CBBE layout) that is drawn into it shows on her.
	//
	// Stage 1: proving it works in FO4. Until the film simulation comes, it draws a test pattern while a scene runs.
	//
	// Note: FO4 shares a texture object between everyone using the same material, so while it runs, any other woman
	// wearing this body shows the same film.

	constexpr int kSize = 2048;

	// Main thread, every frame while a scene runs: finds her overlays (a_her: her 3D's root) and puts our texture in
	void Attach(RE::NiAVObject* a_her);
	// Main thread: puts the placeholder back (scene end, a load)
	void Detach();

	// Main thread: the pixels (ABGR: R in the low byte, kSize x kSize) and the rows that changed; they're uploaded on the
	// render thread at its next Present
	void Submit(std::span<const std::uint32_t> a_pixels, int a_firstRow, int a_rows);

	// Main thread: a moving test pattern (stage 1)
	void TestPattern(float a_seconds);
}
