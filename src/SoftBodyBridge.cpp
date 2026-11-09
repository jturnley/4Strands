#include "SoftBodyBridge.h"

#include "SoftBodyFO4API.h"

namespace FourStrands::SoftBodyBridge
{
	namespace
	{
		SoftBodyFO4_GetCanal_t      getCanal{ nullptr };
		SoftBodyFO4_SetUterusFill_t setFill{ nullptr };
		SoftBodyFO4_SetOsOpen_t     setOs{ nullptr };
		SoftBodyFO4_SetCanalSqueeze_t setSqueeze{ nullptr };

		Fluid::Vec3 ToSolver(const Frame& a_frame, const float* a_point)
		{
			return (Fluid::Vec3{ a_point[0], a_point[1], a_point[2] } - a_frame.origin) * a_frame.scale;
		}
	}

	bool Connect()
	{
		if (getCanal) {
			return true;
		}
		const auto module = REX::W32::GetModuleHandleW(L"SoftBodyFO4.dll");
		if (!module) {
			return false;
		}
		getCanal = reinterpret_cast<SoftBodyFO4_GetCanal_t>(REX::W32::GetProcAddress(module, "SoftBodyFO4_GetCanal"));
		setFill = reinterpret_cast<SoftBodyFO4_SetUterusFill_t>(REX::W32::GetProcAddress(module, "SoftBodyFO4_SetUterusFill"));
		setOs = reinterpret_cast<SoftBodyFO4_SetOsOpen_t>(REX::W32::GetProcAddress(module, "SoftBodyFO4_SetOsOpen"));
		setSqueeze = reinterpret_cast<SoftBodyFO4_SetCanalSqueeze_t>(REX::W32::GetProcAddress(module, "SoftBodyFO4_SetCanalSqueeze"));
		if (getCanal) {
			REX::INFO("SoftBody FO4 found: fluid goes into and out of her canal and uterus");
		}
		return getCanal != nullptr;
	}

	bool Connected()
	{
		return getCanal != nullptr;
	}

	std::optional<Woman> Read(const Frame& a_frame)
	{
		if (!getCanal) {
			return std::nullopt;
		}
		SoftBodyFO4API::Canal canal{};
		if (!getCanal(&canal) || canal.herFormID == 0 || canal.ringCount < 2) {
			return std::nullopt;
		}
		Woman woman;
		woman.formID = canal.herFormID;
		auto& shape = woman.shape;
		for (std::uint32_t i = 0; i < canal.ringCount && i < SoftBodyFO4API::kMaxRings; ++i) {
			shape.rings.push_back({ ToSolver(a_frame, canal.rings[i].centre), canal.rings[i].radius * a_frame.scale });
		}
		shape.cervix = ToSolver(a_frame, canal.cervix);
		shape.os = { ToSolver(a_frame, canal.os.centre), canal.os.radius * a_frame.scale };
		shape.uterusCentre = ToSolver(a_frame, canal.uterusCentre);
		shape.uterusAxis = { canal.uterusAxis[0], canal.uterusAxis[1], canal.uterusAxis[2] };
		shape.uterusLength = canal.uterusLength * a_frame.scale;
		shape.uterusRadius = canal.uterusRadius * a_frame.scale;
		woman.osRadius = canal.os.radius;
		// version 2: the urethra (a version 1 SoftBody leaves these zero)
		const bool urethra = canal.version >= 2 &&
		                     (canal.urethraOut[0] != 0.0f || canal.urethraOut[1] != 0.0f || canal.urethraOut[2] != 0.0f);
		if (urethra) {
			woman.urethra = ToSolver(a_frame, canal.urethra);
			woman.urethraOut = { canal.urethraOut[0], canal.urethraOut[1], canal.urethraOut[2] };
		}
		// version 3: the anal canal, rim first
		if (canal.version >= 3) {
			for (std::uint32_t i = 0; i < canal.analRingCount && i < SoftBodyFO4API::kMaxRings; ++i) {
				shape.analRings.push_back({ ToSolver(a_frame, canal.analRings[i].centre), canal.analRings[i].radius * a_frame.scale });
			}
		}
		return woman;
	}

	void SetUterusFill(std::uint32_t a_formID, float a_fill)
	{
		if (setFill) {
			setFill(a_formID, a_fill);
		}
	}

	void SetOsOpen(std::uint32_t a_formID, float a_radius)
	{
		if (setOs) {
			setOs(a_formID, a_radius);
		}
	}

	void SetCanalSqueeze(std::uint32_t a_formID, float a_squeeze)
	{
		if (setSqueeze) {
			setSqueeze(a_formID, a_squeeze);
		}
	}
}
