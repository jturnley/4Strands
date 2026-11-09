#include "Fluids.h"
#include "FourStimBridge.h"
#include "Settings.h"
#include "Spike.h"

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case F4SE::MessagingInterface::kPostLoad:
			FourStrands::FourStimBridge::Connect();
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
			// TODO: reset strands, fluids and per-actor state.
			FourStrands::Spike::OnPreLoadGame();
			FourStrands::Fluids::Reset();
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			FourStrands::Spike::OnGameLoaded();
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	REX::INFO("4Strands loaded on Fallout 4 {}", a_f4se->RuntimeVersion().string());

	FourStrands::Settings::Load();
	if (!FourStrands::Settings::Get().enabled) {
		REX::INFO("disabled in 4Strands.ini");
		return true;
	}

	auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		REX::ERROR("could not register the F4SE message listener");
		return false;
	}
	FourStrands::Spike::Install();
	return true;
}
