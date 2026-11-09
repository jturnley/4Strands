// Offline checks of the fluid kinds, canal containment and sources (no game). Build and run:
//   xmake build FluidTests && xmake run FluidTests
#include "fluid/Kinds.h"
#include "fluid/Opening.h"
#include "fluid/Solver.h"
#include "fluid/Sources.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

using namespace FourStrands::Fluid;

namespace
{
	constexpr float kSpacing = 0.004f;  // metres
	constexpr float kStep = 1.0f / 120.0f;

	int failures = 0;

	void Check(bool a_ok, const char* a_what)
	{
		std::printf("  [%s] %s\n", a_ok ? "ok" : "FAIL", a_what);
		failures += a_ok ? 0 : 1;
	}

	// A standing woman's canal, pointing up into her: the vagina (open at its entrance), the os, the uterus
	std::vector<Canal> Canals(float a_seep, float a_osRadius)
	{
		std::vector<Canal> canals(3);
		canals[0] = { { 0, 0, 0 }, { 0, 0, 0.08f }, 0.012f, 0.012f, -1, 1, true, -1.0f, 2.0f };
		canals[1] = { { 0, 0, 0.08f }, { 0, 0, 0.09f }, a_osRadius, a_osRadius, 0, 2, false, -1.0f, 2.0f };
		canals[2] = { { 0, 0, 0.09f }, { 0, 0, 0.13f }, 0.02f, 0.02f, 1, -1, false, a_seep, 4.0f };
		return canals;
	}

	std::size_t Count(const std::vector<Particle>& a_particles, int a_canal)
	{
		std::size_t n = 0;
		for (const auto& p : a_particles) {
			n += p.canal == a_canal ? 1 : 0;
		}
		return n;
	}

	// fills the uterus with semen and lets it seep for a_seconds; returns how many are left in it
	std::size_t Uterus(float a_seep, float a_seconds)
	{
		Solver             solver(kSpacing, Parameters{});
		Ground             ground(0.5f);
		auto               canals = Canals(a_seep, 0.002f);
		std::vector<Particle> particles;
		for (int z = 0; z < 5; ++z) {
			for (int y = -3; y <= 3; ++y) {
				for (int x = -3; x <= 3; ++x) {
					Particle p;
					p.position = { x * kSpacing, y * kSpacing, 0.095f + z * kSpacing };
					p.canal = 2;
					particles.push_back(p);
				}
			}
		}
		std::vector<Anchor> anchors;
		Colliders           colliders{ {}, {}, canals, nullptr };
		for (float t = 0; t < a_seconds; t += kStep) {
			solver.Step(particles, anchors, colliders, ground, kStep);
		}
		return Count(particles, 2);
	}

	// a blob of one kind against the side of a near-vertical capsule (a thigh); returns (how far it ran down, how many
	// still touch it) after a_seconds
	std::pair<float, std::size_t> Thigh(Kind a_kind, float a_seconds)
	{
		Solver                solver(kSpacing, Parameters{});
		Ground                ground(0.5f);
		const Capsule         thigh{ { 0, 0, 0.5f }, { 0.02f, 0, 0.9f }, 0.07f, {}, {} };  // above the ground (z = 0)
		std::vector<Capsule>  capsules{ thigh };
		std::vector<Particle> particles;
		for (int z = 0; z < 3; ++z) {
			for (int y = -2; y <= 2; ++y) {
				Particle p;
				p.position = { 0.07f + 0.6f * kSpacing + 0.0125f, y * kSpacing, 0.8f + z * kSpacing };
				p.kind = static_cast<std::uint8_t>(a_kind);
				p.age = 1800.0f;  // semen liquefied: fresh, it is a gel (checked below)
				p.touching = true;
				p.surface = 0;
				particles.push_back(p);
			}
		}
		const float         start = 0.8f;
		std::vector<Anchor> anchors;
		Colliders           colliders{ capsules, {}, {}, nullptr };
		for (float t = 0; t < a_seconds; t += kStep) {
			solver.Step(particles, anchors, colliders, ground, kStep);
		}
		float       lowest = 1e9f;
		std::size_t touching = 0;
		for (const auto& p : particles) {
			if (p.touching) {
				lowest = std::min(lowest, p.position.z);
				++touching;
			}
		}
		return { touching ? start - lowest : 0.0f, touching };
	}
}

