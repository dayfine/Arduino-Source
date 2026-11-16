/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's own autosaves, as a run sees them: the badge (a pencil and
 *  "Saving...", top right) on any look of any step of a checkpoint. A
 *  checkpoint whose badge showed is saved for that run; a chained run
 *  reloads only onto such a save (run_plza_segments()).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H
#define PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H

#include <optional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The game's autosave badges (AUTOSAVE_READING) a run sees in the
//  checkpoint under way. The story game made with it notes every look of
//  every step, and its save counts a checkpoint saved when the badge showed
//  during it: the save is verified for this run.
class AutosaveSightings{
public:
    //  A checkpoint starts: no badge seen in it yet.
    void restart();
    //  `look`, taken during step `step`. A badge with none in the 5 s
    //  before it (the badge shows 1-3 s) is a new sighting, logged as
    //  "[PLZA] autosave badge at <t>s in <step>", t from the checkpoint's
    //  first look.
    void note(PlzaBackend& backend, const Pokemon::Observation<Pokemon::ZonePose>& look, std::string_view step);
    //  The checkpoint ends: whether a badge showed during it, kept for
    //  last_end_seen().
    bool finish();
    //  Whether the badge showed during the checkpoint that last ended.
    bool last_end_seen() const{
        return m_last_end_seen;
    }

private:
    std::optional<WallClock> m_origin;
    std::optional<WallClock> m_last_badge;
    bool m_seen = false;
    bool m_last_end_seen = false;
};

//  `game` with every action's looks noted in `sightings`, which must
//  outlive it, and its save counting the autosave `sightings` saw in the
//  checkpoint ("[Save] <id>: the game autosaved (seen)"); else nothing
//  saved (Pokemon::NOT_SAVED), worded by the plan's "save" in `rows`.
Pokemon::StoryGame<Pokemon::ZonePose> with_autosave_sightings(
    Pokemon::StoryGame<Pokemon::ZonePose> game, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
