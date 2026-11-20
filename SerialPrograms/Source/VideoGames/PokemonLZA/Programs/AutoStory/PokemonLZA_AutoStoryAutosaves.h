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
#include <span>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryPlan.h"

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
    void note(PlzaBackend& backend, const ScreenObservation& look);
    //  A menu save (PlzaAction::SAVE_FROM_MENU) counts as an autosave seen in the step.
    void saved(PlzaBackend& backend);
    //  The step under way, which note() and saved() name.
    void enter(std::string_view step){
        m_step = std::string(step);
    }
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
    std::string m_step;
    std::optional<WallClock> m_origin;
    std::optional<WallClock> m_last_badge;
    std::optional<std::string> m_seen_in;
    std::optional<std::string> m_last_seen_in;
    bool m_last_end_seen = false;
    //  The badge last seen showed first in the checkpoint under way.
    bool m_badge_is_ours = false;
    bool m_earlier_badge_logged = false;
};

//  `autosave_step` must be `checkpoint`'s last step, and an "autosave" checkpoint must
//  name it. Checked at plan load, not on the console.
std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& checkpoint, bool marked_autosave, std::optional<std::string_view> autosave_step
);

//  Every step of `checkpoints` notes its looks in `sightings`, which must
//  outlive them.
void note_autosaves(std::span<Pokemon::StoryCheckpoint> checkpoints, AutosaveSightings& sightings);

//  The program never saves: a checkpoint ends on a save when `sightings`
//  saw the autosave in its last step ("[Save] <id>: the game autosaved
//  (seen)"); else it logs why not, by what `marked` expected. `sightings`
//  must outlive it.
decltype(Pokemon::StoryGame::save) autosave_policy(MarkedCheckpoints marked, AutosaveSightings& sightings);

//  `game`'s reload, which says it is at no start once the checkpoint under
//  way has autosaved: a retry from that save would start elsewhere. Both
//  must outlive it.
decltype(Pokemon::StoryGame::reload) reloading_only_to_start(Pokemon::StoryGame& game, const AutosaveSightings& sightings);

//  After `checkpoint` failed: logs where it autosaved, if it did.
void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
