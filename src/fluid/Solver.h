#pragma once

#include "fluid/Ground.h"
#include "fluid/Particle.h"
#include "fluid/Skin.h"

#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace FourStrands::Fluid
{
	struct Parameters
	{
		std::uint32_t iterations{ 4 };
		float         cohesion{ 4.0f };
		float         viscosity{ 40.0f };
		float         gravity{ 9.81f };
		float         stretch{ 3.0f };
		float         friction{ 30.0f };
		float         grip{ 0.6f };
		float         liquefy{ 360.0f };  // seconds: semen's thick first gel thins by about two-thirds in this
	};

	struct Anchor
	{
		std::size_t   particle{};
		Vec3          point;
		float         rest{};
		std::uint32_t owner{};
	};

	struct Capsule
	{
		Vec3  a;
		Vec3  b;
		float radius{};
		Vec3  velocityA;
		Vec3  velocityB;
		bool  shaft{ false };  // a penis: it also pushes and drags the fluid inside her canals (the rest skip it there)
	};

	struct Hole
	{
		Vec3  center;
		float radius{};
	};

	// The inside of an opening, as a chain of tapered capsules from its entrance inward (the vagina's rings, the os,
	// the uterus). A particle in one is held inside its radius; at its ends it passes to the neighbouring one, leaves
	// through an open entrance, or stops at a closed end. Through `previous` it may only go back out of a canal whose
	// `seep` is set at that many particles a second (the uterus behind a closed os), so what is inside drips out slowly.
	// `drag` (per second) is the flow lost along the walls. Rebuilt every frame from SoftBody FO4's canal (its order must
	// stay the same, since particles keep their canal's index)
	struct Canal
	{
		Vec3  a;                  // the entrance end (toward `previous`, or outside)
		Vec3  b;                  // the deep end (toward `next`)
		float radiusA{};
		float radiusB{};
		int   previous{ -1 };
		int   next{ -1 };
		bool  open{ false };      // with no `previous`: whether `a` lets particles out (and in)
		float seep{ -1.0f };      // < 0: back out through `a` freely; else that many particles a second
		float inflow{ -1.0f };    // < 0: in from `previous` freely; else that many particles a second (0: shut, a closed os)
		float drag{ 0.0f };
		float pull{ 0.0f };       // m/s² along a→b on what is inside (a womb drawing semen in; > 0 inward, < 0 out)
	};

	struct Colliders
	{
		std::span<const Capsule> capsules;
		std::span<const Hole>    holes;
		std::span<const Canal>   canals;
		const Skin*              skin{ nullptr };
	};

	class Solver
	{
	public:
		Solver(float a_spacing, Parameters a_parameters);

		static bool Valid(const Parameters& a_parameters);
		void        Configure(const Parameters& a_parameters);

		void  Step(std::span<Particle> a_particles, std::span<Anchor> a_anchors, const Colliders& a_colliders, const Ground& a_ground, float a_seconds);
		float Spacing() const { return _spacing; }

	private:
		struct Cell
		{
			int  x, y, z;
			bool operator==(const Cell&) const = default;
		};

		struct CellHash
		{
			std::size_t operator()(Cell a_cell) const noexcept;
		};

		Cell  CellOf(Vec3 a_point) const;
		float Weight(Vec3 a_delta) const;
		Vec3  Gradient(Vec3 a_delta) const;
		void  FindNeighbors(std::span<const Particle> a_particles, bool a_all);
		void  Collide(std::span<const Particle> a_particles, const Colliders& a_colliders, bool a_pull);
		void  Plugs(const Colliders& a_colliders);
		void  Contain(std::span<const Particle> a_particles, const Colliders& a_colliders);
		void  Blend(std::span<Particle> a_particles, float a_seconds);
		void  PassCanals(std::span<Particle> a_particles, const Colliders& a_colliders, float a_seconds);
		static std::optional<Vec3> Carrier(const Colliders& a_colliders, int a_surface, float a_along, Vec3 a_position);

		float                                                _spacing;
		float                                                _radiusSquared;
		float                                                _restWeight{};
		Parameters                                           _parameters;
		std::vector<Vec3>                                    _predicted;
		std::vector<Vec3>                                    _velocity;
		std::vector<Vec3>                                    _correction;
		std::vector<float>                                   _lambda;
		std::vector<float>                                   _seepBudget;  // per canal: particles it may let back out now
		std::vector<float>                                   _inBudget;    // per canal: particles it may let in now
		std::vector<int>                                     _plug;        // per canal: the shaft through its open entrance, or -1
		std::vector<bool>                                    _anchored;
		std::vector<float>                                   _floor;
		std::vector<float>                                   _weights;
		std::vector<int>                                     _contact;
		std::vector<float>                                   _along;
		std::vector<Vec3>                                    _normal;
		std::vector<std::vector<std::size_t>>                _neighbors;
		std::unordered_map<Cell, std::vector<std::size_t>, CellHash> _grid;
	};
}
