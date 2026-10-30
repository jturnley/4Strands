# Integrating with 4Stim

Notes from reading the 4Stim 0.4.0 alpha build (plugin, Papyrus sources, INI). Not a spec: confirm against 4Stim's own headers and docs.

## C++ plugin interface

4Stim.dll carries an OStim-style plugin interface in the `FourStimAPI` namespace: `PluginInterface`, `InterfaceMap`, `ThreadInterface`, `Thread`, `ThreadActor`, `ThreadBuilder`, `AlignmentInterface` (with `AlignmentOffset`), `SexToyInterface`. It hands the interface map to other plugins over F4SE messaging ("Plugin interface: handed to ..."), as OStim does. The headers are not in the release archive, so 4Strands needs them from the 4Stim source.

This is what OStrands' `OStimBridge` talks to on Skyrim, so that bridge should port with renames once the headers are in `extern/4stim`.

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

4Stim targets 1.11.240 with Address Library, and 1.10.163, 1.10.984 and other 1.11 builds through the Runtime Database. 4Strands should build against the same CommonLibF4 fork 4Stim uses so the two match.
