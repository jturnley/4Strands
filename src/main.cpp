#include "FourStimBridge.h"

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case F4SE::MessagingInterface::kPostLoad:
			FourStrands::FourStimBridge::Connect();
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			// TODO: reset strands, fluids and per-actor state.
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

	auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		REX::ERROR("could not register the F4SE message listener");
		return false;
	}
	return true;
}
