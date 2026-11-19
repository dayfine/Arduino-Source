/*  PokemonLZA AutoStory - Backup saves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game keeps one backup save older than its latest; the title screen's
 *  Up + B + X opens its card. A checkpoint that autosaved and then failed
 *  cannot reload its start, so the run rewinds to the checkpoint whose
 *  "start_signature" names the card's count. There is no backup until the
 *  first Pokemon Center save (03.05's heal).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackups_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackups_H

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The console's backup save, as a run rewinds to it.
struct BackupSave{
    //  Relaunches the game and opens the card at the title: its "Registered
    //  in the Pokedex", none when no card is read.
    std::function<std::optional<size_t>()> open_card;
    //  Fails as "backup_load_failed".
    std::function<std::optional<Pokemon::StoryFailure>()> load;
    //  On the card: B, back to the title, nothing loaded.
    std::function<void()> cancel;
};

//  A run of `checkpoints[first..last]` that may rewind up to `max_rewinds` times.
struct RewindPlan{
    std::span<Pokemon::StoryCheckpoint> checkpoints;
    size_t first = 0;
    size_t last = 0;
    //  Its `starts` name the checkpoints a backup card can rewind to.
    CheckpointRunOptions run_options = {};
    size_t max_rewinds = 0;
};


//  The checkpoint to play from by `read`. When it matches no start (a catch
//  autosaves mid-zone), rewinds to `backup` if `plan.max_rewinds` allows.
//  None: nothing may run.
std::optional<size_t> start_or_rewind(
    PlzaBackend& backend, const RewindPlan& plan, const StartSignature& read, const BackupSave& backup
);

//  A checkpoint that fails after it autosaved rewinds to `backup` and runs
//  on from the checkpoint the card names, at or before the failed one.
//  Stops as "unknown_backup" (never loads an unknown backup), or, before
//  the first backup exists, "no_backup_yet" unless the autosave was in the
//  last step (then the next checkpoint runs from it). A failure with no autosave
//  stops the run.
std::optional<Pokemon::CheckpointFailure> run_plza_with_rewinds(
    Pokemon::StoryGame& game,
    RewindPlan& plan, const BackupSave& backup,
    PlzaBackend& backend, AutosaveSightings& sightings, Pokemon::RunState& state
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
