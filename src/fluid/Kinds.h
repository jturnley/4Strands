#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace FourStrands::Fluid
{
	// What a particle is. Each kind scales the solver's Parameters (semen is the reference, 1 everywhere), so the
	// tuning of Parameters keeps its meaning and the kinds keep their differences.
	enum class Kind : std::uint8_t
	{
		kSemen,
		kSecretion,  // vaginal lubrication: slick, clings to skin, hangs as a large drop before it falls
		kSkene,      // Skene's gland fluid ("squirting"): water, sticks to neither skin nor itself
		kUrine,      // as kSkene, light yellow
		kFoam,       // semen and secretion whipped together by thrusting: white, thick, clinging
		kTotal
	};

	struct KindTraits
	{
		float cohesion;     // × Parameters::cohesion: pull toward its own kind (a drop holds together, water doesn't)
		float viscosity;    // × Parameters::viscosity
		float friction;     // × Parameters::friction: how slowly it runs on skin
		float grip;         // × Parameters::grip: the push along the skin it holds against before sliding at all
		float hang;         // the most a surface may face down and still hold it (normal.z above this): -1 holds it
		                    // under anything, 0 only on top. Below it the particle is let go and drips
		float stickReach;   // × the solver's stick reach: how far off the skin it is still pulled back onto it
		float wetting;      // how much one second of contact wets a skin patch (its shine), 0 for none
		float wetFade;      // seconds a wet patch takes to dry by about two-thirds
		std::array<float, 4> colour;  // linear RGBA, for drawing
	};

	// The reference table. Secretion runs about a third as fast as semen and hangs under nearly anything, so it gathers
	// into a big drop before the drop's weight tears it off (the cohesion and stretch links decide when); Skene's fluid
	// and urine are water: almost no cohesion, viscosity or grip, and they never hang
	inline constexpr std::array<KindTraits, static_cast<std::size_t>(Kind::kTotal)> kKinds{ {
		{ 1.0f, 1.0f, 1.0f, 1.0f, -0.5f, 1.0f, 0.3f, 30.0f, { 0.92f, 0.92f, 0.88f, 0.85f } },    // semen
		{ 1.6f, 0.6f, 3.0f, 2.5f, -0.9f, 1.5f, 1.0f, 90.0f, { 0.95f, 0.95f, 0.95f, 0.25f } },    // secretion
		{ 0.05f, 0.08f, 0.05f, 0.0f, 0.2f, 0.0f, 0.1f, 15.0f, { 0.95f, 0.97f, 1.0f, 0.15f } },   // Skene's
		{ 0.05f, 0.08f, 0.05f, 0.0f, 0.2f, 0.0f, 0.1f, 15.0f, { 1.0f, 0.93f, 0.55f, 0.35f } },   // urine
		{ 2.0f, 2.0f, 4.0f, 3.0f, -0.8f, 1.5f, 0.6f, 60.0f, { 0.98f, 0.98f, 0.96f, 0.95f } },    // foam
	} };

	inline const KindTraits& Traits(Kind a_kind)
	{
		return kKinds[static_cast<std::size_t>(a_kind) < kKinds.size() ? static_cast<std::size_t>(a_kind) : 0];
	}

	// Semen right after it is shot is a thick gel; it liquefies over the next quarter of an hour or so (its own enzymes),
	// so at first it clumps and later runs and drips. Its viscosity starts this many times its liquid one, its cohesion
	// this many times
	inline constexpr float kGelViscosity = 3.0f;
	inline constexpr float kGelCohesion = 1.8f;
	inline constexpr float kGelCling = 3.0f;

	// A semen-and-secretion mix's share of semen (a particle that hasn't mixed is all its own kind)
	inline float SemenShare(std::uint8_t a_kind, float a_mix)
	{
		if (a_mix >= 0.0f) {
			return a_mix;
		}
		return a_kind == static_cast<std::uint8_t>(Kind::kSemen) ? 1.0f : 0.0f;
	}

	// Whether a kind mixes with semen and secretion (they blend; water doesn't)
	inline bool Mixes(std::uint8_t a_kind)
	{
		return a_kind == static_cast<std::uint8_t>(Kind::kSemen) || a_kind == static_cast<std::uint8_t>(Kind::kSecretion) ||
		       a_kind == static_cast<std::uint8_t>(Kind::kFoam);
	}

	// A particle's traits now: semen and secretion blend by their mix, and the semen in it is as thick as its age leaves
	// it (a_liquefy: seconds for the gel to thin by about two-thirds)
	inline KindTraits Effective(std::uint8_t a_kind, float a_mix, float a_age, float a_liquefy)
	{
		KindTraits out = Traits(static_cast<Kind>(a_kind));
		if (a_kind == static_cast<std::uint8_t>(Kind::kSemen) || a_kind == static_cast<std::uint8_t>(Kind::kSecretion)) {
			const float m = SemenShare(a_kind, a_mix);
			const auto& semen = Traits(Kind::kSemen);
			const auto& wet = Traits(Kind::kSecretion);
			auto lerp = [m](float a_wet, float a_semen) { return a_wet + (a_semen - a_wet) * m; };
			out = { lerp(wet.cohesion, semen.cohesion), lerp(wet.viscosity, semen.viscosity), lerp(wet.friction, semen.friction),
				lerp(wet.grip, semen.grip), lerp(wet.hang, semen.hang), lerp(wet.stickReach, semen.stickReach),
				lerp(wet.wetting, semen.wetting), lerp(wet.wetFade, semen.wetFade),
				{ lerp(wet.colour[0], semen.colour[0]), lerp(wet.colour[1], semen.colour[1]), lerp(wet.colour[2], semen.colour[2]),
					lerp(wet.colour[3], semen.colour[3]) } };
			if (m > 0.0f && a_liquefy > 0.0f) {
				const float gel = std::exp(-a_age / a_liquefy);
				out.viscosity *= 1.0f + (kGelViscosity - 1.0f) * gel * m;
				out.cohesion *= 1.0f + (kGelCohesion - 1.0f) * gel * m;
				out.friction *= 1.0f + (kGelCling - 1.0f) * gel * m;   // a gel clings: it creeps rather than runs
				out.grip *= 1.0f + (kGelCling - 1.0f) * gel * m;
			}
		}
		return out;
	}
}
