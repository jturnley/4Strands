# Integrating with 4Stim

Notes from reading the 4Stim 0.4.0 alpha build (plugin, Papyrus sources, INI). Not a spec: confirm against 4Stim's own headers and docs.

## C++ plugin interface

4Stim's API header is vendored at `extern/4stim/PluginInterface.h` (from KeryoWolfe/4Stim). 4Strands gets the interface map by dispatching `'4ST'` to "4Stim" at kPostLoad (`src/FourStimBridge.cpp`). The `Threads` interface gives thread start, scene change, speed, climax (per actor) and stop listeners, plus `Thread` / `ThreadActor` with excitement and `getGameActor()`. `Alignment` gives per-actor offsets.

What's missing compared with OStim: there is **no GraphInterface**. Scenes are passed by ID, and their actions (type, actor, target, tags) aren't available from C++. OStrands' `OStimBridge` classifies each pair from OStim action tags (`vaginalpenetration`, `fellatio`, ...). For 4Strands, either:

1. 4Stim adds a small C++ scene/action query to its API (preferred, since the same team owns both), or
2. 4Strands reads 4Stim's scene files (`F4SE/Plugins/4Stim/Scenes/*.json`) and action types (`Actions/*.json`) itself and looks up the current scene ID.

## Papyrus fallback

If the C++ interface does not cover something, `FourStim.RegisterForSceneEvents(form)` delivers to scripts on that form:

- `FourStim_OnSceneStart(Actor[] akActors, String asSceneID)`
- `FourStim_OnSceneChange(Actor[] akActors, String asOldID, String asNewID)`
- `FourStim_OnSpeedChange(Actor[] akActors, String asSceneID, int aiLevel, int aiCount)`
- `FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)`
- `FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)`
- `FourStim_OnEvent(String asEvent, Actor akActor, Actor akTarget, Actor akPerformer, Actor[] akActors, String asSceneID)`

Registrations are not saved: register again on every game load. Action types and tags are queryable per scene (`FourStim.GetSceneActionType`, `GetSceneActionRole`, `ActionHasTag`), with OStim's action and tag names. `wash*` events exist and can drive residue cleaning.

## Skeleton and bodies

- Male genitals: 4Stim bends the ZeX skeleton's `Penis_01_Offset` node (`sPenisBendNode` in 4Stim.ini); actors without it are treated as "not ZeX, or no penis". 4Strands' shaft chain should follow the same ZeX penis bones.
- A-Body (male) physics swaps to `MaleBodyHard.xml` / `MaleBodyHardBottom.xml` during scenes (FO4 Faster HDT-SMP).
- Female orifice anchors: still to be mapped for the chosen female body.

## Game versions

4Stim targets 1.11.240 with Address Library, and 1.10.163, 1.10.984 and other 1.11 builds through the Runtime Database. 4Strands uses the same libxse/commonlibf4 commit as 4Stim (submodule `lib/commonlibf4`).
