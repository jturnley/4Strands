#include "fluid/Opening.h"

#include <algorithm>

namespace FourStrands::Fluid
{
	namespace
	{
		constexpr float kVaginaDrag = 2.0f;  // per second: lubricated walls
		constexpr float kUterusDrag = 4.0f;
	}

	OpeningCanals BuildCanals(const OpeningShape& a_shape, float a_seep, float a_osOpen)
	{
		OpeningCanals out;
		if (a_shape.rings.size() < 2) {
			return out;
		}
		auto add = [&](Vec3 a_a, Vec3 a_b, float a_ra, float a_rb) {
			Canal canal;
			canal.a = a_a;
			canal.b = a_b;
			canal.radiusA = a_ra;
			canal.radiusB = a_rb;
			canal.previous = out.canals.empty() ? -1 : static_cast<int>(out.canals.size()) - 1;
			canal.open = out.canals.empty();
			canal.drag = kVaginaDrag;
			if (!out.canals.empty()) {
				out.canals.back().next = static_cast<int>(out.canals.size());
			}
			out.canals.push_back(canal);
			return static_cast<int>(out.canals.size()) - 1;
		};
		// the vagina: ring to ring, then on to the cervix
		for (std::size_t i = 0; i + 1 < a_shape.rings.size(); ++i) {
			out.walls.push_back(add(a_shape.rings[i].centre, a_shape.rings[i + 1].centre, a_shape.rings[i].radius, a_shape.rings[i + 1].radius));
		}
		const auto& last = a_shape.rings.back();
		out.walls.push_back(add(last.centre, a_shape.cervix, last.radius, last.radius));
		// the os: the cervix to the uterus's near end, as narrow as the os is open
		const Vec3 uterusNear = a_shape.uterusCentre - a_shape.uterusAxis * (0.5f * a_shape.uterusLength);
		const Vec3 uterusFar = a_shape.uterusCentre + a_shape.uterusAxis * (0.5f * a_shape.uterusLength);
		out.os = add(a_shape.cervix, uterusNear, a_shape.os.radius, a_shape.os.radius);
		out.canals[static_cast<std::size_t>(out.os)].drag = kUterusDrag;
		// the uterus: closed at the fundus; behind a closed os it lets fluid back out only at a_seep
		out.uterus = add(uterusNear, uterusFar, a_shape.uterusRadius, a_shape.uterusRadius);
		auto& uterus = out.canals[static_cast<std::size_t>(out.uterus)];
		uterus.drag = kUterusDrag;
		uterus.seep = a_shape.os.radius >= a_osOpen ? -1.0f : std::max(0.0f, a_seep);
		return out;
	}
}
