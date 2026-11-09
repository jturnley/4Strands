#include "fluid/Sources.h"

#include <algorithm>
#include <cmath>

namespace FourStrands::Fluid
{
	namespace
	{
		constexpr float kPi = 3.14159265f;

		// a unit vector at right angles to a_axis
		Vec3 Across(Vec3 a_axis)
		{
			const Vec3 other = std::abs(a_axis.z) < 0.9f ? Vec3{ 0.0f, 0.0f, 1.0f } : Vec3{ 1.0f, 0.0f, 0.0f };
			return Normalize(Cross(a_axis, other), Vec3{ 1.0f, 0.0f, 0.0f });
		}
	}

	void Secretion::Step(float a_seconds, float a_wetness, float a_arousal, std::span<const Canal> a_canals, std::span<const int> a_walls,
		std::mt19937& a_random, std::vector<Particle>& a_out)
	{
		const float level = std::clamp(a_wetness, 0.0f, 1.0f) * std::pow(std::clamp(a_arousal, 0.0f, 1.0f), 1.5f);
		_due += kMostPerSecond * level * a_seconds;
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		while (_due >= 1.0f && !a_walls.empty()) {
			_due -= 1.0f;
			const int index = a_walls[static_cast<std::size_t>(unit(a_random) * static_cast<float>(a_walls.size())) % a_walls.size()];
			if (index < 0 || static_cast<std::size_t>(index) >= a_canals.size()) {
				continue;
			}
			const auto& canal = a_canals[static_cast<std::size_t>(index)];
			const Vec3  axis = Normalize(canal.b - canal.a, Vec3{ 0.0f, 0.0f, 1.0f });
			const Vec3  first = Across(axis);
			const Vec3  second = Cross(axis, first);
			const float t = unit(a_random);
			const float angle = 2.0f * kPi * unit(a_random);
			const float radius = 0.8f * (canal.radiusA + (canal.radiusB - canal.radiusA) * t);
			Particle    particle;
			particle.position = canal.a + (canal.b - canal.a) * t + (first * std::cos(angle) + second * std::sin(angle)) * radius;
			particle.life = 0.0f;
			particle.kind = static_cast<std::uint8_t>(Kind::kSecretion);
			particle.canal = static_cast<std::int16_t>(index);
			a_out.push_back(particle);
		}
		_due = std::min(_due, 4.0f);
	}

	void Urethra::Squirt(float a_strength, std::mt19937& a_random)
	{
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		const float strength = std::clamp(a_strength, 0.0f, 1.0f);
		_kind = Kind::kSkene;
		_pulses.clear();
		_time = 0.0f;
		// a trickle is one long weak pulse; a flood several hard ones, each a little weaker than the last
		const int count = 1 + static_cast<int>(std::round(strength * (2.0f + 2.0f * unit(a_random))));
		float     start = 0.0f;
		for (int i = 0; i < count; ++i) {
			const float length = (0.25f + 0.5f * unit(a_random)) * (strength > 0.2f ? 1.0f : 3.0f);
			_pulses.push_back({ start, length, strength * std::pow(0.75f, static_cast<float>(i)) });
			start += length + 0.15f + 0.4f * unit(a_random);
		}
		_left = start;
	}

	void Urethra::Urinate(float a_seconds)
	{
		_kind = Kind::kUrine;
		_pulses = { { 0.0f, std::max(0.0f, a_seconds), 0.45f } };
		_time = 0.0f;
		_left = std::max(0.0f, a_seconds);
	}

	float Urethra::DrawVolume(std::mt19937& a_random)
	{
		std::lognormal_distribution<float> volume(std::log(40.0f), 0.6f);
		return std::clamp(volume(a_random), 10.0f, 150.0f);
	}

	void Urethra::Stop()
	{
		_pulses.clear();
		_left = 0.0f;
	}

