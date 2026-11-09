# Fluids: kinds, sources and SoftBody FO4

## The fluids

Each particle has a kind (`src/fluid/Kinds.h`). A kind scales the solver's `Parameters`, with semen as the reference
at 1 everywhere.

| | cohesion | viscosity | friction | grip | hangs under | wets skin | colour |
|---|---|---|---|---|---|---|---|
| Semen | 1 | 1 | 1 | 1 | surfaces facing down to −0.5 | lightly (fades over 30 s) | off-white |
| Vaginal secretion | 1.6 | 0.6 | 3 | 2.5 | nearly anything (−0.9) | strongly (fades over 90 s) | clear |
| Skene's fluid | 0.05 | 0.08 | 0.05 | 0 | nothing (0.2) | barely | clear |
| Urine | as Skene's | | | | | | light yellow |

What the kinds do:

- **Semen** is as tuned before.
- **Vaginal secretion is slick and sticks to skin.** It clings and holds under surfaces, so it gathers into a large drop
  before the drop's weight tears it off. It runs slowly down whatever it lands on: about a third as fast as semen down a
  thigh in the tests. It leaves skin wet, and wet skin shines.
- **Skene's fluid and urine are water.** They have almost no cohesion, viscosity or grip, so they stick to neither skin
  nor themselves, and they fall off.

How the solver uses the kinds:

- **Cohesion:** a pair of particles holds together as the less cohesive of its two kinds does.
- **Viscosity, friction and grip:** each particle uses its own kind's.
- **Drips:** a kind's `hang` decides how far a surface may face down before it lets go.

## Sources (`src/fluid/Sources.h`)

### Secretion

Secretion is made all along the vagina's walls while she is aroused. The rate is
40 per second × wetness × arousal^1.5:

- **Wetness** is the woman, from 0 dry to 1 very wet. It will be per woman, from a hash of her form ID and the INI.
- **Arousal** is 4Stim's excitement ÷ 100.

At low levels it is a coating that clings to the walls. At high levels it runs out and drips. New particles start on
the wall, inside their canal.

### Urethra

The urethra makes two kinds of jet:

- **`Squirt(strength)`** is Skene's fluid, from a trickle (0) to a flood (1). A trickle is one long weak pulse, about 30
  particles. A flood is 3 to 5 fading pulses, up to 500 particles a second at 3.5 m/s.
- **`Urinate(seconds)`** is one steady stream.

**Whether she squirts (`SquirtOdds`):** her climax only has a chance of a squirt. Each woman has her own chance.
Every climax without a squirt takes a fifth of that chance away: a 50% woman goes 50%, 40, 30, 20, 10%, then 0. So
after five climaxes without one, she won't squirt in that scene. A squirt leaves her chance where it is.

### Wetness

`Wetness` keeps how wet each skin patch is, from 0 to 1. Fluid touching a patch wets it at its kind's rate, and the
patch dries over its kind's fade time. It is meant for drawing wet skin glossier.

## Canals (`Solver.h` `Canal`, `src/fluid/Opening.h`)

A canal is a chain of tapered capsules. A particle inside one is held within its radius. At the canal's ends it passes
to the next or previous canal, leaves through an open entrance, or is held at a closed end. Free particles that cross
an open entrance join it. Inside a canal, her body's colliders are skipped: the canal holds the particle. `drag` is the
flow lost along the walls.

`BuildCanals` turns SoftBody FO4's report into a fixed chain:
- **Vagina:** 6 links, ring to ring and then to the cervix. These are the walls secretion comes from. The first link is
  open.
- **Os:** the cervix to the uterus, as narrow as the os is open.
- **Uterus:** closed at the fundus.

**Semen trapped in the uterus:** while the os is closed, below `a_osOpen`, the uterus lets fluid back out only at `seep`
particles a second, so it drips out slowly. While the os is open, the fluid flows freely. In a standing woman, an open
os drains straight back down, so the os should open only while semen is shot through it.

## SoftBody FO4 (`src/SoftBodyBridge.h`)

The bridge reads SoftBody's exported canal (`extern/softbodyfo4/SoftBodyFO4API.h`, a copy of SoftBody's) and converts it
to the solver's frame. It reports back the uterus fill, which SoftBody swells, and the os opening.

SoftBody API version 2 adds the urethra: a third of the way from the vaginal rim's front to the clitoris, pointing out
along the vagina's axis. A version 1 SoftBody leaves the urethra unset.

## Checks

`xmake build FluidTests && xmake run FluidTests` runs them offline:
- the uterus seeps out at its rate behind a closed os, and an open os drains it;
- secretion clings and runs slowly, and Skene's fluid falls off;
- the sources follow their inputs;
- semen shot through an open os is trapped when it closes and drips out slowly.

## Next, in game

1. **A fluid session per 4Stim thread:**
   - **every frame:** read SoftBody, build the canals, and step the solver;
   - **arousal:** `getExcitement()` drives secretion;
   - **her climax:** a squirt, with a chance and strength per woman;
   - **his climax inside her:** semen at his tip, with the os opened while it passes (`SetOsOpen`);
   - **the uterus:** its particle count goes to `SetUterusFill`;
   - **urine:** a scene action.
2. **The shaft as a pusher**, as fo4-ocbpc does it, so his thrusts move fluid in the canal.
3. **Drawing.** This is still blocked by the Phase 0 strand: the per-frame buffer of a `BSDynamicTriShape`, see
   SPIKE.md. Wet skin's shine needs a material or shader approach too.
