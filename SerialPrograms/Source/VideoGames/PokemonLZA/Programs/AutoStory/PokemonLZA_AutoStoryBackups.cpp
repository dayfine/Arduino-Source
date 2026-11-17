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

//  The index of the first row of `rows` a backup save can load at: the
//  first whose start in `starts` names a backup card's count
//  ("registered"). There is no backup until the first Pokemon Center save:
//  03.05's heal, which loads at 03.06's start (console B, 2026-11-16).
//  None: no plan names one, and no backup is ever opened.
std::optional<size_t> first_backup_row(std::span<const StoryCheckpoint> rows, std::span<const CheckpointStart> starts){
    for (size_t index = 0; index < rows.size(); index++){
        const CheckpointStart* start = find_start(starts, rows[index].id);
        if (start != nullptr && start->signature.registered){
            return index;
        }
    }
    return std::nullopt;
}

//  "no backup save exists before 03.06", by first_backup_row().
std::string no_backup_before(std::span<const StoryCheckpoint> rows, std::optional<size_t> first_backup){
    return first_backup
        ? std::format("no backup save exists before {}", rows[*first_backup].id)
        : std::string("no plan names a backup save's start");
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

//  After `failed` (an index of `plan.rows`, before the first backup) failed
//  with the latest save past its start: an autosave in its last step
//  (`seen_in`) is the plan's autosave, the next checkpoint's start; that
//  save is loaded and its index returned. Anywhere else the position is
//  unknown: nothing is loaded, and the failure ("no_backup_yet") says
//  where the save is.
std::variant<size_t, Outcome> reload_latest_save(
    const Pokemon::StoryGame<ZonePose>& game, PlzaBackend& backend, const RewindPlan& plan,
    size_t failed, const std::optional<std::string>& seen_in
){
    const std::span<const StoryCheckpoint> rows = plan.rows;
    const StoryCheckpoint& row = rows[failed];
    const std::string none = no_backup_before(rows, first_backup_row(rows, plan.starts));
    const bool at_end = seen_in && !row.steps.empty() && *seen_in == row.steps.back().name;
    if (at_end && failed + 1 <= plan.last){
        const StoryCheckpoint& next = rows[failed + 1];
        backend.log(std::format(
            "[Rewind] {}: the latest save is {}'s start ({}'s autosave in {}); loading it", none, next.id, row.id, *seen_in
        ));
        const Outcome loaded = load_checkpoint_start(game, backend, next);
        if (!loaded.succeeded()){
            return loaded;
        }
        return failed + 1;
    }
    std::string where = std::format("past {}'s start", row.id);
    if (at_end && failed + 1 < rows.size()){
        where = std::format("{}'s start, past the run's end", rows[failed + 1].id);
    }else if (seen_in){
        where = std::format("in {}'s {}", row.id, *seen_in);
    }
    const std::string message = std::format("{}: the latest save is {}", none, where);
    backend.log(std::format("[Rewind] {}", message));
    return Pokemon::outcome_failed("no_backup_yet", message);
}

//  After `failed` (an index of `plan.rows`) failed with its start save
//  gone, as rewind `attempt` of `plan.max_rewinds`: to the backup when one
//  exists at or before it (rewind_to_backup()), else to the latest save
//  (reload_latest_save()).
std::variant<size_t, Outcome> rewind_after(
    const Pokemon::StoryGame<ZonePose>& game, PlzaBackend& backend, const RewindPlan& plan, const BackupSave& backup,
    size_t failed, const std::optional<std::string>& seen_in, size_t attempt
){
    const std::optional<size_t> first_backup = first_backup_row(plan.rows, plan.starts);
    if (!first_backup || failed < *first_backup){
        return reload_latest_save(game, backend, plan, failed, seen_in);
    }
    backend.log(std::format(
        "[Rewind] {} failed after the game autosaved in {}: its start save is gone; rewinding to the backup save ({} of {})",
        plan.rows[failed].id, seen_in.value_or("a step"), attempt, plan.max_rewinds
    ));
    return rewind_to_backup(backend, plan.rows, failed, plan.starts, backup);
}

} // namespace


std::optional<size_t> start_or_rewind(
    PlzaBackend& backend, const RewindPlan& plan, const StartSignature& read, const BackupSave& backup
){
    const std::optional<size_t> from = run_start(plan.rows, plan.starts, plan.first, read);
    if (from || plan.max_rewinds == 0){
        return from;
    }
    const std::optional<size_t> first_backup = first_backup_row(plan.rows, plan.starts);
    if (!first_backup || plan.first < *first_backup){
        backend.log(std::format(
            "AutoStory: the screen ({}) matches no checkpoint start up to {}, and {}: no rewind",
            to_string(read), plan.rows[plan.first].id, no_backup_before(plan.rows, first_backup)
        ));
        return std::nullopt;
    }
    backend.log(std::format(
        "AutoStory: the screen ({}) matches no checkpoint start up to {}: rewinding to the backup save", to_string(read), plan.rows[plan.first].id
    ));
    const std::variant<size_t, Outcome> back = rewind_to_backup(backend, plan.rows, plan.first, plan.starts, backup);
    if (const size_t* index = std::get_if<size_t>(&back)){
        return *index;
    }
    backend.log(std::format("AutoStory: no rewind: {}", to_string(std::get<Outcome>(back))));
    return std::nullopt;
}


SegmentReport run_plza_with_rewinds(
    const Pokemon::StoryGame<ZonePose>& game,
    const RewindPlan& plan, const BackupSave& backup,
    PlzaBackend& backend, AutosaveSightings& sightings, Pokemon::RunState& state
){
    const std::span<const StoryCheckpoint> rows = plan.rows;
    SegmentReport total;
    size_t from = plan.first;
    for (size_t rewinds = 0;; rewinds++){
        const std::vector<StoryCheckpoint> run_rows = Pokemon::story_run_rows(rows, from, plan.last);
        add_part(run_plza_segments(game, run_rows, plan.segments, backend, state, sightings), total);
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
        const std::variant<size_t, Outcome> back = rewind_after(game, backend, plan, backup, *failed, seen_in, rewinds + 1);
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
