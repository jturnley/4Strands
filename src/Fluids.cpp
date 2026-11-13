#include "Fluids.h"

#include "Film.h"

#include "PluginInterface.h"
#include "Settings.h"
#include "SoftBodyBridge.h"
#include "fluid/Opening.h"
#include "fluid/Solver.h"
#include "fluid/Sources.h"

namespace FourStrands::Fluids
{
	namespace
	{
		using Fluid::Vec3;

		constexpr float kSpacing = 0.004f;        // metres between particles
		constexpr float kStep = 1.0f / 120.0f;    // the solver's step
		constexpr float kOsOpenThreshold = 0.003f;  // metres: an os at least this open lets fluid through freely
		constexpr float kOsOpenGame = 0.35f;      // game units: how far his climax inside her opens the os
		constexpr float kUterusSeep = 0.3f;       // particles a second out of a closed os
		constexpr float kAbsorbGrace = 600.0f;    // seconds before fluid in her starts being absorbed
		constexpr float kAbsorbRate = 1.0f / 600.0f;
		constexpr float kTipBeyond = 0.75f;       // game units: his tip past Penis_05_SMP (A-Body's)
		constexpr float kSemenMl = 3.5f;          // a typical ejaculate
		constexpr float kShotSpeedOut = 2.5f;     // m/s from his tip in the air; inside her, slower
		constexpr float kShotSpeedIn = 0.9f;

		std::mutex                 eventsLock;
		std::vector<std::pair<int, RE::TESFormID>> climaxes;
		int                        startedThread{ -1 };
		bool                       stopped{ false };

