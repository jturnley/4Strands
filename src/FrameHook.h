#pragma once

namespace FourStrands::FrameHook
{
	using Callback = void (*)(RE::PlayerCharacter* a_player, float a_delta);

	// Calls a_callback on the main thread after every PlayerCharacter::Update. Call once at plugin load.
	void Install(Callback a_callback);
}
