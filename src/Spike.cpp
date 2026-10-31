#include "Spike.h"

#include "BodyProbe.h"
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

		void OnFrame(RE::PlayerCharacter* a_player, float a_delta)
		{
			const auto& settings = Settings::Get();
			if (settings.probeBody) {
				if (probePlayer && a_player->Get3D()) {
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