	void Urethra::Step(float a_seconds, Vec3 a_position, Vec3 a_direction, float a_spacing, std::mt19937& a_random, std::vector<Particle>& a_out)
	{
		if (_left <= 0.0f) {
			return;
		}
		float strength = 0.0f;
		for (const auto& pulse : _pulses) {
			if (_time >= pulse.start && _time < pulse.start + pulse.length) {
				// each pulse rises fast and falls off
				const float phase = (_time - pulse.start) / pulse.length;
				strength = std::max(strength, pulse.strength * std::min(1.0f, phase * 8.0f) * (1.0f - 0.5f * phase));
			}
		}
		_time += a_seconds;
		_left -= a_seconds;
		// a squirt draws on what the gland holds: the last 10 ml come weaker and weaker, and an empty gland gives nothing
		const float each = a_spacing * a_spacing * a_spacing * 1.0e6f;  // millilitres a particle
		if (_kind == Kind::kSkene) {
			if (_skene < each) {
				_left = 0.0f;
				_pulses.clear();
				return;
			}
			strength = std::min(strength, _skene / 10.0f);
		}
		if (strength <= 0.0f) {
			return;
		}
		// a trickle of 20 particles a second barely leaving her, to a flood of 500 at 3.5 m/s
		const float speed = 0.15f + 3.3f * strength;
		_due += (20.0f + 480.0f * strength * strength) * a_seconds;
		const Vec3                            out = Normalize(a_direction, Vec3{ 0.0f, 0.0f, -1.0f });
		const Vec3                            first = Across(out);
		const Vec3                            second = Cross(out, first);
		std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
		while (_due >= 1.0f) {
			_due -= 1.0f;
			if (_kind == Kind::kSkene) {
				if (_skene < each) {
					break;
				}
				_skene -= each;
			}
			Particle particle;
			// a spray that widens as it weakens
			const float wide = 0.08f + 0.25f * (1.0f - strength);
			const Vec3  aim = Normalize(out + (first * spread(a_random) + second * spread(a_random)) * wide, out);
			particle.position = a_position + (first * spread(a_random) + second * spread(a_random)) * (0.3f * a_spacing);
			particle.velocity = aim * (speed * (0.85f + 0.15f * spread(a_random)));
			particle.kind = static_cast<std::uint8_t>(_kind);
			a_out.push_back(particle);
		}
	}

	float SquirtOdds::Chance() const
	{
		return std::clamp(_chance, 0.0f, 1.0f) * std::max(0.0f, 1.0f - 0.2f * static_cast<float>(_misses));
	}

	bool SquirtOdds::Roll(std::mt19937& a_random)
	{
		const float chance = Chance();
		if (chance > 0.0f && std::uniform_real_distribution<float>(0.0f, 1.0f)(a_random) < chance) {
			return true;
		}
		++_misses;
		return false;
	}

	void Wetness::Step(float a_seconds, std::span<const Particle> a_particles, std::size_t a_capsules, std::size_t a_patches)
	{
		if (_wet.size() != a_patches) {
			_wet.assign(a_patches, 0.0f);
			_fade.assign(a_patches, 30.0f);
		}
		for (std::size_t k = 0; k < _wet.size(); ++k) {
			_wet[k] *= std::exp(-a_seconds / _fade[k]);
			if (_wet[k] < 0.01f) {
				_fade[k] = 30.0f;  // dry again: the next fluid sets its own fade
			}
		}
		for (const auto& particle : a_particles) {
			if (!particle.touching || particle.surface < 0 || static_cast<std::size_t>(particle.surface) < a_capsules) {
				continue;
			}
			const std::size_t k = static_cast<std::size_t>(particle.surface) - a_capsules;
			if (k >= _wet.size()) {
				continue;
			}
			const auto& kind = Traits(static_cast<Kind>(particle.kind));
			_wet[k] = std::min(1.0f, _wet[k] + kind.wetting * a_seconds);
			_fade[k] = std::max(_fade[k], kind.wetFade);
		}
	}
}
