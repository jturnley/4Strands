#pragma once

namespace FourStrands::FourStimBridge
{
	// Asks 4Stim for its interface map and registers the scene listeners. Call at F4SE's kPostLoad or later.
	void Connect();
}
