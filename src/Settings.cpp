#include "Settings.h"

#include <fstream>

namespace FourStrands::Settings
{
	namespace
	{
		Values values;

		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r");
			return std::string(a_text.substr(first, last - first + 1));
		}

		bool ToBool(const std::string& a_value)
		{
			return a_value == "1" || _stricmp(a_value.c_str(), "true") == 0;
		}

		void Set(const std::string& a_section, const std::string& a_key, const std::string& a_value)
		{
			const auto is = [&](const char* a_section2, const char* a_key2) {
				return _stricmp(a_section.c_str(), a_section2) == 0 && _stricmp(a_key.c_str(), a_key2) == 0;
			};
			try {
				if (is("General", "Enabled")) {
					values.enabled = ToBool(a_value);
				} else if (is("General", "DebugLog")) {
					values.debugLog = ToBool(a_value);
				} else if (is("Fluids", "Enabled")) {
					values.fluids = ToBool(a_value);
				} else if (is("Fluids", "Wetness")) {
					values.fluidWetness = std::stof(a_value);
				} else if (is("Fluids", "SquirtChance")) {
					values.fluidSquirtChance = std::stof(a_value);
				} else if (is("Fluids", "MaxParticles")) {
					values.fluidMaxParticles = static_cast<std::uint32_t>(std::stoul(a_value));
				} else if (is("Fluids", "PenisRadius")) {
					values.fluidPenisRadius = std::stof(a_value);
				} else if (is("Spike", "ProbeBody")) {
					values.probeBody = ToBool(a_value);
				} else if (is("Spike", "Strand")) {
					values.strand = ToBool(a_value);
				} else if (is("Spike", "StrandTemplate")) {
					values.strandTemplate = a_value;
				} else if (is("Spike", "StrandBoneA")) {
					values.strandBoneA = a_value;
				} else if (is("Spike", "StrandBoneB")) {
					values.strandBoneB = a_value;
				} else if (is("Spike", "StrandRadius")) {
					values.strandRadius = std::stof(a_value);
				} else if (is("Spike", "StrandDynamicFlags")) {
					values.strandDynamicFlags = static_cast<std::uint32_t>(std::stoul(a_value, nullptr, 0));
				} else if (is("Spike", "StrandDynamicOffset")) {
					values.strandDynamicOffset = static_cast<std::uint32_t>(std::stoul(a_value, nullptr, 0));
				} else if (is("Spike", "StrandDynamicStride")) {
					values.strandDynamicStride = static_cast<std::uint32_t>(std::stoul(a_value, nullptr, 0));
				}
			} catch (const std::exception&) {
				REX::WARN("4Strands.ini: bad value for [{}] {}: {}", a_section, a_key, a_value);
			}
		}
	}

	const Values& Get()
	{
		return values;
	}

	void Load()
	{
		std::ifstream file{ "Data/F4SE/Plugins/4Strands.ini" };
		if (!file) {
			REX::INFO("4Strands.ini not found; using defaults");
			return;
		}
		std::string section;
		std::string line;
		while (std::getline(file, line)) {
			auto text = Trim(line);
			if (text.empty() || text[0] == ';' || text[0] == '#') {
				continue;
			}
			if (text.front() == '[' && text.back() == ']') {
				section = Trim(std::string_view(text).substr(1, text.size() - 2));
				continue;
			}
			const auto equals = text.find('=');
			if (equals != std::string::npos) {
				Set(section, Trim(std::string_view(text).substr(0, equals)), Trim(std::string_view(text).substr(equals + 1)));
			}
		}
	}
}
