/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's own autosaves, as a run sees them: the badge (a pencil and
 *  "Saving...", top right) on any look of any step of a checkpoint. A
 *  checkpoint whose badge showed in its last step is saved for that run; a
 *  chained run reloads only onto such a save (run_plza_segments()). The
 *  next checkpoint starts where that save reloads, so a checkpoint ends on
 *  its autosave: the plan names the step it shows in ("autosave_step"),
 *  and the plan check (autosave_step_error()) fails a checkpoint with steps
 *  after it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H
#define PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The game's autosave badges (AUTOSAVE_READING) a run sees in the
//  checkpoint under way. The story game made with it notes every look of
//  every step, and its save counts a checkpoint saved when the badge last
//  showed in its last step: the save is verified for this run. A badge in
//  an earlier step saves the game short of the checkpoint's end, where a
//  reload would skip the steps after it (console B, 2026-11-16: 03.07
//  reloaded 03.06's autosave from before the run to Wild Zone 1's gate).
class AutosaveSightings{
public:
    //  A checkpoint starts: no badge seen in it yet.
    void restart();
    //  `look`, taken during step `step`. A badge with none in the 5 s
    //  before it (the badge shows 1-3 s) is a new sighting, logged as
    //  "[PLZA] autosave badge at <t>s in <step>", t from the checkpoint's
    //  first look.
    void note(PlzaBackend& backend, const Pokemon::Observation<Pokemon::ZonePose>& look, std::string_view step);
    //  The run saved the game from the menu in step `step` (MENU_SAVE_ACTION):
    //  counted as an autosave seen there, logged as "[PLZA] saved from the
    //  menu in <step>".
    void saved(PlzaBackend& backend, std::string_view step);
    //  The checkpoint ends with step `last_step`: the step the badge last
    //  showed in, if it did. Whether that is `last_step` is kept for
    //  last_end_seen().
    std::optional<std::string> finish(std::string_view last_step);
    //  Whether the badge showed in the last step of the checkpoint that
    //  last ended.
    bool last_end_seen() const{
        return m_last_end_seen;
    }
    //  The step of the checkpoint under way the badge first showed in, if
    //  it did: the game's latest save is then past the checkpoint's start.
    const std::optional<std::string>& seen_in() const{
        return m_seen_in;
    }

private:
    std::optional<WallClock> m_origin;
    std::optional<WallClock> m_last_badge;
    std::optional<std::string> m_seen_in;
    std::optional<std::string> m_last_seen_in;
    bool m_last_end_seen = false;
};

//  The action that saves the game from the menu at a fixed spot, where a
//  checkpoint would otherwise end on an autosave the plan does not place
//  (a catch's). A plan names its step as "autosave_step".
inline constexpr std::string_view MENU_SAVE_ACTION = "save_from_menu";

//  Why `row` cannot end on the game's autosave in its step `autosave_step`
//  (the plan's "autosave_step"; `save` is the plan's "save"), none when it
//  can: the step must be one of its steps and its last, and an "autosave"
//  row must name it. Checked when the plan loads, so a checkpoint that
//  would start the next one mid-way fails there, not on the console.
std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& row, Pokemon::StoryRowSave save, std::optional<std::string_view> autosave_step
);

//  `game` with every action's looks noted in `sightings`, which must
//  outlive it, a MENU_SAVE_ACTION step that succeeds noted as saved, and
//  its save counting the autosave `sightings` saw in the
//  checkpoint's last step ("[Save] <id>: the game autosaved (seen)"); one
//  seen only before it saves nothing ("[Save] <id>: the game autosaved in
//  <step>, before <next>"); else nothing saved (Pokemon::NOT_SAVED),
//  worded by the plan's "save" in `rows`.
Pokemon::StoryGame<Pokemon::ZonePose> with_autosave_sightings(
    Pokemon::StoryGame<Pokemon::ZonePose> game, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);

//  `game` whose reload fails as "save_moved" while `sightings` has seen an
//  autosave in the checkpoint under way: the game's latest save is then
//  past that checkpoint's start, and a retry from it would start elsewhere
//  (console B 2026-11-16: 03.04 autosaved in pokedex_app, failed after it,
//  and the next 03.04 walked from outside the Center).
Pokemon::StoryGame<Pokemon::ZonePose> reloading_only_to_start(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, const AutosaveSightings& sightings
);

//  After `checkpoint` failed: logs "[Save] <checkpoint>: the game autosaved
//  in <step> before the run failed" when `sightings` saw an autosave in it.
void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
