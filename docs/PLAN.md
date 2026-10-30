# 4Strands plan

The full design and port plan is kept in the project doc "4Strands: design and port plan". Summary:

- **Phase 0, spike (about 1 week):** CommonLibF4 project for the target FO4 build; one FO4-format strand mesh driven by the plugin; confirm body vertex data can be read at runtime.
- **Phase 1, engine and body (1-2 weeks):** frame driver, menus and pause, logging, settings, the OStim port's bone table, capsule colliders.
- **Phase 2, OStim and strands (1 week):** port OStimBridge to the FO4 OStim interface; StrandManager in scenes.
- **Phase 3, climax fluids and storage (2-3 weeks):** fluid solver in game, shots and ropes, per-actor storage in the F4SE co-save, ground pooling.
- **Phase 4, OCum parity (3-5 weeks):** residue meshes, cleaning, inflation, female climax, MCM, Papyrus API, performance limits.

Open questions: does the OStim port keep the C++ PluginInterface and fire a climax event; which FO4 build; which body and skeleton mods.
