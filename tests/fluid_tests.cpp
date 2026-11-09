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

	std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
