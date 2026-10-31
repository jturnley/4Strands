#include "FourStimBridge.h"

#include "PluginInterface.h"
#include "Spike.h"

namespace FourStrands::FourStimBridge
{
	namespace
	{
		RE::Actor* ActorOf(FourStimAPI::ThreadActor* a_actor)
		{
			return a_actor ? static_cast<RE::Actor*>(a_actor->getGameActor()) : nullptr;
		}

		const char* NameOf(RE::Actor* a_actor)
		{
			const char* name = a_actor ? a_actor->GetDisplayFullName() : nullptr;
			return name ? name : "?";
		}

		FourStimAPI::SceneInterface* scenes{ nullptr };

		// Logs what each role does in the scene. Needs 4Stim's "Scenes" interface (patches/).
		void LogActions(const char* a_sceneID)
		{
			if (!scenes || !a_sceneID) {
				return;
			}
			const auto count = scenes->getActionCount(a_sceneID);
			for (std::uint32_t i = 0; i < count; ++i) {
				const char* type = scenes->getActionType(a_sceneID, i);
				REX::INFO("  action {}: {} by role {} on role {}{}", i, type, scenes->getActionRole(a_sceneID, i, 0), scenes->getActionRole(a_sceneID, i, 1),
					scenes->actionTypeHasTag(type, "sexual") ? " (sexual)" : "");
			}
		}

		// For now each listener only logs, so the Phase 0 spike can confirm the events arrive.
		struct ThreadStarted : FourStimAPI::ThreadEventListener
		{
			void listen(FourStimAPI::Thread* a_thread) override
			{
				REX::INFO("4Stim thread {} started in {} with {} actor(s)", a_thread->getThreadID(), a_thread->getCurrentSceneID(), a_thread->getActorCount());
				for (std::uint32_t i = 0; i < a_thread->getActorCount(); ++i) {
					Spike::QueueProbe(ActorOf(a_thread->getActor(i)));
				}
			}
		};

		struct SceneChanged : FourStimAPI::ThreadEventListener
		{
			void listen(FourStimAPI::Thread* a_thread) override
			{
				REX::INFO("4Stim thread {} now in {}", a_thread->getThreadID(), a_thread->getCurrentSceneID());
				LogActions(a_thread->getCurrentSceneID());
			}
		};

		struct SpeedChanged : FourStimAPI::ThreadEventListener
		{
			void listen(FourStimAPI::Thread* a_thread) override
			{
				REX::INFO("4Stim thread {} speed {}", a_thread->getThreadID(), a_thread->getSpeed());
			}
		};

		struct ThreadStopped : FourStimAPI::ThreadEventListener
		{
			void listen(FourStimAPI::Thread* a_thread) override
			{
				REX::INFO("4Stim thread {} stopped", a_thread->getThreadID());
			}
		};

		struct Climaxed : FourStimAPI::ThreadActorEventListener
		{
			void listen(FourStimAPI::Thread* a_thread, FourStimAPI::ThreadActor* a_actor) override
			{
				REX::INFO("4Stim thread {}: {} climaxed in {}", a_thread->getThreadID(), NameOf(ActorOf(a_actor)), a_thread->getCurrentSceneID());
			}
		};

		ThreadStarted started;
		SceneChanged  changed;
		SpeedChanged  speed;
		ThreadStopped stopped;
		Climaxed      climaxed;
	}

	void Connect()
	{
		FourStimAPI::InterfaceExchangeMessage message;
		auto* messaging = F4SE::GetMessagingInterface();
		if (messaging) {
			messaging->Dispatch(FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE, &message, sizeof(message), "4Stim");
		}
		if (!message.interfaceMap) {
			REX::INFO("4Stim not found; 4Strands follows no scenes");
			return;
		}
		auto* threads = static_cast<FourStimAPI::ThreadInterface*>(message.interfaceMap->queryInterface(FourStimAPI::ThreadInterface::NAME));
		if (!threads) {
			REX::WARN("4Stim has no thread interface; 4Strands follows no scenes");
			return;
		}
		threads->registerThreadStartListener(&started);
		threads->registerNodeChangedListener(&changed);
		threads->registerSpeedChangedListener(&speed);
		threads->registerThreadStopListener(&stopped);
		threads->registerClimaxListener(&climaxed);
		REX::INFO("4Stim thread interface version {}: following its scenes", threads->getVersion());

		scenes = static_cast<FourStimAPI::SceneInterface*>(message.interfaceMap->queryInterface(FourStimAPI::SceneInterface::NAME));
		if (!scenes) {
			REX::WARN("4Stim has no \"Scenes\" interface (older 4Stim): scene actions are unknown");
		}
	}
}
