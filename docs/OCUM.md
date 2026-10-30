# OCum Ascended parity

This is what 4Strands has to match, read from [Aietos/OCum-Ascended](https://github.com/Aietos/OCum-Ascended) at `526306a` (GPLv3, Papyrus). OCum is mostly scripts: NiOverride texture overlays, cum meshes equipped as OStim equip objects, and spell-based particle shots. 4Strands keeps the gameplay and replaces the visuals with procedural fluid.

| OCum Ascended | How it works there | 4Strands |
|---|---|---|
| Per-actor storage (men) | `GetCumStoredAmount`: starts full; regenerates at max storage per in-game day times `cumRegenSpeed`; separate max for the player and NPCs; kept in StorageUtil | Same rule, kept in the F4SE co-save. INI: player max, NPC max, regen speed |
| Load size on climax | `OstimOrgasm` (men only) stalls the climax, takes a random share of the stored amount, subtracts it, then permits the climax. Intensity 0-4 from the fraction of max (<1%, <10%, <20%, <35%, more) | Same math on 4Stim's climax listener. Intensity scales particle count, shot strength and rope count |
| Where the cum goes | OStim action custom records `cum` on the action's actor or target: belly, butt, chest, face, feet, hands, mouth, rectum, thighs, throat, vagina. Handjobs and footjobs fall back to a skeleton-distance guess | **Decided 2026-10-30: no area table.** The fluid's path decides where it lands: shots fly from the shaft tip along its axis and collide with the body capsules and skin. Each settled particle's nearest bone gives the area (for events and stacking). 4Stim's actions, read through the "Scenes" interface, only decide whether a climax is internal or external and who the receiver is |
| Residue on the body | Overlay textures per area, stacking in stages (Breast1, 2, 3 and so on) in free overlay slots | **Stickiness decides what stays.** Contact with skin uses the solver's `grip` / `friction` / `cohesion`. Particles that stick settle and become skinned residue on the nearest bone, and the rest runs off and drips to the ground. Residue stacks per area up to a cap, then fades |
| Creampie / analpie | A cum mesh equipped as an equip object (`ocumvagmesh`, `ocumanmesh`), each by chance (`creampieChance`, `analpieChance`), player scenes only | **Non-fluid event.** No body has a working vagina or anus orifice yet, so particles inside the body would only cause collision problems. An internal climax subtracts the load, records a deposit on the receiver (orifice and amount) and fires the hooks below, with no particles. Fluid can come later if a body with real orifice geometry and collision appears |
| Cum shot | `ocum_play_cum_shoot_effect`: 4-7 spurts from a `Urethra` node placed by an equip object, with the aim spread and later spurts lower. Skipped in vaginal, anal and oral scenes | OStrands' FluidManager: 4-7 pulses from the `Penis_05(_SMP)` tip along the shaft axis, later pulses weaker. A penetrative climax (vaginal, anal, oral) is internal, so it's the non-fluid event above |
| Female squirting | On a woman's climax, by `squirtChance`: spurt or flow (spells), with sound and mod events | **Procedural too.** Same solver, with an emitter at `Vagina_00` aimed out of the body along the pelvis forward axis, which needs no orifice geometry. *Spurt* is a few high-speed pulses and *flow* is a steady low-speed stream. Its fluid preset has low viscosity and cohesion, low grip and a clear material, so most of it runs off and pools instead of sticking. OStrands' arousal wetness drips and strands carry over as well |
| Cleaning | Swimming (`SoundPlay.FSTSwimSwim`, toggle) and a cleanup timer in minutes (spell duration). MCM button to clean everyone | Water check plus timer; 4Stim's `wash*` events also clean their area; a Papyrus native to clean an actor or everyone |
| Cum bar | Hotkey shows the player's storage on an OStim bar for 5 seconds | A Papyrus native for the amount; the bar is optional (4Stim HUD API or a notification) |
| NPC scenes | `enableNPCSceneSupport`; shots only in the player's scene | The same toggle, plus a performance cap on active fluid |
| Mod events | `ocum_applied_cum` / `ocum_applied_cum_npc_scene` (orgasmer, target, amount, area, scene), `ocum_squirt`, `ocum_squirt_spurt`, `ocum_squirt_flow` | The hooks below, in Papyrus and C++ |
| Settings | MCM: NPC support, disable shots, decals or meshes, vaginal/anal overlays, Elin facial skip, clean in water, bar key, chances, cleanup timer, regen speed, max amounts | INI, with an MCM for Fallout 4 front end. The Elin option is Skyrim-only and is dropped |

## Hooks for other mods (pregnancy and others)

Added at the team's request (2026-10-30). There are two ways in, and both carry the same events. Papyrus: `FourStrands.RegisterForEvents(form)`, the same pattern as 4Stim's `RegisterForSceneEvents`. C++: an interface map handed out over F4SE messaging, the same pattern as 4Stim's.

- **OnInsemination**(donor, recipient, orifice `vaginal` / `anal` / `oral`, amountML, sceneID, threadID): every internal climax. This is the one pregnancy mods need.
- **OnCumLanded**(donor, recipient, area, amountML, sceneID): external fluid that stuck, once per area per climax, with the area taken from where it landed.
- **OnSquirt**(actor, mode `spurt` / `flow`, sceneID).
- **OnCleaned**(actor, cause `water` / `timer` / `wash` / `script`).
- Queries: stored amount and max for a donor, recent deposits on a recipient (orifice, amount, donor, game time), and residue per area. Setters: add or remove stored amount, clear deposits, clean an actor. A pregnancy mod can then roll its own chance from the deposit, or ignore 4Strands' amounts.

Deposits are kept in the co-save for a configurable number of in-game days, so a mod that loads late can still ask.

Not in OCum, despite my first plan: belly inflation. Leave it out unless the team wants it.
