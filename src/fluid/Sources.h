#pragma once

#include "fluid/Kinds.h"
#include "fluid/Solver.h"

#include <random>
#include <span>
#include <vector>

namespace FourStrands::Fluid
{
	// Vaginal lubrication: made all along the canal's walls while she is aroused. `wetness` is the woman (0 dry to 1
	// very wet), `arousal` the moment (0-1, from 4Stim's excitement). At low levels it is a thin coating that clings to
	// the walls; at high ones enough that it runs out and drips. The particles start on the wall, at rest, inside their
	// canal, and Kind::kSecretion's traits make them cling, gather and run slowly
	class Secretion
	{
	public:
		static constexpr float kMostPerSecond = 40.0f;  // particles a second at full wetness and arousal

		// a_walls: the canals (indices into a_canals) it comes from: the vagina's, not the os or uterus
		void Step(float a_seconds, float a_wetness, float a_arousal, std::span<const Canal> a_canals, std::span<const int> a_walls,
			std::mt19937& a_random, std::vector<Particle>& a_out);

	private:
		float _due{ 0.0f };
	};

	// The urethra: Skene's fluid (a trickle to a sudden flood: "squirting") or urine, as a jet. A squirt comes in a few
	// pulses that fade; urine as one steady stream until stopped
	class Urethra
	{
	public:
		// strength 0-1: a trickle (0) to a flood (1)
		void Squirt(float a_strength, std::mt19937& a_random);
		void Urinate(float a_seconds);
		void Stop();
		bool Active() const { return _left > 0.0f; }

		// a_direction: out of her, along the urethra's opening
		void Step(float a_seconds, Vec3 a_position, Vec3 a_direction, float a_spacing, std::mt19937& a_random, std::vector<Particle>& a_out);

	private:
		struct Pulse
		{
			float start;
			float length;
			float strength;
		};

		Kind               _kind{ Kind::kSkene };
		std::vector<Pulse> _pulses;
		float              _time{ 0.0f };
		float              _left{ 0.0f };
		float              _due{ 0.0f };
	};

	// Whether her climax brings a squirt. Each woman has her own chance (`a_chance`, 0-1). Every climax without one
	// takes a fifth of it away, so after five climaxes without a squirt she won't squirt in this scene; a squirt leaves
	// the chance where it is
	class SquirtOdds
	{
	public:
		explicit SquirtOdds(float a_chance = 0.5f) : _chance(a_chance) {}

		bool  Roll(std::mt19937& a_random);
		float Chance() const;
		int   Misses() const { return _misses; }

	private:
		float _chance;
		int   _misses{ 0 };
	};

	// How wet each skin patch is (0-1): fluid touching it wets it at its kind's rate, and it dries over its kind's fade.
	// For drawing: wet skin is drawn glossier. The patches are Skin::Patches(), whose count may change (it then restarts)
	class Wetness
	{
	public:
		void Step(float a_seconds, std::span<const Particle> a_particles, std::size_t a_capsules, std::size_t a_patches);
		std::span<const float> Values() const { return _wet; }

	private:
		std::vector<float> _wet;
		std::vector<float> _fade;
	};
}
