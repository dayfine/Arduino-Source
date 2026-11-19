/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's own autosaves, as a run sees them (the "Saving..." badge, top
 *  right). The next checkpoint starts where the save reloads, so a
 *  checkpoint must end on its autosave step.
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


//  The autosave badges seen in the checkpoint under way. A checkpoint is
//  saved only when the badge last showed in its last step; an earlier
//  badge saves short of the end, and a reload would skip the steps after it.
class AutosaveSightings{
public:
    //  A checkpoint starts; a badge still up belongs to the save before it.
    void restart();
    //  A badge with none in the 5 s before it (it shows 1-3 s) is a new
    //  sighting. The last checkpoint's badge is often still up on the next
    //  one's first looks, so a badge within 5 s of one seen before the start,
    //  or first shown in the checkpoint's first 1.5 s, is not counted.
    void note(PlzaBackend& backend, const Pokemon::Observation<Pokemon::ZonePose>& look, std::string_view step);
    //  A menu save (MENU_SAVE_ACTION) counts as an autosave seen in `step`.
    void saved(PlzaBackend& backend, std::string_view step);
    //  Returns the step the badge last showed in.
    std::optional<std::string> finish(std::string_view last_step);
    bool last_end_seen() const{
        return m_last_end_seen;
    }
    //  Set: the game's latest save is past the checkpoint's start.
    const std::optional<std::string>& seen_in() const{
        return m_seen_in;
    }

private:
    std::optional<WallClock> m_origin;
    std::optional<WallClock> m_last_badge;
    std::optional<std::string> m_seen_in;
    std::optional<std::string> m_last_seen_in;
    bool m_last_end_seen = false;
    //  The badge last seen showed first in the checkpoint under way.
    bool m_badge_is_ours = false;
    bool m_earlier_badge_logged = false;
};

//  Saves from the menu where a checkpoint would otherwise end on an
//  autosave the plan does not place (a catch's).
inline constexpr std::string_view MENU_SAVE_ACTION = "save_from_menu";

//  `autosave_step` must be `row`'s last step, and an "autosave" row must
//  name it. Checked at plan load, not on the console.
std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& row, Pokemon::StoryRowSave save, std::optional<std::string_view> autosave_step
);

//  `game` noting every look in `sightings` (which must outlive it), saving
//  per Pokemon::autosave_policy() on what it saw.
Pokemon::StoryGame<Pokemon::ZonePose> with_autosave_sightings(
    Pokemon::StoryGame<Pokemon::ZonePose> game, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);

//  The reload fails as "save_moved" once the checkpoint under way has
//  autosaved: a retry from that save would start elsewhere.
Pokemon::StoryGame<Pokemon::ZonePose> reloading_only_to_start(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, const AutosaveSightings& sightings
);

//  After `checkpoint` failed: logs where it autosaved, if it did.
void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
