/*  PokemonLZA AutoStory - Backup saves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryBackups.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::Outcome;
using Pokemon::SegmentReport;
using Pokemon::StoryCheckpoint;
using Pokemon::ZonePose;


namespace{

//  What a run does after a checkpoint failed.
enum class AfterFailure{
    STOP,           //  its start save is still the latest: the run's own
                    //  retries (attempts per checkpoint) were all it had
    REWIND,         //  the game autosaved in it: load the backup
    REWIND_LIMIT,   //  the game autosaved in it, but the run has rewound
                    //  as often as it may
};

//  After a failed checkpoint whose save moved past its start or not
//  (`save_moved`), with `rewinds` of `max_rewinds` rewinds done.
AfterFailure after_failure(bool save_moved, size_t rewinds, size_t max_rewinds){
    if (!save_moved){
        return AfterFailure::STOP;
    }
    return rewinds < max_rewinds ? AfterFailure::REWIND : AfterFailure::REWIND_LIMIT;
}

//  The index of the row `id` in `rows`, if there is one.
std::optional<size_t> row_index(std::span<const StoryCheckpoint> rows, std::string_view id){
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  `part` added to the run's `total` so far: its counts and runs, and its
//  end.
void add_part(const SegmentReport& part, SegmentReport& total){
    total.completed = part.completed;
    total.checkpoints_run += part.checkpoints_run;
    total.reloads += part.reloads;
    total.runs.insert(total.runs.end(), part.runs.begin(), part.runs.end());
    total.failed_checkpoint = part.failed_checkpoint;
    total.failed_event = part.failed_event;
    total.failure = part.failure;
}
//  Opens the backup card and loads it when its count names a checkpoint
//  at or before `failed` (an index of `rows`, checkpoint_at()): that
//  checkpoint's index. Otherwise the card is cancelled, nothing loaded,
//  and the failure ("unknown_backup", or the load's).
std::variant<size_t, Outcome> rewind_to_backup(
    PlzaBackend& backend, std::span<const StoryCheckpoint> rows, size_t failed,
    std::span<const CheckpointStart> starts, const BackupSave& backup
){
    const std::optional<BackupCard> card = backup.open_card(backend);
    if (!card){
        backend.log("[Rewind] the backup card was not read: nothing loaded");
        backup.cancel(backend);
        return Pokemon::outcome_failed("unknown_backup", "the backup card was not read");
    }
    const std::optional<size_t> index = checkpoint_at(rows, starts, failed, StartSignature{.registered = card->registered});
    if (!index){
        backend.log(std::format(
            "[Rewind] backup card: {} registered: no checkpoint up to {} starts there; B, nothing loaded",
            card->registered, rows[failed].id
        ));
        backup.cancel(backend);
        return Pokemon::outcome_failed("unknown_backup", std::format(
            "no checkpoint up to {} starts at the backup save ({} registered in the Pokedex)", rows[failed].id, card->registered
        ));
    }
    backend.log(std::format("[Rewind] backup card: {} registered: {}'s start; loading it", card->registered, rows[*index].id));
    const Outcome loaded = backup.load(backend);
    if (!loaded.succeeded()){
        backend.log(std::format("[Rewind] the backup did not load: {}", to_string(loaded)));
        return loaded;
    }
    return *index;
}

} // namespace


std::optional<size_t> start_or_rewind(
    PlzaBackend& backend, std::span<const StoryCheckpoint> rows, size_t first,
    std::span<const CheckpointStart> starts, const StartSignature& read,
    const BackupSave& backup, size_t max_rewinds
){
    const std::optional<size_t> from = run_start(rows, starts, first, read);
    if (from || max_rewinds == 0){
        return from;
    }
    backend.log(std::format(
        "AutoStory: the screen ({}) matches no checkpoint start up to {}: rewinding to the backup save", to_string(read), rows[first].id
    ));
    const std::variant<size_t, Outcome> back = rewind_to_backup(backend, rows, first, starts, backup);
    if (const size_t* index = std::get_if<size_t>(&back)){
        return *index;
    }
    backend.log(std::format("AutoStory: no rewind: {}", to_string(std::get<Outcome>(back))));
    return std::nullopt;
}


SegmentReport run_plza_with_rewinds(
    const Pokemon::StoryGame<ZonePose>& game, const Pokemon::StoryRows& story,
    const RewindPlan& plan, const BackupSave& backup,
    PlzaBackend& backend, AutosaveSightings& sightings, Pokemon::RunState& state
){
    const std::span<const StoryCheckpoint> rows = plan.rows;
    SegmentReport total;
    size_t from = plan.first;
    for (size_t rewinds = 0;; rewinds++){
        const std::vector<StoryCheckpoint> run_rows = Pokemon::story_run_rows(rows, from, plan.last);
        add_part(run_plza_segments(game, story, run_rows, plan.attempts_per_checkpoint, backend, state, sightings), total);
        if (total.completed){
            return total;
        }
        const std::string& failed_id = total.failed_checkpoint;
        const std::optional<std::string>& seen_in = sightings.seen_in();
        const std::optional<size_t> failed = row_index(rows, failed_id);
        //  A catch autosaves where it lands, and the badge can go unseen:
        //  the next try then reloads off 03.08's rocks ("rocks_not_left", read
        //  96 after a catch at 98, console B 2026-11-16 16:40). Either
        //  failure says the start save is gone.
        const std::string& kind = total.failure.failure;
        const bool save_moved = failed && (seen_in || kind == "save_moved" || kind == "rocks_not_left");
        switch (after_failure(save_moved, rewinds, plan.max_rewinds)){
        case AfterFailure::STOP:
            backend.log(std::format("[Rewind] {} failed with no autosave in it: no rewind", failed_id));
            return total;
        case AfterFailure::REWIND_LIMIT:
            backend.log(std::format("[Rewind] {} failed after an autosave, but the run has rewound {} times already: stop", failed_id, rewinds));
            return total;
        case AfterFailure::REWIND:
            break;
        }
        backend.log(std::format(
            "[Rewind] {} failed after the game autosaved in {}: its start save is gone; rewinding to the backup save ({} of {})",
            failed_id, seen_in.value_or("a step"), rewinds + 1, plan.max_rewinds
        ));
        const std::variant<size_t, Outcome> back = rewind_to_backup(backend, rows, *failed, plan.starts, backup);
        if (const Outcome* failure = std::get_if<Outcome>(&back)){
            total.failed_event = "rewind";
            total.failure = *failure;
            return total;
        }
        from = std::get<size_t>(back);
        backend.log(std::format("[Rewind] on from {} to {}", rows[from].id, rows[plan.last].id));
        state = Pokemon::RunState{};
        sightings.restart();
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
