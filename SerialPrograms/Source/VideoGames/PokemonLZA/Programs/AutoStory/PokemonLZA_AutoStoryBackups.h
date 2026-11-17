/*  PokemonLZA AutoStory - Backup saves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game keeps one backup save, a save older than its latest: the
 *  title screen's Up + B + X opens its card (console B, 2026-11-16). A
 *  checkpoint that autosaved and then failed cannot reload its start: the
 *  latest save is past it. The run then rewinds: it opens the backup card,
 *  finds which checkpoint starts where that backup is (the card's count
 *  in the plans' "start_signature"), loads it and plays on from that
 *  checkpoint, the failed one again included. Nothing is saved by hand: the game's own autosaves
 *  take over from the backup.
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
#include "Pokemon/AutoStory/Types/Pokemon_StorySegmentTypes.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What the backup card says about its save.
struct BackupCard{
    //  "Registered in the Pokedex".
    size_t registered = 0;

    bool operator==(const BackupCard&) const = default;
};


//  The console's backup save, as a run rewinds to it.
struct BackupSave{
    //  Closes the game, launches it and opens the backup card at the
    //  title: what the card says, none when no card is read.
    std::function<std::optional<BackupCard>(PlzaBackend&)> open_card;
    //  On the card: loads the backup, into the game. Fails as
    //  "backup_load_failed".
    std::function<Pokemon::Outcome(PlzaBackend&)> load;
    //  On the card: B, back to the title, nothing loaded.
    std::function<void(PlzaBackend&)> cancel;
};

//  A run that may rewind: the rows `first` to `last` of `rows` (all the
//  story's checkpoints), `attempts_per_checkpoint` each, and up to
//  `max_rewinds` rewinds to the backup saves `starts` name (a backup
//  card's StartSignature::registered).
struct RewindPlan{
    std::span<const Pokemon::StoryCheckpoint> rows;
    size_t first = 0;
    size_t last = 0;
    size_t attempts_per_checkpoint = 1;
    std::span<const CheckpointStart> starts = {};
    size_t max_rewinds = 0;
};


//  The checkpoint a run asked to start at `plan.first` plays from, by
//  `read`, the start signature read where the player stands: run_start()'s.
//  When that matches no start and `plan.max_rewinds` allows one, the run
//  rewinds to `backup` first, and plays from the checkpoint its card names:
//  a catch autosaves where it lands, so 03.08's latest save can stand
//  mid-zone (console B, 2026-11-16 17:24, at the 100 spot). None: neither
//  names a checkpoint, and nothing may run.
std::optional<size_t> start_or_rewind(
    PlzaBackend& backend, const RewindPlan& plan, const StartSignature& read, const BackupSave& backup
);

//  `plan`'s rows with run_plza_segments(). A checkpoint that fails after
//  the game autosaved in it (seen, or its reload failed as "save_moved")
//  has no start save left: the run rewinds to `backup`. It opens the
//  backup card and, when its count names a checkpoint at or before the
//  failed one (checkpoint_at()), loads it and runs from that checkpoint to
//  `plan.last`, up to `plan.max_rewinds` times. A card not read, or one no
//  start matches, is cancelled and the run stops as "unknown_backup": an
//  unknown backup is never loaded. A failure with no autosave in it stops
//  the run, as do the failures past the last rewind. Logs each decision
//  as "[Rewind] ...".
Pokemon::SegmentReport run_plza_with_rewinds(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, const Pokemon::StoryRows& story,
    const RewindPlan& plan, const BackupSave& backup,
    PlzaBackend& backend, AutosaveSightings& sightings, Pokemon::RunState& state
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
