#pragma once

namespace FourStrands::Settings
{
	struct Values
	{
		bool enabled{ true };
		bool debugLog{ false };

		// [Fluids] the scene's fluids in SoftBody FO4's canals (logged; not drawn yet)
		bool          fluids{ true };
		float         fluidWetness{ 1.0f };       // × each woman's own wetness (0.25-1)
		float         fluidSquirtChance{ 0.35f }; // the average woman's chance her climax brings a squirt
		std::uint32_t fluidMaxParticles{ 3000 };
		float         fluidPenisRadius{ 1.6f };   // game units: his shaft, as it pushes fluid

		// [Spike] Phase 0 checks: read body vertex data, and draw one strand mesh.
		bool          probeBody{ true };
		bool          strand{ true };
		std::string   strandTemplate{ "Interiors\\Building\\Glass\\BldGlassGlow01.nif" };
		std::string   strandBoneA{ "LArm_Hand" };
		std::string   strandBoneB{ "RArm_Hand" };
		float         strandRadius{ 0.4f };
		std::uint32_t strandDynamicFlags{ 0 };
		std::uint32_t strandDynamicOffset{ 0 };  // 0: find it; otherwise use this field of BSDynamicTriShape
		std::uint32_t strandDynamicStride{ 16 };
	};

	const Values& Get();

	// Reads Data/F4SE/Plugins/4Strands.ini. Missing keys keep their defaults.
	void Load();
}
