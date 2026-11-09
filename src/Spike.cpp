#include "Spike.h"

#include "BodyProbe.h"
#include "Fluids.h"
#include "FrameHook.h"
#include "Settings.h"
#include "SpikeStrand.h"

namespace FourStrands::Spike
{
	namespace
	{
		SpikeStrand strand;

		std::mutex                 queueLock;
		std::vector<RE::TESFormID> queued;
		std::atomic_bool           probePlayer{ false };

		// On the first frames after a load the skeleton's world transforms aren't updated yet (the first probe saw the
		// root near the origin), so the spike waits this long after the player's 3D appears.
		constexpr float kSettleSeconds = 2.0f;
		float           settled{ 0.0f };

		void OnFrame(RE::PlayerCharacter* a_player, float a_delta)
		{
			Fluids::Tick(a_delta);
			if (!a_player->Get3D()) {
				settled = 0.0f;
				return;
			}
			if (settled < kSettleSeconds) {
				settled += a_delta;
				return;
			}
			const auto& settings = Settings::Get();
			if (settings.probeBody) {
				if (probePlayer) {
					probePlayer = false;
					BodyProbe::Probe(a_player);
				}
				std::vector<RE::TESFormID> actors;
				{
					std::scoped_lock lock{ queueLock };
					actors.swap(queued);
				}
				for (const auto id : actors) {
					BodyProbe::Probe(RE::TESForm::GetFormByID<RE::Actor>(id));
				}
			}
			if (settings.strand) {
				strand.Update(a_player, a_delta);
			}
		}
	}

	void Install()
	{
		FrameHook::Install(OnFrame);
	}

	void OnGameLoaded()
	{
		settled = 0.0f;
		probePlayer = true;
	}

	void OnPreLoadGame()
	{
		strand.Reset();
	}

	void QueueProbe(RE::Actor* a_actor)
	{
		if (a_actor) {
			std::scoped_lock lock{ queueLock };
			queued.push_back(a_actor->GetFormID());
		}
	}
}
