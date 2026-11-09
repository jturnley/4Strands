#include "fluid/Opening.h"

#include <algorithm>
#include <cmath>

namespace FourStrands::Fluid
{
	namespace
	{
		constexpr float kVaginaDrag = 2.0f;    // per second: lubricated walls
		constexpr float kUterusDrag = 4.0f;
		constexpr float kAnalDrag = 3.0f;
		constexpr float kVault = 1.3f;         // the vagina's deepest link widens this much toward the cervix
		constexpr float kUptakePull = 6.0f;    // m/s² inward through the os at full uptake
		constexpr float kSqueezePull = 8.0f;   // m/s² outward along the vagina at full squeeze
		constexpr float kShakePull = 2.5f;     // m/s² outward at full agitation
		constexpr float kSqueezeNarrow = 0.35f;
	}

	OpeningCanals BuildCanals(const OpeningShape& a_shape, float a_seep, float a_osOpen, const OpeningMood& a_mood)
	{
		OpeningCanals out;
		auto add = [&](Vec3 a_a, Vec3 a_b, float a_ra, float a_rb, bool a_first, float a_drag) {
			Canal canal;
			canal.a = a_a;
			canal.b = a_b;
			canal.radiusA = a_ra;
			canal.radiusB = a_rb;
			canal.previous = a_first ? -1 : static_cast<int>(out.canals.size()) - 1;
			canal.open = a_first;
			canal.drag = a_drag;
			if (!a_first) {
				out.canals.back().next = static_cast<int>(out.canals.size());
			}
			out.canals.push_back(canal);
			return static_cast<int>(out.canals.size()) - 1;
		};
		const float squeeze = std::clamp(a_mood.squeeze, 0.0f, 1.0f);
		const float shake = std::clamp(a_mood.agitation, 0.0f, 1.0f);
		const float uptake = std::clamp(a_mood.uptake, 0.0f, 1.0f);
		if (a_shape.rings.size() >= 2) {
			// the vagina: ring to ring, then on to the cervix; tightened and pushed outward by a squeeze, shaken outward
			const float narrow = 1.0f - kSqueezeNarrow * squeeze;
			const float outward = -(kSqueezePull * squeeze + kShakePull * shake);
			for (std::size_t i = 0; i + 1 < a_shape.rings.size(); ++i) {
				const int k = add(a_shape.rings[i].centre, a_shape.rings[i + 1].centre, a_shape.rings[i].radius * narrow,
					a_shape.rings[i + 1].radius * narrow, i == 0, kVaginaDrag);
				out.canals[static_cast<std::size_t>(k)].pull = outward;
				out.walls.push_back(k);
			}
			const auto& last = a_shape.rings.back();
			const int   vault = add(last.centre, a_shape.cervix, last.radius * narrow, last.radius * kVault, false, kVaginaDrag);
			// the uptake draws from the vault toward the os; else it is pushed out with the rest
			out.canals[static_cast<std::size_t>(vault)].pull = uptake > 0.0f ? kUptakePull * uptake : outward;
			out.walls.push_back(vault);
			// the os: the cervix to the uterus's near end, as narrow as it is open. Closed, it lets nothing in, but
			// for the uptake, which draws through it
			const Vec3 near = a_shape.uterusCentre - a_shape.uterusAxis * (0.5f * a_shape.uterusLength);
			const Vec3 far = a_shape.uterusCentre + a_shape.uterusAxis * (0.5f * a_shape.uterusLength);
			const bool open = a_shape.os.radius >= a_osOpen;
			out.os = add(a_shape.cervix, near, std::max(a_shape.os.radius, uptake > 0.0f ? a_osOpen : 0.0f),
				std::max(a_shape.os.radius, uptake > 0.0f ? a_osOpen : 0.0f), false, kUterusDrag);
			auto& os = out.canals[static_cast<std::size_t>(out.os)];
			os.inflow = open || uptake > 0.0f ? -1.0f : 0.0f;
			os.pull = kUptakePull * uptake;
			// the uterus: closed at the fundus; behind a closed os it lets fluid back out only at a_seep (faster as she
			// moves), and the uptake draws it in
			out.uterus = add(near, far, a_shape.uterusRadius, a_shape.uterusRadius, false, kUterusDrag);
			auto& uterus = out.canals[static_cast<std::size_t>(out.uterus)];
			uterus.seep = open ? -1.0f : std::max(0.0f, a_seep) * (1.0f + 3.0f * shake) * (1.0f - uptake);
			uterus.pull = kUptakePull * uptake;
		}
		// the anal canal: its own chain, open at the anus, closed at its deepest ring
		if (a_shape.analRings.size() >= 2) {
			for (std::size_t i = 0; i + 1 < a_shape.analRings.size(); ++i) {
				const int k = add(a_shape.analRings[i].centre, a_shape.analRings[i + 1].centre, a_shape.analRings[i].radius,
					a_shape.analRings[i + 1].radius, i == 0, kAnalDrag);
				out.canals[static_cast<std::size_t>(k)].pull = -kShakePull * shake;
				out.anal.push_back(k);
			}
		}
		return out;
	}

	void Absorb(std::vector<Particle>& a_particles, const OpeningCanals& a_canals, float a_seconds, float a_grace, float a_rate,
		std::mt19937& a_random)
	{
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		std::erase_if(a_particles, [&](const Particle& a_particle) {
			if (a_particle.canal < 0 || a_particle.age < a_grace) {
				return false;
			}
			const float rate = a_particle.canal == a_canals.uterus ? 0.5f * a_rate : a_rate;
			return unit(a_random) < rate * a_seconds;
		});
	}

	void Contractions::Start(float a_strength, std::mt19937& a_random)
	{
		std::uniform_int_distribution<int>    count(5, 8);
		std::uniform_real_distribution<float> period(0.7f, 0.9f);
		_strength = std::clamp(a_strength, 0.0f, 1.0f);
		_left = count(a_random);
		_period = period(a_random);
		_time = 0.0f;
	}

	float Contractions::Step(float a_seconds)
	{
		if (_left <= 0) {
			return 0.0f;
		}
		_time += a_seconds;
		if (_time >= _period) {
			_time -= _period;
			--_left;
			_strength *= 0.85f;
			if (_left <= 0) {
				return 0.0f;
			}
		}
		// each pulse: tightens in a tenth of it, eases over the rest
		const float phase = _time / _period;
		const float shape = phase < 0.1f ? phase / 0.1f : std::exp(-(phase - 0.1f) * 4.0f);
		return _strength * shape;
	}
}