		// per woman, from her form ID: the same woman is the same every encounter
		float Hash01(std::uint32_t a_id, std::uint32_t a_salt)
		{
			std::uint32_t x = a_id * 2654435761u ^ (a_salt * 40503u);
			x ^= x >> 15;
			x *= 2246822519u;
			x ^= x >> 13;
			return static_cast<float>(x & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
		}

		struct Spurts
		{
			int   left{};
			float timer{};
			float perSpurt{};
			bool  active() const { return left > 0; }
		};

		struct Session
		{
			int                         thread{ -1 };
			std::uint32_t               her{};
			RE::TESFormID               him{};
			SoftBodyBridge::Frame       frame;
			bool                        framed{ false };
			Fluid::Solver               solver{ kSpacing, Fluid::Parameters{} };
			Fluid::Ground               ground{ 0.25f };
			std::vector<Fluid::Particle> particles;
			std::vector<Fluid::Anchor>  anchors;
			Fluid::Secretion            secretion;
			Fluid::Urethra              urethra;
			Fluid::SquirtOdds           odds;
			Fluid::Contractions         contractions;
			float                       wetness{ 0.5f };
			std::mt19937                random{ std::random_device{}() };
			float                       lag{};
			float                       logTimer{};
			float                       absorbTimer{};
			float                       groundTimer{};
			float                       filmTimer{};
			float                       osOpenFor{};
			Vec3                        lastEntrance[2];
			int                         entranceSeen{};
			float                       agitation{};
			Vec3                        lastTip, lastBase;
			bool                        hadShaft{ false };
			Spurts                      spurts;
			int                         spurtCanal{ -1 };
			float                       squeeze{};
		};

		std::optional<Session> session;

		RE::Actor* ActorOf(FourStimAPI::ThreadActor* a_actor)
		{
			return a_actor ? static_cast<RE::Actor*>(a_actor->getGameActor()) : nullptr;
		}

		FourStimAPI::ThreadInterface* threads{ nullptr };

		RE::NiAVObject* Node(RE::Actor* a_actor, const char* a_name)
		{
			auto* root = a_actor ? a_actor->Get3D() : nullptr;
			return root ? root->GetObjectByName(RE::BSFixedString(a_name)) : nullptr;
		}

		Vec3 ToSolver(const SoftBodyBridge::Frame& a_frame, const RE::NiPoint3& a_point)
		{
			return (Vec3{ a_point.x, a_point.y, a_point.z } - a_frame.origin) * a_frame.scale;
		}

		// Her excitement now (0-1), from 4Stim
		float Arousal(const Session& a_session)
		{
			auto* thread = threads ? threads->getThread(a_session.thread) : nullptr;
			if (!thread) {
				return 0.0f;
			}
			for (std::uint32_t i = 0; i < thread->getActorCount(); ++i) {
				auto* actor = thread->getActor(i);
				if (auto* game = ActorOf(actor); game && game->GetFormID() == a_session.her) {
					return std::clamp(actor->getExcitement() / 100.0f, 0.0f, 1.0f);
				}
			}
			return 0.0f;
		}

		// Him: whoever in the thread has A-Body's penis
		RE::Actor* Him(const Session& a_session)
		{
			auto* thread = threads ? threads->getThread(a_session.thread) : nullptr;
			for (std::uint32_t i = 0; thread && i < thread->getActorCount(); ++i) {
				auto* game = ActorOf(thread->getActor(i));
				if (game && game->GetFormID() != a_session.her && Node(game, "Penis_05_SMP")) {
					return game;
				}
			}
			return nullptr;
		}

		// The canal his tip is in now (a vaginal or anal link), or -1
		int CanalAt(const Fluid::OpeningCanals& a_canals, Vec3 a_point)
		{
			for (std::size_t k = 0; k < a_canals.canals.size(); ++k) {
				if (static_cast<int>(k) == a_canals.os || static_cast<int>(k) == a_canals.uterus) {
					continue;
				}
				const auto& canal = a_canals.canals[k];
				const Vec3  span = canal.b - canal.a;
				const float length2 = Fluid::LengthSquared(span);
				if (length2 < 1e-10f) {
					continue;
				}
				const float t = Fluid::Dot(a_point - canal.a, span) / length2;
				const Vec3  radial = a_point - (canal.a + span * t);
				const float radius = canal.radiusA + (canal.radiusB - canal.radiusA) * std::clamp(t, 0.0f, 1.0f);
				if (t >= 0.0f && t <= 1.0f && Fluid::Length(radial) < 1.5f * radius + kSpacing) {
					return static_cast<int>(k);
				}
			}
			return -1;
		}

		void Start(int a_thread)
		{
			session.emplace();
			session->thread = a_thread;
			REX::INFO("fluids: following 4Stim thread {}", a_thread);
		}

		// The woman is known once SoftBody reports her: her traits for this encounter
		void Meet(Session& a_session, std::uint32_t a_her)
		{
			const auto& settings = Settings::Get();
			a_session.her = a_her;
			a_session.wetness = std::clamp((0.25f + 0.75f * Hash01(a_her, 1)) * settings.fluidWetness, 0.0f, 1.0f);
			a_session.odds = Fluid::SquirtOdds(std::clamp(settings.fluidSquirtChance * (0.4f + 1.2f * Hash01(a_her, 2)), 0.0f, 1.0f));
			a_session.urethra.Fill(Fluid::Urethra::DrawVolume(a_session.random));
			REX::INFO("fluids: {:08X}: wetness {:.2f}, squirt chance {:.0f}%, {:.0f} ml to squirt this encounter", a_her, a_session.wetness,
				100.0f * a_session.odds.Chance(), a_session.urethra.Remaining());
		}

		void HerClimax(Session& a_session)
		{
			std::uniform_real_distribution<float> unit(0.0f, 1.0f);
			a_session.contractions.Start(0.7f + 0.3f * unit(a_session.random), a_session.random);
			const bool squirt = a_session.odds.Roll(a_session.random);
			if (squirt && a_session.urethra.Remaining() > 0.0f) {
				a_session.urethra.Squirt(0.2f + 0.8f * unit(a_session.random), a_session.random);
			}
			REX::INFO("fluids: her climax: contractions; {} (chance now {:.0f}%, {:.0f} ml left)", squirt ? "she squirts" : "no squirt",
				100.0f * a_session.odds.Chance(), a_session.urethra.Remaining());
		}

		void HisClimax(Session& a_session)
		{
			std::uniform_int_distribution<int> count(4, 7);
			a_session.spurts.left = count(a_session.random);
			a_session.spurts.timer = 0.0f;
			const float each = kSpacing * kSpacing * kSpacing * 1.0e6f;
			a_session.spurts.perSpurt = kSemenMl / each / static_cast<float>(a_session.spurts.left);
			REX::INFO("fluids: his climax: {} spurts", a_session.spurts.left);
		}

		void SetGround(Session& a_session, RE::Actor* a_her)
		{
			if (!a_her) {
				return;
			}
			const float floor = (a_her->GetPositionZ() - a_session.frame.origin.z) * a_session.frame.scale;
			auto&       ground = a_session.ground;
			const int   reach = static_cast<int>(2.0f / ground.Cell());
			for (int y = -reach; y <= reach; ++y) {
				for (int x = -reach; x <= reach; ++x) {
					ground.Set(x, y, floor);
				}
			}
		}

		void Step(Session& a_session, float a_seconds)
		{
			auto woman = SoftBodyBridge::Read(a_session.frame);
			if (!a_session.framed) {
				// the frame: her vaginal entrance, as SoftBody first reports it
				SoftBodyBridge::Frame origin{ {}, 1.0f };  // game units
				auto                  probe = SoftBodyBridge::Read(origin);
				if (!probe || probe->shape.rings.empty()) {
					return;
				}
				a_session.frame.origin = probe->shape.rings[0].centre;
				a_session.framed = true;
				woman = SoftBodyBridge::Read(a_session.frame);
			}
			if (!woman) {
				return;
			}
			if (a_session.her != woman->formID) {
				Meet(a_session, woman->formID);
			}
			auto* her = RE::TESForm::GetFormByID<RE::Actor>(a_session.her);
			// the film on her skin (stage 1: a test pattern)
			if (Settings::Get().film && her) {
				if ((a_session.filmTimer -= a_seconds) <= 0.0f) {
					a_session.filmTimer = 1.0f;
					Film::Attach(her->Get3D());
				}
				if (Settings::Get().filmTestPattern) {
					Film::TestPattern(a_seconds);
				}
			}
			if ((a_session.groundTimer -= a_seconds) <= 0.0f) {
				a_session.groundTimer = 1.0f;
				SetGround(a_session, her);
			}

			// her movement: the entrance's acceleration
			const Vec3 entrance = woman->shape.rings.empty() ? Vec3{} : woman->shape.rings[0].centre;
			if (a_session.entranceSeen >= 2) {
				const Vec3  accel = (entrance - a_session.lastEntrance[0] * 2.0f + a_session.lastEntrance[1]) * (1.0f / (a_seconds * a_seconds));
				const float now = std::clamp(Fluid::Length(accel) / 20.0f, 0.0f, 1.0f);
				a_session.agitation += (now - a_session.agitation) * std::min(1.0f, a_seconds * 3.0f);
			}
			a_session.lastEntrance[1] = a_session.lastEntrance[0];
			a_session.lastEntrance[0] = entrance;
			a_session.entranceSeen = std::min(a_session.entranceSeen + 1, 2);

			// her climax's contractions: the womb draws in, the vagina squeezes (SoftBody tightens its rings too)
			const float pulse = a_session.contractions.Step(a_seconds);
			if (std::abs(pulse - a_session.squeeze) > 0.02f || (pulse == 0.0f && a_session.squeeze != 0.0f)) {
				a_session.squeeze = pulse;
				SoftBodyBridge::SetCanalSqueeze(a_session.her, pulse);
			}
			Fluid::OpeningMood mood{ pulse, pulse, a_session.agitation };
			auto               canals = Fluid::BuildCanals(woman->shape, kUterusSeep, kOsOpenThreshold, mood);

			// his shaft: Penis_01_SMP to past Penis_05_SMP
			std::vector<Fluid::Capsule> capsules;
			auto*                       him = Him(a_session);
			auto*                       base = Node(him, "Penis_01_SMP");
			auto*                       tipBone = Node(him, "Penis_05_SMP");
			Vec3                        tip{}, along{ 0.0f, 0.0f, 1.0f };
			if (base && tipBone) {
				const Vec3 b = ToSolver(a_session.frame, base->world.translate);
				const Vec3 t5 = ToSolver(a_session.frame, tipBone->world.translate);
				along = Fluid::Normalize(t5 - b, along);
				tip = t5 + along * (kTipBeyond * a_session.frame.scale);
				Fluid::Capsule shaft{ b, tip, Settings::Get().fluidPenisRadius * a_session.frame.scale, {}, {}, true };
				if (a_session.hadShaft) {
					shaft.velocityA = (b - a_session.lastBase) * (1.0f / a_seconds);
					shaft.velocityB = (tip - a_session.lastTip) * (1.0f / a_seconds);
				}
				a_session.lastBase = b;
				a_session.lastTip = tip;
				a_session.hadShaft = true;
				capsules.push_back(shaft);
			} else {
				a_session.hadShaft = false;
			}

			// sources: her secretion, her urethra, his spurts
			std::vector<Fluid::Particle> born;
			a_session.secretion.Step(a_seconds, a_session.wetness, Arousal(a_session), canals.canals, canals.walls, a_session.random, born);
			if (woman->urethra) {
				a_session.urethra.Step(a_seconds, *woman->urethra, woman->urethraOut, kSpacing, a_session.random, born);
			}
			if (a_session.spurts.active() && base && tipBone) {
				a_session.spurts.timer -= a_seconds;
				if (a_session.spurts.timer <= 0.0f) {
					a_session.spurts.timer = 0.6f;
					const int  canal = CanalAt(canals, tip);
					const bool inside = canal >= 0;
					if (inside && std::ranges::find(canals.walls, canal) != canals.walls.end()) {
						a_session.osOpenFor = 2.0f;  // the os opens while he spurts into her
					}
					std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
					const int count = static_cast<int>(a_session.spurts.perSpurt * (0.6f + 0.15f * a_session.spurts.left));
					for (int k = 0; k < count; ++k) {
						Fluid::Particle p;
						p.position = tip + Vec3{ jitter(a_session.random), jitter(a_session.random), jitter(a_session.random) } * (0.4f * kSpacing);
						p.velocity = along * ((inside ? kShotSpeedIn : kShotSpeedOut) * (0.8f + 0.2f * jitter(a_session.random)));
						p.kind = static_cast<std::uint8_t>(Fluid::Kind::kSemen);
						p.canal = static_cast<std::int16_t>(canal);
						born.push_back(p);
					}
					--a_session.spurts.left;
				}
			}
			const auto cap = static_cast<std::size_t>(Settings::Get().fluidMaxParticles);
			for (auto& p : born) {
				if (a_session.particles.size() >= cap) {
					break;
				}
				a_session.particles.push_back(p);
			}

			// the os: open while he spurts into her
			if (a_session.osOpenFor > 0.0f) {
				a_session.osOpenFor -= a_seconds;
				SoftBodyBridge::SetOsOpen(a_session.her, a_session.osOpenFor > 0.0f ? kOsOpenGame : 0.0f);
			}

			// the step, in fixed substeps
			a_session.lag = std::min(a_session.lag + a_seconds, 4.0f * kStep);
			Fluid::Colliders colliders{ capsules, {}, canals.canals, nullptr };
			while (a_session.lag >= kStep) {
				a_session.lag -= kStep;
				try {
					a_session.solver.Step(a_session.particles, a_session.anchors, colliders, a_session.ground, kStep);
				} catch (const std::exception& e) {
					REX::WARN("fluids: {}; starting over", e.what());
					a_session.particles.clear();
					break;
				}
			}

			// absorption, and the uterus's fill back to SoftBody
			if ((a_session.absorbTimer += a_seconds) >= 1.0f) {
				Fluid::Absorb(a_session.particles, canals, a_session.absorbTimer, kAbsorbGrace, kAbsorbRate, a_session.random);
				a_session.absorbTimer = 0.0f;
				const float uterusVolume = 3.14159f * woman->shape.uterusRadius * woman->shape.uterusRadius * woman->shape.uterusLength;
				const float capacity = std::max(1.0f, uterusVolume / (kSpacing * kSpacing * kSpacing));
				std::size_t inUterus = 0;
				for (const auto& p : a_session.particles) {
					inUterus += p.canal == canals.uterus ? 1 : 0;
				}
				SoftBodyBridge::SetUterusFill(a_session.her, std::min(1.0f, static_cast<float>(inUterus) / capacity));
			}

			// what it is doing (nothing is drawn yet)
			if ((a_session.logTimer += a_seconds) >= 2.0f) {
				a_session.logTimer = 0.0f;
				std::size_t vagina = 0, uterus = 0, os = 0, anal = 0, outside = 0, foam = 0, kinds[5]{};
				for (const auto& p : a_session.particles) {
					if (p.canal < 0) {
						++outside;
					} else if (p.canal == canals.uterus) {
						++uterus;
					} else if (p.canal == canals.os) {
						++os;
					} else if (std::ranges::find(canals.anal, p.canal) != canals.anal.end()) {
						++anal;
					} else {
						++vagina;
					}
					foam += p.kind == static_cast<std::uint8_t>(Fluid::Kind::kFoam) ? 1 : 0;
					if (p.kind < 5) {
						++kinds[p.kind];
					}
				}
				REX::INFO("fluids: {} particles: vagina {}, os {}, uterus {}, anal {}, outside {} | semen {}, secretion {}, Skene's {}, "
				          "urine {}, foam {} | arousal {:.2f}, agitation {:.2f}, squeeze {:.2f}, shaft {}, plugged {}",
					a_session.particles.size(), vagina, os, uterus, anal, outside, kinds[0], kinds[1], kinds[2], kinds[3], foam, Arousal(a_session),
					a_session.agitation, pulse, capsules.empty() ? "no" : "yes",
					!capsules.empty() && CanalAt(canals, tip) >= 0 ? "in her" : "no");
			}
		}

		void End()
		{
			Film::Detach();
			if (session && session->her) {
				SoftBodyBridge::SetCanalSqueeze(session->her, 0.0f);
				SoftBodyBridge::SetOsOpen(session->her, 0.0f);
			}
			session.reset();
		}
	}