int main()
{
	std::printf("uterus\n");
	const auto sealed = Uterus(0.5f, 10.0f);
	const auto open = Uterus(-1.0f, 10.0f);
	std::printf("  245 particles in the uterus; after 10 s: %zu with the os closed (seep 0.5/s), %zu with it open\n", sealed, open);
	Check(sealed >= 235 && sealed <= 242, "behind a closed os it seeps out at about its seep rate");
	Check(open < sealed, "an open os lets it out faster");

	std::printf("thigh (2 s)\n");
	const auto semen = Thigh(Kind::kSemen, 2.0f);
	const auto secretion = Thigh(Kind::kSecretion, 2.0f);
	const auto skene = Thigh(Kind::kSkene, 2.0f);
	std::printf("  semen: ran %.3f m, %zu of 15 still on the skin\n", semen.first, semen.second);
	std::printf("  secretion: ran %.3f m, %zu still on\n", secretion.first, secretion.second);
	std::printf("  Skene's: ran %.3f m, %zu still on\n", skene.first, skene.second);
	Check(secretion.second >= skene.second, "secretion clings at least as well as Skene's fluid");
	Check(secretion.first <= semen.first + 1e-4f || secretion.second > semen.second, "secretion runs no faster than semen");

	std::printf("sources\n");
	{
		std::mt19937          random(7);
		auto                  canals = Canals(0.5f, 0.002f);
		const int             walls[] = { 0 };
		Secretion             lubrication;
		std::vector<Particle> out;
		for (int i = 0; i < 120; ++i) {
			lubrication.Step(kStep, 0.5f, 0.0f, canals, walls, random, out);
		}
		const auto calm = out.size();
		for (int i = 0; i < 120; ++i) {
			lubrication.Step(kStep, 1.0f, 1.0f, canals, walls, random, out);
		}
		std::printf("  secretion in 1 s: %zu unaroused, %zu at full wetness and arousal\n", calm, out.size() - calm);
		Check(calm == 0 && out.size() - calm >= 35 && out.size() - calm <= 41, "secretion follows wetness and arousal");
		bool inside = true;
		for (const auto& p : out) {
			inside = inside && p.canal == 0 && p.kind == static_cast<std::uint8_t>(Kind::kSecretion);
		}
		Check(inside, "it starts on the vagina's walls, in its canal");

		Urethra               urethra;
		std::vector<Particle> trickle, flood;
		urethra.Fill(1000.0f);  // plenty: this checks the jet, not the gland
		urethra.Squirt(0.05f, random);
		while (urethra.Active()) {
			urethra.Step(kStep, { 0, 0, 0 }, { 0, -1, -1 }, kSpacing, random, trickle);
		}
		urethra.Squirt(1.0f, random);
		while (urethra.Active()) {
			urethra.Step(kStep, { 0, 0, 0 }, { 0, -1, -1 }, kSpacing, random, flood);
		}
		float fastest = 0.0f;
		for (const auto& p : flood) {
			fastest = std::max(fastest, Length(p.velocity));
		}
		std::printf("  squirt: a trickle made %zu particles, a flood %zu (fastest %.2f m/s)\n", trickle.size(), flood.size(), fastest);
		Check(flood.size() > 4 * trickle.size() && fastest > 2.5f, "a flood is far bigger and faster than a trickle");
	}

	std::printf("opening (SoftBody's canal)\n");
	{
		// a standing woman: the vagina up into her, then the cervix, os and uterus
		OpeningShape shape;
		for (int i = 0; i < 6; ++i) {
			shape.rings.push_back({ { 0, 0, 0.012f * i }, 0.012f });
		}
		shape.cervix = { 0, 0, 0.075f };
		shape.os = { { 0, 0, 0.08f }, 0.004f };
		shape.uterusAxis = { 0, 0, 1 };
		shape.uterusLength = 0.05f;
		shape.uterusRadius = 0.02f;
		shape.uterusCentre = { 0, 0, 0.085f + 0.025f };
		auto built = BuildCanals(shape, 0.3f, 0.003f);
		Check(built.canals.size() == 8 && built.walls.size() == 6 && built.os == 6 && built.uterus == 7, "6 vaginal links, the os, the uterus");
		// a climax at the cervix with the os held open: semen shot up into the uterus
		Solver                                solver(kSpacing, Parameters{});
		Ground                                ground(0.5f);
		std::vector<Particle>                 particles;
		std::vector<Anchor>                   anchors;
		std::mt19937                          random(3);
		std::uniform_real_distribution<float> jitter(-0.002f, 0.002f);
		for (float t = 0; t < 0.5f; t += kStep) {
			for (int k = 0; k < 2; ++k) {
				Particle p;
				p.position = { jitter(random), jitter(random), 0.07f };
				p.velocity = { 0, 0, 1.2f };
				p.canal = static_cast<std::int16_t>(built.walls.back());
				particles.push_back(p);
			}
			Colliders colliders{ {}, {}, built.canals, nullptr };
			solver.Step(particles, anchors, colliders, ground, kStep);
		}
		// the shooting stops and the os closes: what got through stays, dripping back out at 0.3 a second (open, a
		// standing woman's uterus drains straight back down)
		shape.os.radius = 0.0015f;
		built = BuildCanals(shape, 0.3f, 0.003f);
		for (float t = 0; t < 1.0f; t += kStep) {
			Colliders colliders{ {}, {}, built.canals, nullptr };
			solver.Step(particles, anchors, colliders, ground, kStep);
		}
		const auto inside = Count(particles, built.uterus);
		std::printf("  %zu shot through an open os; %zu in the uterus 1 s after it closed\n", particles.size(), inside);
		for (float t = 0; t < 10.0f; t += kStep) {
			Colliders colliders{ {}, {}, built.canals, nullptr };
			solver.Step(particles, anchors, colliders, ground, kStep);
		}
		const auto  after = Count(particles, built.uterus);
		std::size_t out = 0;
		for (const auto& p : particles) {
			out += p.canal < 0 ? 1 : 0;
		}
		std::printf("  10 s after the os closed: %zu still in the uterus, %zu out of her\n", after, out);
		Check(inside >= 20, "semen shot through an open os is trapped once it closes");
		Check(after + 5 >= inside && after + 1 <= inside, "behind the closed os it drips out, about 3 in 10 s");
	}

	std::printf("squirt odds\n");
	{
		std::mt19937 random(11);
		SquirtOdds   unlucky(0.0001f);
		for (int i = 0; i < 10; ++i) {
			unlucky.Roll(random);
		}
		Check(unlucky.Chance() == 0.0f && unlucky.Misses() >= 5, "after five climaxes without a squirt, no more chance");
		SquirtOdds sure(1.0f);
		bool       always = true;
		for (int i = 0; i < 20; ++i) {
			always = always && sure.Roll(random);
		}
		Check(always && sure.Chance() == 1.0f, "a squirt keeps her chance");
		// many women at 50%: at which climax each first squirts
		int squirtedAt[7] = {};
		for (int w = 0; w < 20000; ++w) {
			SquirtOdds woman(0.5f);
			for (int c = 1; c <= 6; ++c) {
				if (woman.Roll(random)) {
					++squirtedAt[c];
					break;
				}
			}
		}
		std::printf("  20000 women at 50%%, first squirt at climax 1-6: %d %d %d %d %d %d\n", squirtedAt[1], squirtedAt[2],
			squirtedAt[3], squirtedAt[4], squirtedAt[5], squirtedAt[6]);
		// expected 10000, 4000, 1800, 840, 336, 0 (e.g. the 2nd: 0.5 x 0.4 x 20000)
		Check(squirtedAt[6] == 0 && squirtedAt[1] > 9700 && squirtedAt[1] < 10300 && squirtedAt[2] > 3800 && squirtedAt[2] < 4200,
			"50%, then 40, 30, 20, 10% after each miss, then none");
	}

	std::printf("squirt volume\n");
	{
		std::mt19937       random(5);
		std::vector<float> volumes;
		for (int i = 0; i < 10000; ++i) {
			volumes.push_back(Urethra::DrawVolume(random));
		}
		std::sort(volumes.begin(), volumes.end());
		std::printf("  10000 encounters: least %.0f ml, median %.0f, 90%% under %.0f, most %.0f\n", volumes.front(), volumes[5000],
			volumes[9000], volumes.back());
		Check(volumes.front() >= 10.0f && volumes.back() <= 150.0f && volumes[5000] > 36.0f && volumes[5000] < 44.0f,
			"a realistic volume per encounter: 10-150 ml, median 40");
		// a 20 ml gland, flooded again and again: it gives about 20 ml in all, then nothing
		Urethra urethra;
		urethra.Fill(20.0f);
		const float           each = kSpacing * kSpacing * kSpacing * 1.0e6f;
		std::vector<Particle> out;
		std::size_t           perSquirt[4]{};
		for (int s = 0; s < 4; ++s) {
			const auto before = out.size();
			urethra.Squirt(1.0f, random);
			while (urethra.Active()) {
				urethra.Step(kStep, { 0, 0, 0 }, { 0, -1, -1 }, kSpacing, random, out);
			}
			perSquirt[s] = out.size() - before;
		}
		std::printf("  a 20 ml gland, four floods: %zu, %zu, %zu, %zu particles (%.0f ml in all, %.1f left)\n", perSquirt[0], perSquirt[1],
			perSquirt[2], perSquirt[3], out.size() * each, urethra.Remaining());
		Check(out.size() * each <= 20.0f + each && out.size() * each > 19.0f, "it squirts what the gland holds, no more");
		Check(perSquirt[3] == 0, "an empty gland gives nothing");
	}

	// ---------------------------------------------------------------- realism inside the canals
	{
		// her canal along a_inward from an entrance at height 0.5 m (above the ground), with an os and uterus beyond
		auto shapeAlong = [](Vec3 a_inward) {
			OpeningShape shape;
			const Vec3   in = Normalize(a_inward, Vec3{ 0, 0, 1 });
			const Vec3   entrance{ 0, 0, 0.5f };
			for (int i = 0; i < 6; ++i) {
				shape.rings.push_back({ entrance + in * (0.012f * i), 0.012f });
			}
			shape.cervix = entrance + in * 0.075f;
			shape.os = { entrance + in * 0.08f, 0.0015f };
			shape.uterusAxis = in;
			shape.uterusLength = 0.05f;
			shape.uterusRadius = 0.02f;
			shape.uterusCentre = entrance + in * 0.11f;
			shape.analRings = { { entrance + Vec3{ 0, -0.04f, 0 }, 0.006f }, { entrance + Vec3{ 0, -0.04f, 0 } + in * 0.03f, 0.008f },
				{ entrance + Vec3{ 0, -0.04f, 0 } + in * 0.06f, 0.008f } };
			return shape;
		};
		auto fill = [](std::vector<Particle>& a_out, int a_canal, Vec3 a_at, int a_count, Kind a_kind) {
			for (int k = 0; k < a_count; ++k) {
				Particle p;
				p.position = a_at + Vec3{ ((k % 3) - 1) * kSpacing, (((k / 3) % 3) - 1) * kSpacing, (k / 9) * 0.5f * kSpacing };
				p.kind = static_cast<std::uint8_t>(a_kind);
				p.canal = static_cast<std::int16_t>(a_canal);
				a_out.push_back(p);
			}
		};
		auto outside = [](const std::vector<Particle>& a_particles) {
			std::size_t n = 0;
			for (const auto& p : a_particles) {
				n += p.canal < 0 ? 1 : 0;
			}
			return n;
		};
		auto run = [](Solver& a_solver, std::vector<Particle>& a_particles, const OpeningCanals& a_canals, float a_seconds,
					   std::span<const Capsule> a_capsules = {}) {
			Ground              ground(0.5f);
			std::vector<Anchor> anchors;
			Colliders           colliders{ a_capsules, {}, a_canals.canals, nullptr };
			for (float t = 0; t < a_seconds; t += kStep) {
				a_solver.Step(a_particles, anchors, colliders, ground, kStep);
			}
		};

		std::printf("liquefaction\n");
		{
			auto thigh = [&](float a_age) {
				Solver                solver(kSpacing, Parameters{});
				std::vector<Capsule>  capsules{ { { 0, 0, 0.5f }, { 0.02f, 0, 0.9f }, 0.07f, {}, {} } };
				std::vector<Particle> particles;
				for (int z = 0; z < 3; ++z) {
					for (int y = -2; y <= 2; ++y) {
						Particle p;
						p.position = { 0.07f + 0.6f * kSpacing + 0.0125f, y * kSpacing, 0.8f + z * kSpacing };
						p.touching = true;
						p.surface = 0;
						p.age = a_age;
						particles.push_back(p);
					}
				}
				OpeningCanals none;
				run(solver, particles, none, 2.0f, capsules);
				float lowest = 1e9f;
				for (const auto& p : particles) {
					lowest = std::min(lowest, p.position.z);
				}
				return 0.8f - lowest;
			};
			const float fresh = thigh(0.0f), liquid = thigh(1800.0f);
			std::printf("  down a thigh in 2 s: fresh semen %.3f m, after 30 min %.3f m\n", fresh, liquid);
			Check(liquid > 1.3f * fresh, "fresh semen is a gel that runs slower; it thins with age");
		}

		std::printf("mixing\n");
		{
			Solver                solver(kSpacing, Parameters{});
			std::vector<Particle> particles;
			auto                  built = BuildCanals(shapeAlong({ 0, 0.3f, -1 }), 0.3f, 0.003f);   // lying back: pools at the cervix
			fill(particles, built.walls.back(), shapeAlong({ 0, 0.3f, -1 }).cervix + Vec3{ 0, 0, 0.004f }, 27, Kind::kSemen);
			fill(particles, built.walls.back(), shapeAlong({ 0, 0.3f, -1 }).cervix + Vec3{ 0, 0, 0.010f }, 27, Kind::kSecretion);
			run(solver, particles, built, 5.0f);
			std::size_t blended = 0;
			for (const auto& p : particles) {
				const float m = SemenShare(p.kind, p.mix);
				blended += m > 0.1f && m < 0.9f ? 1 : 0;
			}
			std::printf("  27 semen and 27 secretion pooled together, 5 s: %zu now a cloudy blend\n", blended);
			Check(blended >= 10, "semen and secretion that touch blend");
		}

		std::printf("posture\n");
		std::size_t standingOut = 0, lyingOut = 0;
		for (int posture = 0; posture < 2; ++posture) {
			// standing: the vagina goes up into her; on her back: it runs back and a little down toward the cervix
			const Vec3            in = posture == 0 ? Vec3{ 0, 0.3f, 1 } : Vec3{ 0, 1, -0.25f };
			auto                  shape = shapeAlong(in);
			auto                  built = BuildCanals(shape, 0.3f, 0.003f);
			Solver                solver(kSpacing, Parameters{});
			std::vector<Particle> particles;
			fill(particles, built.walls.back(), shape.cervix - Normalize(in, {}) * 0.006f, 45, Kind::kSemen);
			for (auto& p : particles) {
				p.age = 1800.0f;  // liquefied
			}
			run(solver, particles, built, 8.0f);
			(posture == 0 ? standingOut : lyingOut) = outside(particles);
		}
		std::printf("  45 particles by the cervix, 8 s: %zu ran out of her standing, %zu lying on her back\n", standingOut, lyingOut);
		Check(standingOut > 20 && lyingOut <= 2, "it pools against the cervix while she lies back, and runs out when she stands");

		std::printf("uptake (her climax)\n");
		{
			std::size_t entered[2]{};
			for (int climax = 0; climax < 2; ++climax) {
				const Vec3            in{ 0, 1, -0.25f };  // on her back
				auto                  shape = shapeAlong(in);
				Solver                solver(kSpacing, Parameters{});
				std::vector<Particle> particles;
				auto                  built = BuildCanals(shape, 0.3f, 0.003f);
				fill(particles, built.walls.back(), shape.cervix - Normalize(in, {}) * 0.006f, 45, Kind::kSemen);
				Contractions contractions;
				std::mt19937 random(9);
				if (climax) {
					contractions.Start(1.0f, random);
				}
				Ground              ground(0.5f);
				std::vector<Anchor> anchors;
				for (float t = 0; t < 6.0f; t += kStep) {
					const float pulse = contractions.Step(kStep);
					built = BuildCanals(shape, 0.3f, 0.003f, OpeningMood{ pulse, 0.0f, 0.0f });
					Colliders colliders{ {}, {}, built.canals, nullptr };
					solver.Step(particles, anchors, colliders, ground, kStep);
				}
				entered[climax] = Count(particles, built.uterus);
			}
			std::printf("  semen waiting at a closed os, 6 s: %zu in the uterus without her climax, %zu with it\n", entered[0], entered[1]);
			Check(entered[0] == 0 && entered[1] >= 8, "her climax draws semen in through the os; a closed os lets none in otherwise");
		}

		std::printf("squeeze and movement\n");
		{
			std::size_t out[3]{};
			for (int mode = 0; mode < 3; ++mode) {
				const Vec3            in{ 0, 1, -0.25f };  // on her back: nothing runs out by itself
				auto                  shape = shapeAlong(in);
				Solver                solver(kSpacing, Parameters{});
				std::vector<Particle> particles;
				auto                  built = BuildCanals(shape, 0.3f, 0.003f);
				for (int link = 1; link < 4; ++link) {
					fill(particles, built.walls[static_cast<std::size_t>(link)], shape.rings[static_cast<std::size_t>(link)].centre, 9, Kind::kSecretion);
				}
				Contractions contractions;
				std::mt19937 random(4);
				if (mode == 1) {
					contractions.Start(1.0f, random);
				}
				Ground              ground(0.5f);
				std::vector<Anchor> anchors;
				for (float t = 0; t < 5.0f; t += kStep) {
					const float pulse = contractions.Step(kStep);
					// squeezing without the uptake (as the vagina's own contractions); moving: agitation 1
					built = BuildCanals(shape, 0.3f, 0.003f, OpeningMood{ 0.0f, pulse, mode == 2 ? 1.0f : 0.0f });
					Colliders colliders{ {}, {}, built.canals, nullptr };
					solver.Step(particles, anchors, colliders, ground, kStep);
				}
				out[mode] = outside(particles);
			}
			std::printf("  27 particles in her vagina, on her back, 5 s: %zu out still, %zu with contractions, %zu while she moves\n", out[0],
				out[1], out[2]);
			Check(out[1] > out[0] + 3 && out[2] > out[0] + 3, "contractions and movement push it out");
		}

		std::printf("shaft: plug, gush, coating and foam\n");
		{
			const Vec3            in{ 0, 0, 1 };  // standing: everything would run out
			auto                  shape = shapeAlong(in);
			auto                  built = BuildCanals(shape, 0.3f, 0.003f);
			Solver                solver(kSpacing, Parameters{});
			std::vector<Particle> particles;
			for (int link = 0; link < 5; ++link) {
				fill(particles, built.walls[static_cast<std::size_t>(link)], shape.rings[static_cast<std::size_t>(link)].centre + Vec3{ 0.008f, 0, 0 } +
					Vec3{ 0, 0, link == 0 ? 0.006f : 0.0f }, 9,
					Kind::kSemen);
			}
			for (auto& p : particles) {
				p.mix = 0.5f;  // half semen, half secretion: what thrusting whips to foam
				p.age = 1800.0f;
			}
			// his shaft fills the vagina: from below her to the cervix
			const Vec3            entrance = shape.rings[0].centre;
			Ground                ground(0.5f);
			std::vector<Anchor>   anchors;
			std::size_t           heldIn = 0, gushed = 0;
			float                 depth = 0.07f, lastDepth = depth;
			for (float t = 0; t < 4.0f; t += kStep) {
				// 2.5 s of strokes (4 a second, 2 cm deep), then he pulls out over 0.3 s
				if (t < 2.5f) {
					depth = 0.06f + 0.01f * std::sin(t * 2.0f * 3.14159f * 4.0f);
				} else {
					depth = std::max(-0.02f, 0.06f - (t - 2.5f) / 0.3f * 0.08f);
				}
				const float speed = (depth - lastDepth) / kStep;
				lastDepth = depth;
				Capsule shaft{ entrance - in * 0.15f, entrance + in * depth, 0.0105f, in * speed, in * speed, true };
				Capsule shafts[] = { shaft };
				Colliders colliders{ shafts, {}, built.canals, nullptr };
				solver.Step(particles, anchors, colliders, ground, kStep);
				if (t < 2.5f) {
					heldIn = particles.size() - outside(particles);
				}
			}
			gushed = outside(particles);
			std::size_t foam = 0, coated = 0;
			for (const auto& p : particles) {
				foam += p.kind == static_cast<std::uint8_t>(Kind::kFoam) ? 1 : 0;
				coated += p.whip > 0.0f ? 1 : 0;
			}
			std::printf("  45 particles of a half-and-half mix: %zu still in her while he thrusts, %zu out after he pulls out; %zu dragged by him, %zu foamed\n",
				heldIn, gushed, coated, foam);
			Check(heldIn >= 22, "his shaft plugs her: most stays in while he is in her, standing up");
			Check(gushed >= 22, "it gushes out when he pulls out");
			Check(coated >= 10 && foam >= 3, "his strokes drag it along, and at her entrance whip the mix to foam");
		}

		std::printf("absorption\n");
		{
			std::mt19937          random(2);
			auto                  built = BuildCanals(shapeAlong({ 0, 1, -0.25f }), 0.3f, 0.003f);
			std::vector<Particle> particles;
			fill(particles, built.walls[2], { 0, 0, 0 }, 200, Kind::kSemen);
			fill(particles, built.uterus, { 0, 0, 0 }, 200, Kind::kSemen);
			for (auto& p : particles) {
				p.age = 700.0f;
			}
			for (int s = 0; s < 600; ++s) {   // 10 minutes in 1 s steps, past a 10-minute grace, at 1/600 a second
				Absorb(particles, built, 1.0f, 600.0f, 1.0f / 600.0f, random);
			}
			std::printf("  200 in the vagina and 200 in the uterus, all past the grace, 10 min: %zu and %zu left\n", Count(particles, built.walls[2]),
				Count(particles, built.uterus));
			Check(Count(particles, built.walls[2]) < 100 && Count(particles, built.uterus) > Count(particles, built.walls[2]),
				"old fluid is absorbed, the uterus's slower");
		}

		std::printf("anal canal\n");
		{
			auto                  shape = shapeAlong({ 0, 0.3f, 1 });   // standing
			auto                  built = BuildCanals(shape, 0.3f, 0.003f);
			Check(built.anal.size() == 2 && built.canals[static_cast<std::size_t>(built.anal[0])].open, "its own chain, open at the anus");
			Solver                solver(kSpacing, Parameters{});
			std::vector<Particle> particles;
			fill(particles, built.anal.back(), shape.analRings.back().centre, 18, Kind::kSemen);
			for (auto& p : particles) {
				p.age = 1800.0f;
			}
			run(solver, particles, built, 6.0f);
			std::size_t inVagina = 0;
			for (const auto& p : particles) {
				inVagina += std::ranges::find(built.walls, p.canal) != built.walls.end() ? 1 : 0;
			}
			std::printf("  18 particles deep in her anal canal, standing, 6 s: %zu out, %zu crossed into the vagina\n", outside(particles), inVagina);
			Check(outside(particles) >= 9 && inVagina == 0, "it leaks out of the anus, and never into the vagina");
		}
	}

	std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
