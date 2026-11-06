# AutoStory: sections, postgame and DLC

Status: **design note only**. Nothing here is implemented; it records how the
shared start/end selection grows past SV's main story, so the next person to
add a postgame or DLC starts from the right questions.

## Today

`Pokemon/AutoStory/Pokemon_AutoStorySelect.h` (upstream PokemonAutomation/Arduino-Source#1828)
builds the start/end dropdowns and maps a selection back to one flat story list:

- SV AutoStory has one ordered list, `ALL_AUTO_STORY_SEGMENT_LIST()`, segments
  0 to 40, ending with "40: Finish Main Story" (the AI Professor). No postgame,
  no The Teal Mask, no The Indigo Disk.
- The list is cut into two consecutive sections, tutorial then main story. Each
  has its own start/end dropdowns; `STORY_SECTION` picks which pair counts.
- `story_index(bool main_story, index_in_section, first_main_story_index)` turns
  a position in a section back into the flat index.
- FRLG and BDSP have one section; their checkpoints have stable string ids
  (`"00.08"`).

## Rule: a section is a break

A section ends where the user must step in, not where the content changes
(jw098 on #1828): SV's tutorial ends where the user levels up the Pokemon by
hand. Content that needs no break is more checkpoints at the end of the last
section.

## Cases

| Case | Shape | Model |
|---|---|---|
| FRLG / SV postgame | carries on from the ending | append to the last section |
| SV DLC, run after the main story | carries on from the ending | append; a new section only if a break is needed |
| DLC that needs grinding first (e.g. Legends Z-A's DLC is a big step up) | break before it | new section |
| DLC started partway through the main story | branch: later checkpoints can't assume one game state | **not supported**: DLC runs only after the main story |
| another game with 0, 1 or 3 DLCs | per-game count | sections as data, not a bool |

Decision (user, 2026-11-02): disallow DLC-first. A player may go off to the DLC
by hand, but AutoStory's DLC checkpoints assume the main story is done.

## DLC bought later

A user ran the main story, the DLC comes out, they buy it and want only the DLC:

1. The DLC checkpoints are appended after the main story's last one (or form
   the next section). Appending moves no existing index, so saved start/end
   settings still mean the same checkpoints.
2. The user picks the first DLC checkpoint as start and the last as end. Each
   checkpoint starts from the save, so it doesn't matter whether the main story
   was played by AutoStory or by hand.
3. The first DLC checkpoint states its precondition (main story done, DLC
   installed, where to stand) and checks it at the start: no DLC or wrong spot
   stops the run with a clear message before anything is saved.

## What changes in code

- `story_index(bool main_story, ...)` assumes two sections. A third one (a
  break before a DLC) turns the bool into a section index and the section list
  into per-game data.
- Inserting checkpoints mid-list would shift saved indices; only append, or
  identify checkpoints by stable string ids (FRLG and BDSP already have them,
  e.g. `"00.08"`).

## If branches are ever supported: tracks

Only needed if DLC-first (or another free-order choice) is supported. Sketch:

```cpp
struct StoryRef{
    std::string track;      //  "main"
    std::string entry;      //  a checkpoint id in that track
};

struct StoryTrack{
    std::string id;         //  "main", "teal_mask"
    std::string name;       //  shown in the UI
    std::vector<Entry> entries;             //  linear within the track
    std::optional<StoryRef> starts_after;   //  where the branch leaves "main"
};
```

A run picks a track, then start and end inside it; checkpoints are identified by
track plus entry id; SV's saved (section, index) settings migrate to (track, id).
Later checkpoints in "main" would then need a known state for both orders, which
is why this stays out until someone needs it.