	void OnThreadStarted(int a_threadID)
	{
		std::scoped_lock lock{ eventsLock };
		startedThread = a_threadID;
	}

	void OnThreadStopped(int a_threadID)
	{
		std::scoped_lock lock{ eventsLock };
		if (a_threadID == startedThread || (session && session->thread == a_threadID)) {
			stopped = true;
		}
	}

	void OnClimax(int a_threadID, RE::Actor* a_actor)
	{
		if (a_actor) {
			std::scoped_lock lock{ eventsLock };
			climaxes.emplace_back(a_threadID, a_actor->GetFormID());
		}
	}

	void Tick(float a_seconds)
	{
		const auto& settings = Settings::Get();
		if (!settings.fluids || a_seconds <= 0.0f) {
			return;
		}
		std::vector<std::pair<int, RE::TESFormID>> events;
		int                                        start = -1;
		bool                                       stop = false;
		{
			std::scoped_lock lock{ eventsLock };
			events.swap(climaxes);
			start = startedThread;
			startedThread = -1;
			stop = stopped;
			stopped = false;
		}
		if (!threads && (start >= 0 || session)) {
			FourStimAPI::InterfaceExchangeMessage message;
			if (auto* messaging = F4SE::GetMessagingInterface()) {
				messaging->Dispatch(FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE, &message, sizeof(message), "4Stim");
			}
			if (message.interfaceMap) {
				threads = static_cast<FourStimAPI::ThreadInterface*>(message.interfaceMap->queryInterface(FourStimAPI::ThreadInterface::NAME));
			}
		}

		if (start >= 0 || session) {
			SoftBodyBridge::Connect();
		}
		if (stop) {
			REX::INFO("fluids: the scene ended");
			End();
		}
		if (start >= 0) {
			End();
			Start(start);
		}
		if (!session) {
			return;
		}
		for (const auto& [thread, actor] : events) {
			if (thread != session->thread) {
				continue;
			}
			if (session->her && actor == session->her) {
				HerClimax(*session);
			} else if (Node(RE::TESForm::GetFormByID<RE::Actor>(actor), "Penis_05_SMP")) {
				HisClimax(*session);
			}
		}
		Step(*session, std::min(a_seconds, 0.1f));
	}

	void Reset()
	{
		std::scoped_lock lock{ eventsLock };
		climaxes.clear();
		startedThread = -1;
		stopped = false;
		session.reset();
		Film::Detach();
	}
}
