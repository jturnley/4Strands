#pragma once

namespace FourStrands::BodyProbe
{
	// Phase 0 check: logs every skinned geometry under the actor's 3D (vertex layout, CPU vertex copy, bones)
	// and, for the largest one, a few vertices skinned to world space with their distance to their main bone.
	// Main thread only.
	void Probe(RE::Actor* a_actor);
}
