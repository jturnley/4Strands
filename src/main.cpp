// Plugin entry point. This is a stub: the exact load/version API differs between CommonLibF4 forks,
// so check it against the fork chosen in CMakeLists.txt before the first build.

namespace
{
	void OnMessage(F4SE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			// TODO: connect to the OStim port's plugin interface and register the frame driver.
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

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	logger::info("4Strands loaded");

	auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::error("could not register the F4SE message listener");
		return false;
	}
	return true;
}
