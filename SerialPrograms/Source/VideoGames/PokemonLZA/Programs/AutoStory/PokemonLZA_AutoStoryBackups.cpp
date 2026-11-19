/*  PokemonLZA AutoStory - Backup saves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryBackups.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


namespace{

//  checkpoints[first..last]; empty when the range is out of `checkpoints`.
std::span<StoryCheckpoint> checkpoint_range(std::span<StoryCheckpoint> checkpoints, size_t first, size_t last){
    if (first > last || last >= checkpoints.size()){
        return {};
    }
    return checkpoints.subspan(first, last - first + 1);
}

std::optional<size_t> checkpoint_index(std::span<const StoryCheckpoint> checkpoints, std::string_view id){
    for (size_t index = 0; index < checkpoints.size(); index++){
        if (checkpoints[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  The first checkpoint whose start names a backup card's count. None: no backup
//  is ever opened.
std::optional<size_t> first_backup_checkpoint(std::span<const StoryCheckpoint> checkpoints, std::span<const CheckpointStart> starts){
    for (size_t index = 0; index < checkpoints.size(); index++){
        const CheckpointStart* start = find_start(starts, checkpoints[index].id);
        if (start != nullptr && start->signature.registered){
            return index;
        }
    }
    return std::nullopt;
}

std::string no_backup_before(std::span<const StoryCheckpoint> checkpoints, std::optional<size_t> first_backup){
    return first_backup
        ? std::format("no backup save exists before {}", checkpoints[*first_backup].id)
        : std::string("no plan names a backup save's start");
}

//  Loads the backup only when its card names a checkpoint at or before
//  `failed`; otherwise cancels the card ("unknown_backup").
std::variant<size_t, Pokemon::StoryFailure> rewind_to_backup(
    PlzaBackend& backend, std::span<const StoryCheckpoint> checkpoints, size_t failed,
    std::span<const CheckpointStart> starts, const BackupSave& backup
){
    const std::optional<size_t> registered = backup.open_card();
    if (!registered){
        backend.log("[Rewind] the backup card was not read: nothing loaded");
        backup.cancel();
        return Pokemon::StoryFailure{.code = "unknown_backup", .detail = "the backup card was not read"};
    }
    const std::optional<size_t> index = checkpoint_at(checkpoints, starts, failed, StartSignature{.registered = registered});
    if (!index){
        backend.log(std::format(
            "[Rewind] backup card: {} registered: no checkpoint up to {} starts there; B, nothing loaded",
            *registered, checkpoints[failed].id
        ));
        backup.cancel();
        return Pokemon::StoryFailure{.code = "unknown_backup", .detail = std::format(
            "no checkpoint up to {} starts at the backup save ({} registered in the Pokedex)", checkpoints[failed].id, *registered
        )};
    }
    backend.log(std::format("[Rewind] backup card: {} registered: {}'s start; loading it", *registered, checkpoints[*index].id));
    const std::optional<Pokemon::StoryFailure> loaded = backup.load();
    if (loaded){
        backend.log(std::format("[Rewind] the backup did not load: {}", *loaded));
        return *loaded;
    }
    return *index;
}

//  Before the first backup: an autosave in the failed checkpoint's last step is
//  the next checkpoint's start and is loaded; anywhere else the position is
//  unknown and nothing is loaded ("no_backup_yet").
std::variant<size_t, Pokemon::StoryFailure> reload_latest_save(
    Pokemon::StoryGame& game, PlzaBackend& backend, const RewindPlan& plan,
    size_t failed, const std::optional<std::string>& seen_in
){
    const std::span<const StoryCheckpoint> checkpoints = plan.checkpoints;
    const StoryCheckpoint& checkpoint = checkpoints[failed];
    const std::string none = no_backup_before(checkpoints, first_backup_checkpoint(checkpoints, plan.run_options.starts));
    const bool at_end = seen_in && !checkpoint.steps.empty() && *seen_in == checkpoint.steps.back().name;
    if (at_end && failed + 1 <= plan.last){
        const StoryCheckpoint& next = checkpoints[failed + 1];
        backend.log(std::format(
            "[Rewind] {}: the latest save is {}'s start ({}'s autosave in {}); loading it", none, next.id, checkpoint.id, *seen_in
        ));
        const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(game, backend, next);
        if (loaded){
            return *loaded;
        }
        return failed + 1;
    }
    std::string where = std::format("past {}'s start", checkpoint.id);
    if (at_end && failed + 1 < checkpoints.size()){
        where = std::format("{}'s start, past the run's end", checkpoints[failed + 1].id);
    }else if (seen_in){
        where = std::format("in {}'s {}", checkpoint.id, *seen_in);
    }
    const std::string message = std::format("{}: the latest save is {}", none, where);
    backend.log(std::format("[Rewind] {}", message));
    return Pokemon::StoryFailure{.code = "no_backup_yet", .detail = message};
}

std::variant<size_t, Pokemon::StoryFailure> rewind_after(
    Pokemon::StoryGame& game, PlzaBackend& backend, const RewindPlan& plan, const BackupSave& backup,
    size_t failed, const std::optional<std::string>& seen_in, size_t attempt
){
    const std::optional<size_t> first_backup = first_backup_checkpoint(plan.checkpoints, plan.run_options.starts);
    if (!first_backup || failed < *first_backup){
        return reload_latest_save(game, backend, plan, failed, seen_in);
    }
    backend.log(std::format(
        "[Rewind] {} failed after the game autosaved in {}: its start save is gone; rewinding to the backup save ({} of {})",
        plan.checkpoints[failed].id, seen_in.value_or("a step"), attempt, plan.max_rewinds
    ));
    return rewind_to_backup(backend, plan.checkpoints, failed, plan.run_options.starts, backup);
}

} // namespace


std::optional<size_t> start_or_rewind(
    PlzaBackend& backend, const RewindPlan& plan, const StartSignature& read, const BackupSave& backup
){
    const std::span<const CheckpointStart> starts = plan.run_options.starts;
    const std::optional<size_t> from = checkpoint_at(plan.checkpoints, starts, plan.first, read);
    if (from || plan.max_rewinds == 0){
        return from;
    }
    const std::optional<size_t> first_backup = first_backup_checkpoint(plan.checkpoints, starts);
    if (!first_backup || plan.first < *first_backup){
        backend.log(std::format(
            "AutoStory: the screen ({}) matches no checkpoint start up to {}, and {}: no rewind",
            to_string(read), plan.checkpoints[plan.first].id, no_backup_before(plan.checkpoints, first_backup)
        ));
        return std::nullopt;
    }
    backend.log(std::format(
        "AutoStory: the screen ({}) matches no checkpoint start up to {}: rewinding to the backup save", to_string(read), plan.checkpoints[plan.first].id
    ));
    const std::variant<size_t, Pokemon::StoryFailure> back = rewind_to_backup(backend, plan.checkpoints, plan.first, starts, backup);
    if (const size_t* index = std::get_if<size_t>(&back)){
        return *index;
    }
    backend.log(std::format("AutoStory: no rewind: {}", std::get<Pokemon::StoryFailure>(back)));
    return std::nullopt;
}


std::optional<Pokemon::CheckpointFailure> run_plza_with_rewinds(
    Pokemon::StoryGame& game,
    RewindPlan& plan, const BackupSave& backup,
    PlzaBackend& backend, AutosaveSightings& sightings, Pokemon::RunState& state
){
    const std::span<StoryCheckpoint> checkpoints = plan.checkpoints;
    size_t from = plan.first;
    for (size_t rewinds = 0;; rewinds++){
        std::optional<Pokemon::CheckpointFailure> failure = run_plza_checkpoints(
            game, checkpoint_range(checkpoints, from, plan.last), plan.run_options, backend, state, sightings
        );
        if (!failure){
            return std::nullopt;
        }
        const std::string& failed_id = failure->checkpoint_id;
        const std::optional<std::string>& seen_in = sightings.seen_in();
        const std::optional<size_t> failed = checkpoint_index(checkpoints, failed_id);
        //  A catch autosaves where it lands and the badge can go unseen; the
        //  retry then fails as "rocks_not_left". Either says the start save is gone.
        if (!failed || (!seen_in && failure->failure.code != "rocks_not_left")){
            backend.log(std::format("[Rewind] {} failed with no autosave in it: no rewind", failed_id));
            return failure;
        }
        if (rewinds >= plan.max_rewinds){
            backend.log(std::format("[Rewind] {} failed after an autosave, but the run has rewound {} times already: stop", failed_id, rewinds));
            return failure;
        }
        std::variant<size_t, Pokemon::StoryFailure> back = rewind_after(game, backend, plan, backup, *failed, seen_in, rewinds + 1);
        if (Pokemon::StoryFailure* stopped = std::get_if<Pokemon::StoryFailure>(&back)){
            return Pokemon::CheckpointFailure{.checkpoint_id = failed_id, .failure = std::move(*stopped)};
        }
        from = std::get<size_t>(back);
        backend.log(std::format("[Rewind] on from {} to {}", checkpoints[from].id, checkpoints[plan.last].id));
        state = Pokemon::RunState{};
        sightings.restart();
    }
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
