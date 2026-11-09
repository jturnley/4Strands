#pragma once

#include <array>
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
	} };

	inline const KindTraits& Traits(Kind a_kind)
	{
		return kKinds[static_cast<std::size_t>(a_kind) < kKinds.size() ? static_cast<std::size_t>(a_kind) : 0];
	}
}
