#pragma once

#include "fluid/Solver.h"

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
	};

	// The canal chain the solver holds fluid in, and which link is which. Always the same count and order for a given
	// ring count, so particles keep their canal from frame to frame
	struct OpeningCanals
	{
		std::vector<Canal> canals;
		std::vector<int>   walls;      // the vagina's links: where secretion comes from
		int                os{ -1 };
		int                uterus{ -1 };
	};

	// a_seep: particles a second the uterus lets back out through a closed os (semen trapped in it drips out slowly);
	// a_osOpen: the os radius above which it no longer holds anything back (the os is then open: free flow)
	OpeningCanals BuildCanals(const OpeningShape& a_shape, float a_seep, float a_osOpen);
}
