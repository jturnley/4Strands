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
| SOS `NPC Genitals01-06` | `Penis_00` to `Penis_05` (balls `Penis_Balls_01-02`); on A-Body prefer `Penis_01_SMP` to `Penis_05_SMP` |
| `VaginaDeep1` | `Vagina_00`, with lips `Vagina_L_01-02`, `Vagina_R_01-02` |
| `NPC Anus Deep1` | `Anus_00` to `Anus_04` |
| mouth (from `NPC Head`) | face skeleton: `skin_bone_C_MasterMouth`, `skin_bone_C_MouthTop/Bot`, `skin_bone_L/R_MouthCorner` |
| tongue | `Tongue_00` to `Tongue_04` |

ZeX also has `_CBP_` copies of the genital and breast chains for physics. The ScreenArcher variant adds `_Offset` nodes (for example `Anus_00_Offset`). 4Stim's penis bend node `Penis_01_Offset` is not in ZeX's basic skeleton.nif, so it probably comes from that variant or from A-Body's skeleton. Check which skeleton.nif the team installs.

## Body meshes

- TWB SMP's female body is a `BSSubIndexTriShape` with `BSSkin`, weighted to the vanilla `_skin` bones (`Pelvis_skin`, `Pelvis_Rear_skin`, `LBreast_skin`, `LButtFat_skin`, `Belly_skin`, and so on) and to its own SMP bones (`Breast_skin_SMP_TWB.L/R`, `ButtFat_skin_SMP.L/R`, `Belly_skin_SMP`). It has **no weights on ZeX's Vagina or Anus bones**, so orifices don't deform. 4Strands should place the vaginal and anal emitters from the ZeX nodes, plus the Alignment offsets OStrands already has, and not from mesh deformation.
- Skin sampling (OStrands' Body.cpp) has to read FO4's `BSSkin::Instance` / `BSSkin::BoneData` and the BSSubIndexTriShape vertex layout. Because TWB has SMP bones, the sampled skin follows the physics only if those bones are included in the skinning.
- A-Body (male) `MaleBody.nif` / `BaseMaleBodyFULL.nif` are `BSSubIndexTriShape` with `BSSkin`, weighted to `Penis_00`-`Penis_05` **and** their FSMP copies `Penis_01_SMP`-`Penis_05_SMP`, `Penis_Offset_SMP`, `Penis_Balls_01_SMP`, `Penis_Ball_SMP.L/R`. The extended skeleton nodes it references include the `_Offset` chain, among them `Penis_01_Offset` (4Stim's bend node). It is also weighted to `Anus_00`-`04` and `Anus_01-04_SMP`.
- Because the visible shaft follows the SMP bones while FSMP runs (including 4Stim's `MaleBodyHard.xml` swap), 4Strands should read the shaft chain from `Penis_0N_SMP` when that node exists and fall back to `Penis_0N`. The same applies to `Anus_0N_SMP` for male receivers.

## Does TWB have penetration geometry? No

Checked by decoding the vertex positions in TWB SMP's BodySlide mesh (22,685 vertices). This was a script, not a visual check in Outfit Studio. The crotch has dense surface detail: about 285 vertices in the 1-unit slice at the crotch floor, which matches its `Pussy Down` / `Pussy Pinch` sliders. But **no vertices go up into the body** from the vulva. Above the crotch floor, the only vertices between the front and back surfaces of the midline are the gluteal cleft, which rises along the back. So TWB has an external vulva and no vaginal or anal canal, and it has no weights on ZeX's `Vagina_` / `Anus_` bones. Penetration in animations clips into a closed surface.

## GT Softbody (Skyrim SE) genitals on TWB: findings

GT Softbody v3.37.2 is a **Skyrim SE** body, not a Fallout 4 one. Its CBBE body shape (`NakedBody/CBBE/.../GT_Softbody (CBBE).nif`) is a Skyrim `BSTriShape` with `NiSkinInstance`/`NiSkinPartition`. Its genitals are rigged to a dense 3BA-style set of bones that ZeX doesn't have:

- vagina: `VaginaB`, `VaginaB1`, `VaginaDeep`, `VaginaDeep1-2`, an opening ring `VaginaDB/DF/DL/DR 00-03`, lips `NPC L/R Pussy01-02`, `Clitoral`, `Clitoral1`
- anus: `NPC Anus00`, `NPC AnusR01`, a ring `NPC LT/LB/RT/RB Anus1-2`

Its genital BodySlide sliders are `LabiaTightUp`, `Labiapuffyness`, `LabiaMorePuffyness_v2`, `Labiaprotrude`, `Labiaprotrude2`, `Labiaprotrudeback`, `Labiaspread`, `LabiaCrumpled_v2`, `LabiaNeat_v2`, `LabiaBulgogi_v2`, `VaginaHole`, `Clit`, `ClitSwell_v2`, `AnalPosition_v2` and `AnalLoose_v2` (157 sliders in total).

Porting them onto TWB (FO4 CBBE topology) is a cross-game mesh port, not a slider copy:

1. **Geometry:** in Outfit Studio, cut Softbody's genital region, align it to TWB, replace TWB's crotch and weld the seam. The sliders' vertex data comes along with the region; conform the rest against TWB.
2. **Rig:** either add the opening rings to the FO4 skeleton (a ZeX extension that 4Stim, A-Body and animations would all need to accept), or reweight to ZeX's `Vagina_00`, `Vagina_L/R_01-02` and `Anus_00-04` (simpler, but loses the opening deformation). Working orifices for 4Strands need the first.
3. **Physics:** rewrite the SMP XML for FO4 Faster HDT-SMP with the new bones.
4. **Permissions:** a converted mesh is a derivative of GT Softbody. Check its Nexus permissions or ask its author before distributing.
