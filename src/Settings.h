#pragma once

namespace FourStrands::Settings
{
	struct Values
	{
		bool enabled{ true };
		bool debugLog{ false };

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
