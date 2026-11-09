#pragma once

#include "fluid/Solver.h"

#include <random>
#include <vector>

namespace FourStrands::Fluid
{
	// A woman's vagina, cervix and uterus as SoftBody FO4 reports them (SoftBodyFO4API.h), already in the solver's
	// frame and units
	struct OpeningShape
	{
		struct Ring
		{
			Vec3  centre;
			float radius{};
		};

		std::vector<Ring> rings;  // the vagina's, entrance first
		Vec3              cervix;
		Ring              os;
		Vec3              uterusCentre;
		Vec3              uterusAxis;  // unit, os toward fundus
		float             uterusLength{};
		float             uterusRadius{};
		std::vector<Ring> analRings;   // the anal canal's: its rim (the anus) first, inward; the deepest is its closed end
	};

	// What her body is doing to what is inside her, each 0-1
	struct OpeningMood
	{
		float uptake{};     // her climax: the womb draws semen waiting at the cervix in through the os (open inward only)
		float squeeze{};    // her climax's contractions: the vagina tightens and pushes what is in it out
		float agitation{};  // how hard she is moving (her pelvis's acceleration): shakes fluid out, and the uterus seeps faster
	};

	// The canal chain the solver holds fluid in, and which link is which. Always the same count and order for a given
	// ring count, so particles keep their canal from frame to frame
	struct OpeningCanals
	{
		std::vector<Canal> canals;
		std::vector<int>   walls;      // the vagina's links: where secretion comes from
		int                os{ -1 };
		int                uterus{ -1 };
		std::vector<int>   anal;       // the anal canal's links
	};

	// a_seep: particles a second the uterus lets back out through a closed os (semen trapped in it drips out slowly);
	// a_osOpen: the os radius above which it no longer holds anything back. A closed os lets nothing in either, but
	// for her climax's uptake. The vagina's deepest link widens toward the cervix (its vault), where semen pools while
	// she lies on her back
	OpeningCanals BuildCanals(const OpeningShape& a_shape, float a_seep, float a_osOpen, const OpeningMood& a_mood = {});

	// Fluid that has been inside her a long time is absorbed (it keeps the count bounded, and the uterus's fill drains
	// over minutes): past a_grace seconds old, each particle in the vagina or anal canal goes at a_rate a second, in the
	// uterus at half that. Removes them from a_particles
	void Absorb(std::vector<Particle>& a_particles, const OpeningCanals& a_canals, float a_seconds, float a_grace, float a_rate,
		std::mt19937& a_random);

	// Her climax's contractions: pulses about 0.8 s apart, each sharp then easing, fading over 5-8 of them. Drives
	// OpeningMood's squeeze and uptake, and SoftBody's canal squeeze
	class Contractions
	{
	public:
		void  Start(float a_strength, std::mt19937& a_random);
		float Step(float a_seconds);  // the squeeze now, 0-1
		bool  Active() const { return _left > 0; }

	private:
		float _strength{};
		float _time{};
		float _period{ 0.8f };
		int   _left{};
	};
}
