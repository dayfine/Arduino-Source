# FRLG AutoStory: path to the Elite Four (user, 2026-11-12)

Moved from fork PR #435 (`frlg/design-2026-11-12`), where it was an edit to
`docs/design/AutoStory.md` (§9 segment table and a new §12). The code
branches keep `docs/design/AutoStory.md` without it.

## Segment table (§9, FRLG rows 07 to 09 as revised)

| # | Segment | Notes |
|---|---|---|
| 07 | To Cinnabar (Route 21 by default) → Mansion (Secret Key) → Blaine | Mansion switches and quiz doors are gates, Mansion holes are one-way warps (§12.2); no Seafoam; decline Bill's One Island trip |
| 08 | Saffron: Silph Co. (Card Key, warp pads, rival, Giovanni) → Sabrina | Card Key doors are gates (§12.2); Lapras is an RNG gift like Eevee; Master Ball is an option (§12.1) |
| 09 | Giovanni (Viridian) → Route 22 rival → Victory Road (Strength boulders) | arrow tiles and boulders are scripted (§12.3); Route 23 guards: read the badges on the trainer card |

The story order after Koga follows PokeGuy's run: Fly to Pallet, Surf down Route 21 to Cinnabar, the Mansion and Blaine (TM14 Blizzard taught over Dig), Fly back to Celadon for Erika, then Silph Co., Sabrina's gym and Viridian's gym (STORY_ORDER in PokemonFRLG_AutoStoryTable.cpp is the exact order).

## 12. Path to the Elite Four

### 12.1 Order and options

Checkpoint ids need not follow story order. Different starters reorder
segments anyway, so ids are never renumbered; the table runs in the order of
an explicit `STORY_ORDER` list, and a checkpoint missing from it, or listed
twice, fails to load.

- Silph Co.: Lapras is an RNG gift, done the same way as the Eevee gift. The
  Master Ball from the president is taken by default, as a program option
  that SPEEDRUN mode skips.
- After Blaine, decline Bill's trip to One Island. The Sevii Islands are not
  part of the run to the Elite Four.
- Cinnabar is reached by Surf down Route 21; Seafoam Islands are not part of
  the MVP.
- Route 23: before the badge guards, read the badge count from the trainer
  card.

### 12.2 Gates: doors that open during the story

Silph Co.'s Card Key doors, the Pokémon Mansion's statue-switch barriers and
the Cinnabar Gym's quiz doors are walls that open during the story. The map
data marks them with one tile kind, `gate`, and each map lists its gates:
their tiles, the flag or variable that opens them, and how (Card Key and A,
a switch, a quiz or a trainer). A Mansion switch toggles a set of gates, so
the map also lists, per switch state, which gates are open. The overlays stay
the record of which flags are set at each point of the story.

AutoStory keeps the gates as story state:
- The checkpoint records the gates it opened and the Mansion switch state,
  and a checkpoint's declared start includes the gate state it expects.
- The planner treats a closed gate as a wall unless the plan opens it;
  opening is one action step (face it and press A) that the planner adds. In
  the Mansion the switch state is part of the search, and the open gates
  follow from it.
- When the state is not known, after a hand recovery for example, it comes
  from the story's progress, or else from a shot of a gate tile before
  planning.

Holes that drop to the floor below (the Mansion, some caves) are one-way
warps, and the planner may use them as such.

### 12.3 Scripted mechanics

- Viridian Gym's arrow tiles are scripted like the Rocket Hideout's spin
  tiles: step onto the arrow without counting tiles, wait for the screen to
  settle, then localize from where the slide stopped.
- Victory Road's boulders are not modelled as movable objects. Each push
  waits for the screen to settle and then checks that the boulder is still
  next to the player in the push direction before the next push.
