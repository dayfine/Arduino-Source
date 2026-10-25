# Review comment drafts for PokemonAutomation/Arduino-Source#1717 (BDSP AutoStory)

**Not for merge, and not posted.** These are ready-to-paste drafts for the
user, who decides whether to post them, and when. Each one stands on its own
and makes no request beyond the fix. None of them proposes shared code or
asks the BDSP team to restructure anything.

Checked against PR head `65094d4cd` (2026-10-25). Before posting, check that
the head has not moved (`gh pr view 1717 -R PokemonAutomation/Arduino-Source
--json headRefOid`), and recheck the line numbers if it has. Paths are under
`SerialPrograms/Source/PokemonBDSP/Programs/AutoStory/`.

Each draft is meant as an inline comment on the file and line given in its
heading. If posted as a plain PR comment instead, add the path.

---

### 1. `PokemonBDSP_AutoStoryTools.cpp:245-246` (the retry with no save)

> When `save` is false and the retry scope isn't `SELECTED_RANGE`, a failed
> attempt logs "Reloading from backup save before retry..." and calls
> `reload_backup_save`, which is still an empty TODO (lines 188-197). The
> next attempt then starts from wherever the failure left the player. Until
> the backup reload exists, resetting from HOME here as well (the `save`
> branch just above) would at least retry from a known state.

### 2. `StorySegments/PokemonBDSP_AutoStory_Segment_08.cpp:116` (segment end)

> `AutoStory_Segment_08::run_segment` stops after 0821, but the registry
> (`PokemonBDSP_AutoStory.cpp:181-185`) continues to 0826. In segment mode,
> 0822-0826 (the house west of Sunyshore through the eighth gym) never run.

### 3. `PokemonBDSP_AutoStory.cpp:155-156` (checkpoint list)

> The checkpoint list goes from 0716 to 0718, so 0717 can't be selected as a
> start or end checkpoint in advanced mode.

### 4. `StorySegments/PokemonBDSP_AutoStory_Segment_07.cpp:214` (trainer id)

> This branch matches `"route_215_skier_edward"`, but the only caller
> (`:773`) passes `"route_216_skier_edward"`, the id that `handle_battle`
> (`:242`) also uses. The 8.1 s push back after that battle never runs.

### 5. `StorySegments/PokemonBDSP_AutoStory_Segment_04.cpp:31` and `Segment_05.cpp:30` (badge texts)

> In BDSP the third badge is Veilstone (Maylene) and the fifth is Hearthome
> (Fantina). Segment 04's start text says "Third Gym Badge acquired
> (Hearthome City)" and Segment 05's end text says "Fifth Gym Badge acquired
> (Veilstone City)", so the two city names are swapped.

### 6. `Utils/PokemonBDSP_AutoStory_Battle.cpp:208, 698` (unused handlers), optional

> `handle_battle_with_outside_help` and `handle_expert_battle` have no
> callers in the PR. Each of segments 04-09 uses its own `handle_battle`,
> keyed by trainer-id strings. Is the plan to switch the segments over to
> these?

Draft 6 is a question rather than a bug. Leave it out if the thread is
already long.

---

Left out on purpose (design opinions, not defects): the shared run-flow
core, the tile-path executor, moving the landmark alignment loop into shared
code, and the retry-limit differences from SV. Raise them only after the FRLG
pilot is running and only if they are asked about. See AutoStory.md §6.
