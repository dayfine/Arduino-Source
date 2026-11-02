# AutoStory: story tracks for postgame and DLC

Status: **design note only**. Nothing here is implemented; it records how the
shared start/end selection could grow past two sections, so the next person to
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

## What doesn't fit

| Case | Shape | One list cut into sections? |
|---|---|---|
| FRLG postgame (Sevii 4-7, National Dex after the Hall of Fame) | carries straight on from the main story | yes: more checkpoints, or a third section |
| SV postgame (Academy Ace Tournament, ...) | carries straight on from the ending | yes |
| SV The Teal Mask | branches: can start partway through the main story, then its own linear story | no |
| SV The Indigo Disk | branches, and needs The Teal Mask finished | no |
| another game with 0, 1 or 3 DLCs | per-game count | only if sections are data, not a bool or shared enum |

## Sketch: tracks

```cpp
struct StoryRef{
    std::string track;      //  "main"
    std::string entry;      //  a checkpoint id in that track
};

struct StoryTrack{
    std::string id;         //  "tutorial", "main", "postgame", "teal_mask"
    std::string name;       //  shown in the UI
    std::vector<Entry> entries;             //  linear within the track
    std::optional<StoryRef> starts_after;   //  Teal Mask: after main X;
                                            //  Indigo Disk: after Teal Mask's end;
                                            //  postgame: after main's end
};

//  Each game supplies its tracks as data.
const std::vector<StoryTrack>& story_tracks();
```

- A run picks one track, then a start and an end inside it. As now, the save
  must match the start checkpoint; `starts_after` documents and validates the
  order, it doesn't drive the run.
- UI: a track dropdown, then start/end dropdowns filtered to that track. With one
  track (FRLG, BDSP today) the track dropdown can be hidden.
- Identify checkpoints by track id plus entry id (or stable string ids), not by
  one global index. SV stores the dropdown choice as an index string in users'
  saved settings; splicing a DLC into one flat numbering would shift every later
  index and silently change saved settings. A migration from SV's current
  two-section settings would map (section, index) to (track, entry id).
- An appended postgame needs only a new track (or more entries in "main"); the
  branch cases are what force `starts_after` and per-track ids.

## When to do it

Not before a game needs a third section. The first one decides the shape: FRLG's
postgame is an append; SV DLC is a branch. Until then `story_index()` carries a
NOTE pointing here.
