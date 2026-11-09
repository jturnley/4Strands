#pragma once

namespace FourStrands::Fluids
{
	// The fluid of the 4Stim scene 4Strands follows: her secretion, squirts and contractions, his climax into her or out,
	// all held in SoftBody FO4's canals. Nothing is drawn yet (see docs/SPIKE.md): it logs what the fluid does

	// 4Stim's events, from any thread: queued for the next frame
	void OnThreadStarted(int a_threadID);
	void OnThreadStopped(int a_threadID);
	void OnClimax(int a_threadID, RE::Actor* a_actor);

	// Every frame, on the main thread (the frame hook)
	void Tick(float a_seconds);

	// A save is about to load: drop everything
	void Reset();
}
