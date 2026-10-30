# Reference bodies and skeleton

The reference setup is CBBE with True Wasteland Body SMP (female), A-Body (male), and the ZeX skeleton, the same setup 4Stim uses. These notes come from reading the archives' NIF string tables and the SMP XML. Nothing here was checked in game.

## ZeX skeleton bones 4Strands needs

| OStrands (Skyrim) | ZeX / Fallout 4 |
|---|---|
| `NPC Pelvis [Pelv]` | `Pelvis` |
| `NPC Spine1/2` | `SPINE1`, `SPINE2` |
| `NPC Neck`, `NPC Head` | `Neck`, `HEAD` |
| thighs, calves, feet | `LLeg_Thigh`, `LLeg_Calf`, `LLeg_Foot` (and `RLeg_`) |
| upper arms, forearms, hands | `LArm_UpperArm`, `LArm_ForeArm1`, `LArm_Hand` (and `RArm_`) |
| butt | `LButtFat_skin`, `RButtFat_skin` (also `Butt_L_01`, `Butt_R_01`) |
| breasts | `LBreast_skin`, `RBreast_skin` (also `Breast_L_00-02`, `Breast_R_00-02`) |
| belly | `Belly_skin` (also `Belly_01`, `Belly_02` for inflation) |
| SOS `NPC Genitals01-06` | `Penis_00` to `Penis_05` (balls `Penis_Balls_01-02`) |
| `VaginaDeep1` | `Vagina_00`, with lips `Vagina_L_01-02`, `Vagina_R_01-02` |
| `NPC Anus Deep1` | `Anus_00` to `Anus_04` |
| mouth (from `NPC Head`) | face skeleton: `skin_bone_C_MasterMouth`, `skin_bone_C_MouthTop/Bot`, `skin_bone_L/R_MouthCorner` |
| tongue | `Tongue_00` to `Tongue_04` |

ZeX also has `_CBP_` copies of the genital and breast chains for physics. The ScreenArcher variant adds `_Offset` nodes (for example `Anus_00_Offset`). 4Stim's penis bend node `Penis_01_Offset` is not in ZeX's basic skeleton.nif, so it probably comes from that variant or from A-Body's skeleton. Check which skeleton.nif the team installs.

## Body meshes

- TWB SMP's female body is a `BSSubIndexTriShape` with `BSSkin`, weighted to the vanilla `_skin` bones (`Pelvis_skin`, `Pelvis_Rear_skin`, `LBreast_skin`, `LButtFat_skin`, `Belly_skin`, and so on) and to its own SMP bones (`Breast_skin_SMP_TWB.L/R`, `ButtFat_skin_SMP.L/R`, `Belly_skin_SMP`). It has **no weights on ZeX's Vagina or Anus bones**, so orifices don't deform. 4Strands should place the vaginal and anal emitters from the ZeX nodes, plus the Alignment offsets OStrands already has, and not from mesh deformation.
- Skin sampling (OStrands' Body.cpp) has to read FO4's `BSSkin::Instance` / `BSSkin::BoneData` and the BSSubIndexTriShape vertex layout. Because TWB has SMP bones, the sampled skin follows the physics only if those bones are included in the skinning.
- A-Body (male) archive: not inspected yet (1.2 GB). 4Stim's A-Body physics swap and penis bend imply A-Body is weighted to ZeX's `Penis_` chain.
