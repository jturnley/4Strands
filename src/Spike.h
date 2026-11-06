#pragma once

// Phase 0 spike: probes body vertex data and drives one strand. See docs/SPIKE.md.
namespace FourStrands::Spike
{
	// Installs the frame hook. Call at plugin load.
	void Install();

	// After a save loads or a new game starts: probe the player once their 3D is ready.
	void OnGameLoaded();

	// Before a save loads: detach the strand while the player's 3D still exists.
	void OnPreLoadGame();

	// Probes the actor on the next frame. Any thread.
	void QueueProbe(RE::Actor* a_actor);
}
