# OCum Ascended parity

This is what 4Strands has to match, read from [Aietos/OCum-Ascended](https://github.com/Aietos/OCum-Ascended) at `526306a` (GPLv3, Papyrus). OCum is mostly scripts: NiOverride texture overlays, cum meshes equipped as OStim equip objects, and spell-based particle shots. 4Strands keeps the gameplay and replaces the visuals with procedural fluid.

| OCum Ascended | How it works there | 4Strands |
|---|---|---|
| Per-actor storage (men) | `GetCumStoredAmount`: starts full; regenerates at max storage per in-game day times `cumRegenSpeed`; separate max for the player and NPCs; kept in StorageUtil | Same rule, kept in the F4SE co-save. INI: player max, NPC max, regen speed |
| Load size on climax | `OstimOrgasm` (men only) stalls the climax, takes a random share of the stored amount, subtracts it, then permits the climax. Intensity 0-4 from the fraction of max (<1%, <10%, <20%, <35%, more) | Same math on 4Stim's climax listener. Intensity scales particle count, shot strength and rope count |
| Where the cum goes | OStim action custom records `cum` on the action's actor or target: belly, butt, chest, face, feet, hands, mouth, rectum, thighs, throat, vagina. Handjobs and footjobs fall back to a skeleton-distance guess | 4Stim's actions have **no `cum` records**, so 4Strands ships its own table (action type to receiving side and areas, with OStim's values as defaults) in `F4SE/Plugins/4Strands/`. Uses the "Scenes" interface for each scene's actions |
| Residue on the body | Overlay textures per area, stacking in stages (Breast1, 2, 3 and so on) in free overlay slots | Settled particles become skinned residue on the nearest bone, stacking per area with a cap, then fading |
| Creampie / analpie | A cum mesh equipped as an equip object (`ocumvagmesh`, `ocumanmesh`), each by chance (`creampieChance`, `analpieChance`), player scenes only | Fluid from `Vagina_00` / `Anus_00` after the pullout (OStrands' drips), with the same chance settings |
| Cum shot | `ocum_play_cum_shoot_effect`: 4-7 spurts from a `Urethra` node placed by an equip object, with the aim spread and later spurts lower. Skipped in vaginal, anal and oral scenes | OStrands' FluidManager shot from the `Penis_05` tip along the shaft. Still skipped while penetrating, which leaves internal fluid instead |
| Female squirting | On a woman's climax, by `squirtChance`: spurt or flow (spells), with sound and mod events | Fluid emitter at `Vagina_00`, in spurt or flow mode, with sound |
| Cleaning | Swimming (`SoundPlay.FSTSwimSwim`, toggle) and a cleanup timer in minutes (spell duration). MCM button to clean everyone | Water check plus timer; 4Stim's `wash*` events also clean their area; a Papyrus native to clean an actor or everyone |
| Cum bar | Hotkey shows the player's storage on an OStim bar for 5 seconds | A Papyrus native for the amount; the bar is optional (4Stim HUD API or a notification) |
| NPC scenes | `enableNPCSceneSupport`; shots only in the player's scene | The same toggle, plus a performance cap on active fluid |
| Mod events | `ocum_applied_cum` / `ocum_applied_cum_npc_scene` (orgasmer, target, amount, area, scene), `ocum_squirt`, `ocum_squirt_spurt`, `ocum_squirt_flow` | Papyrus custom events with the same fields, under 4Strands names |
| Settings | MCM: NPC support, disable shots, decals or meshes, vaginal/anal overlays, Elin facial skip, clean in water, bar key, chances, cleanup timer, regen speed, max amounts | INI, with an MCM for Fallout 4 front end. The Elin option is Skyrim-only and is dropped |

Not in OCum, despite my first plan: belly inflation. Leave it out unless the team wants it.
