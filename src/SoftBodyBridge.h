#pragma once

#include "fluid/Opening.h"

#include <cstdint>
#include <optional>

namespace FourStrands::SoftBodyBridge
{
	// The game's frame and units mapped to the solver's: solver = (game - origin) * scale
	struct Frame
	{
		Fluid::Vec3 origin;
		float       scale{ 0.0142875f };  // metres per game unit
	};

	// What SoftBody FO4 reports of the woman it follows, in the solver's frame
	struct Woman
	{
		std::uint32_t        formID{};
		Fluid::OpeningShape  shape;
		std::optional<Fluid::Vec3> urethra;     // and its direction out of her, when SoftBody reports it (API v2)
		Fluid::Vec3          urethraOut;
		float                osRadius{};        // game units, as reported
	};

	// Looks for SoftBodyFO4.dll and its functions. Safe to call again; false if it isn't loaded
	bool Connect();
	bool Connected();

	// Her canal now, or nothing (no scene, or SoftBody's canal isn't built)
	std::optional<Woman> Read(const Frame& a_frame);

	// How full her uterus is (0-1; SoftBody swells it), and how far to open the os (game units; 0 back to rest)
	void SetUterusFill(std::uint32_t a_formID, float a_fill);
	void SetOsOpen(std::uint32_t a_formID, float a_radius);
	// her vaginal canal tightens, 0-1 (her climax's contractions; SoftBody API v3)
	void SetCanalSqueeze(std::uint32_t a_formID, float a_squeeze);
}
