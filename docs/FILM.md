# The fluid film on her skin

This follows the method of [DripSim](https://github.com/antiarc9/DripSim) (by AntiArc, GPL-3, as 4Strands is). Fluid on
the skin lives in a thickness map over the skin's UV layout:
- it runs downhill along the live pose;
- it leaves tracks and a wet film that dries;
- it is drawn into a texture of the plugin's own, on an overlay of the skin.

The texture also carries a gloss map (wet skin shines) and a normal map.

## FO4

SoftBody FO4 (v0.46) builds the overlays into her body:
- `SoftBodyFilm`, a copy of CBBE;
- `SoftBodyFilmVulva`, a copy of the vulva patch, on the same CBBE UV layout.

Both have the same vertices, weights and sliders as the skin they copy, under `Materials\SoftBodyFO4\Film.bgsm`: an
alpha-blended decal with no depth write, over a transparent placeholder diffuse.

`src/Film.cpp` makes 4Strands' film texture work:
- **The texture:** 4Strands makes a 2048 × 2048 RGBA texture.
- **The swap:** it finds her overlays, then their material's diffuse `NiTexture`, then its renderer texture. There it
  puts our shader resource view in place of the placeholder's. FO4's `BSGraphics::Texture` isn't laid out in
  CommonLibF4, so the field is found by its vtable, since every view the device makes shares ours. The log names the
  offset found.
- **Uploads:** changed rows are uploaded on the render thread, from a hook on the swap chain's `Present`.

**Limit:** FO4 shares one texture object between everyone with the same material path. While the film runs, another
woman wearing this body shows it too. Stage 2 should give each woman her own placeholder path, or her own material.

## Stages

1. **Prove the drawing (now).** `[Film] TestPattern = true` draws a moving grid on her skin while a scene runs. The
   log's `film:` lines show the texture made, the view's offset, and `drawing on SoftBodyFilm`.
2. **The film itself:**
   - the gravity map from her live skin and bones (DripSim's `Field`);
   - the thickness map with beads, runners, tracks and drying (DripSim's `Film`, `SoakFlow`);
   - a look for each kind:
     - **secretion:** a clear, glossy film;
     - **semen:** cream, clear when thin and opaque when thick;
     - **foam:** opaque white;
     - **Skene's fluid:** a short wet shine;
     - **urine:** a yellow tint.
3. **The hand-off:**
   - solver particles that touch her skin become film where they land;
   - film that runs off an edge or an overhang becomes particles again, and falls.
