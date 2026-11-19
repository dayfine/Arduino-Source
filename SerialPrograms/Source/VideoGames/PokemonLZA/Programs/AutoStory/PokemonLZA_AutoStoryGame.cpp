/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryPlatformWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "PokemonLZA_AutoStoryActionNames.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  01.01's first step waits for the platform; a new game has no save to check.
bool first_step_checks_start(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint_first_action(checkpoint) == action_name(PlzaAction::WAIT_FOR_PLATFORM) || checkpoint_starts_without_save(checkpoint);
}

//  Right after a load the platform reads on the first two looks.
constexpr size_t START_LOOKS = 4;


enum class StartCheck{
    PASSED,     //  its first step checks it
    FAILED,     //  the station platform holds: 01.01's start, not this checkpoint's
    UNCHECKED,  //  off the platform; nothing checks the checkpoint's own start yet
};

//  "[Boundary] <id> start: <pinned>": what pins the start ("pinned by
//  minimap", "unchecked"), the evidence for which start checks a run needs.
void log_start_boundary(PlzaBackend& backend, std::string_view id, std::string_view pinned){
    backend.log(std::format("[Boundary] {} start: {}", id, pinned));
}

//  Only the station platform is read so far.
StartCheck check_start(PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint){
    if (first_step_checks_start(checkpoint)){
        return StartCheck::PASSED;
    }
    return platform_holds_within(backend, START_LOOKS) ? StartCheck::FAILED : StartCheck::UNCHECKED;
}

} // namespace


Pokemon::StoryGame plza_story_game(const MarkedCheckpoints& marked, AutosaveSightings& sightings){
    return Pokemon::StoryGame{.id = "plza", .save = autosave_policy(marked, sightings)};
}

std::optional<Pokemon::StoryFailure> load_checkpoint_start(Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint){
    if (!game.reload){
        return Pokemon::StoryFailure{.code = "no_reload", .detail = "this game has no reload"};
    }
    backend.log(std::format("[Save] {}: loading the latest save", checkpoint.id));
    if (!game.reload(backend, checkpoint)){
        return Pokemon::StoryFailure{.code = "reload_failed", .detail = "the reload is at no checkpoint's start"};
    }
    switch (check_start(backend, checkpoint)){
    case StartCheck::PASSED:
        return std::nullopt;
    case StartCheck::FAILED:
        backend.log(std::format("[Checkpoint] {}: the save is on the station platform, not at its start", checkpoint.id));
        return Pokemon::StoryFailure{.code = "older_save", .detail = std::format(
            "the latest save is on the station platform (01.01's start), older than {}'s start", checkpoint.id
        )};
    case StartCheck::UNCHECKED:
        break;
    }
    backend.log(std::format("[Checkpoint] {}: off the station platform; START UNCHECKED: nothing reads this row's own start", checkpoint.id));
    return std::nullopt;
}

namespace{

//  What run_plza_checkpoints()'s steps between two checkpoints (the chain)
//  share. These steps serve only that run: split up to keep each short.
struct CheckpointRun{
    Pokemon::StoryGame& game;
    PlzaBackend& backend;
    CheckpointRunOptions& options;
    AutosaveSightings& sightings;
};

enum class Chain{
    ON,         //  from the live screen
    RELOAD,     //  from `checkpoint`'s start save
};

std::variant<Chain, Pokemon::StoryFailure> check_live_start(CheckpointRun& run, const Pokemon::StoryCheckpoint& checkpoint, bool seen){
    PlzaBackend& backend = run.backend;
    CheckpointRunOptions& options = run.options;
    const CheckpointStart* start = find_start(options.starts, checkpoint.id);
    if (start == nullptr || !options.read_start){
        backend.log(std::format(
            "[Checkpoint] {}: START UNCHECKED: {}; on from the live screen (autosave before it {})",
            checkpoint.id, start == nullptr ? "its plan names no start signature" : "nothing reads the screen", seen ? "seen" : "not seen"
        ));
        log_start_boundary(backend, checkpoint.id, seen ? "pinned by sighting" : "unchecked");
        return Chain::ON;
    }
    const StartSignature read = options.read_start(start->signature);
    const std::string screen = std::format("the screen reads ({}), its start is ({})", to_string(read), to_string(start->signature));
    if (start_confirmed(start->signature, read)){
        backend.log(std::format("[Checkpoint] {}: START CHECKED: {}; on from the live screen", checkpoint.id, screen));
        log_start_boundary(backend, checkpoint.id, "pinned by minimap");
        return Chain::ON;
    }
    backend.log(std::format("[Checkpoint] {}: START CHECK FAILED: {}; nothing run from there", checkpoint.id, screen));
    return Pokemon::StoryFailure{.code = "chain_off_start", .detail = std::format("the run is not at {}'s start: {}", checkpoint.id, screen)};
}

//  `seen`: whether the checkpoint before's autosave showed. Fails when only a
//  reload would do and there is no save to load.
std::variant<Chain, Pokemon::StoryFailure> decide_chain(CheckpointRun& run, const Pokemon::StoryCheckpoint& checkpoint, bool seen){
    PlzaBackend& backend = run.backend;
    const auto no_save = [&](std::string_view why){
        backend.log(std::format("[Checkpoint] {}: no save at its start: {}", checkpoint.id, why));
        return Pokemon::StoryFailure{.code = "no_start_save", .detail = std::format(
            "no save at {}'s start: {}; a reload would rewind to an older save", checkpoint.id, why
        )};
    };
    switch (check_start(backend, checkpoint)){
    case StartCheck::PASSED:
        backend.log(std::format("[Checkpoint] {}: start check passed; on from the live screen", checkpoint.id));
        log_start_boundary(backend, checkpoint.id, "pinned by platform");
        return Chain::ON;
    case StartCheck::UNCHECKED:
        return check_live_start(run, checkpoint, seen);
    case StartCheck::FAILED:
        break;
    }
    if (!seen){
        return no_save("the start check failed and the autosave before it was not seen");
    }
    backend.log(std::format("[Checkpoint] {}: start check failed; reloads its start save", checkpoint.id));
    return Chain::RELOAD;
}

std::optional<Pokemon::StoryFailure> reload_into(
    CheckpointRun& run, const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& checkpoint
){
    PlzaBackend& backend = run.backend;
    CheckpointRunOptions& options = run.options;
    backend.log(std::format(
        "[Checkpoint] {}: reloads its start save when chained, whether or not {}'s autosave showed at its end ({})",
        checkpoint.id, previous.id, run.sightings.last_end_seen() ? "seen" : "not seen"
    ));
    const std::optional<Pokemon::StoryFailure> control = check_can_walk(backend, std::format("the reload into {}", checkpoint.id), WalkStart::UNKNOWN);
    if (control){
        backend.log(std::format("[Checkpoint] {}: control never came back after {}: no reload", checkpoint.id, previous.id));
        return control;
    }
    backend.log(std::format("[Checkpoint] {}: control is back after {}'s last scene; reloading the latest save", checkpoint.id, previous.id));
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(run.game, backend, checkpoint);
    if (loaded){
        return loaded;
    }
    const CheckpointStart* start = find_start(options.starts, checkpoint.id);
    if (start == nullptr || !options.read_start){
        const std::string why = start == nullptr ? "its plan names no start signature" : "nothing reads the screen";
        backend.log(std::format("[Checkpoint] {}: where the reload landed is not checked ({}); nothing pressed after the reload", checkpoint.id, why));
        return Pokemon::StoryFailure{.code = "reload_unchecked", .detail = std::format(
            "{}: where the reload landed is not checked: {}; nothing pressed after the reload", checkpoint.id, why
        )};
    }
    const StartSignature read = options.read_start(start->signature);
    if (!checkpoint_at(std::span<const Pokemon::StoryCheckpoint>(&checkpoint, 1), std::span<const CheckpointStart>(start, 1), /*asked=*/0, read)){
        const std::string where = std::format(
            "the screen reads ({}), the start is ({}); nothing pressed after the reload", to_string(read), to_string(start->signature)
        );
        backend.log(std::format("[Checkpoint] {}: the reload landed somewhere other than its start: {}", checkpoint.id, where));
        return Pokemon::StoryFailure{.code = "reload_off_start", .detail = std::format(
            "the reload into {} landed somewhere other than its start: {}", checkpoint.id, where
        )};
    }
    backend.log(std::format("[Checkpoint] {}: the reload landed at its start ({})", checkpoint.id, to_string(read)));
    log_start_boundary(backend, checkpoint.id, "pinned by minimap");
    return std::nullopt;
}

//  A reload starts a fresh `state`.
std::optional<Pokemon::StoryFailure> chain_into(
    CheckpointRun& run, const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& checkpoint, Pokemon::RunState& state
){
    const bool reload = run.options.reload_when_chained.contains(checkpoint.id);
    const std::variant<Chain, Pokemon::StoryFailure> chain = reload
        ? std::variant<Chain, Pokemon::StoryFailure>(Chain::RELOAD)
        : decide_chain(run, checkpoint, run.sightings.last_end_seen());
    if (const Pokemon::StoryFailure* failure = std::get_if<Pokemon::StoryFailure>(&chain)){
        return *failure;
    }
    if (std::get<Chain>(chain) == Chain::ON){
        return std::nullopt;
    }
    const std::optional<Pokemon::StoryFailure> loaded = reload
        ? reload_into(run, previous, checkpoint)
        : load_checkpoint_start(run.game, run.backend, checkpoint);
    if (!loaded){
        state = Pokemon::RunState{};
    }
    if (!loaded && !reload){
        //  The start save is the autosave seen at the checkpoint before's end.
        log_start_boundary(run.backend, checkpoint.id, "pinned by sighting");
    }
    return loaded;
}

//  After `checkpoints[index]` passed: the next checkpoint, or with a Reload Probe after
//  it (once), the checkpoint whose start the reload landed at.
size_t checkpoint_after(
    CheckpointRun& run, std::span<const Pokemon::StoryCheckpoint> checkpoints, size_t index, Pokemon::RunState& state, std::set<std::string>& probed
){
    Pokemon::StoryGame& game = run.game;
    PlzaBackend& backend = run.backend;
    CheckpointRunOptions& options = run.options;
    const size_t next = index + 1;
    const std::string& passed = checkpoints[index].id;
    if (next >= checkpoints.size() || !game.reload || !options.reload_probe_after.contains(passed) || !probed.insert(passed).second){
        return next;
    }
    if (!game.reload(backend, checkpoints[next])){
        backend.log(std::format("[ReloadProbe] after {}: the reload failed; on to {}", passed, checkpoints[next].id));
        return next;
    }
    state = Pokemon::RunState{};
    if (options.log_landing){
        options.log_landing(checkpoints[index]);
    }
    const CheckpointStart* start = find_start(options.starts, checkpoints[next].id);
    const std::optional<size_t> from = start == nullptr || !options.read_start
        ? std::nullopt
        : checkpoint_at(checkpoints, options.starts, next, options.read_start(start->signature));
    backend.log(std::format(
        "[ReloadProbe] after {}: on from {}", passed, from ? checkpoints[*from].id : std::format("{}, no start matched", checkpoints[next].id)
    ));
    return from.value_or(next);
}

} // namespace

std::optional<Pokemon::CheckpointFailure> run_plza_checkpoints(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> checkpoints, CheckpointRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
){
    CheckpointRun run{.game = game, .backend = backend, .options = options, .sightings = sightings};
    std::set<std::string> probed;
    for (size_t index = 0; index < checkpoints.size(); index = checkpoint_after(run, checkpoints, index, state, probed)){
        Pokemon::StoryCheckpoint& checkpoint = checkpoints[index];
        const bool start_saved = index == 0 || options.reload_when_chained.contains(checkpoint.id) || sightings.last_end_seen();
        if (index > 0){
            if (std::optional<Pokemon::StoryFailure> chained = chain_into(run, checkpoints[index - 1], checkpoint, state)){
                backend.log(std::format("[Event] {}/0 chain: failed: {}", checkpoint.id, *chained));
                return Pokemon::CheckpointFailure{.checkpoint_id = checkpoint.id, .failure = std::move(*chained)};
            }
        }
        if (options.at_start){
            options.at_start(checkpoint);
        }
        if (!start_saved && options.attempts_per_checkpoint > 1){
            backend.log(std::format("[Checkpoint] {}: no autosave seen at its start, so no reload if it fails", checkpoint.id));
        }
        sightings.restart();
        Pokemon::StoryGame retry{
            .id = game.id,
            .save = [&game](PlzaBackend& on, const Pokemon::StoryCheckpoint& passed) -> std::optional<Pokemon::StoryFailure>{
                return game.save ? game.save(on, passed) : std::nullopt;
            },
            .attempts_per_checkpoint = start_saved ? options.attempts_per_checkpoint : 1,
        };
        if (game.reload){
            retry.reload = reloading_only_to_start(game, sightings);
        }
        if (std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, checkpoints.subspan(index, 1), retry, state)){
            log_moved_save(backend, sightings, checkpoint.id);
            return failure;
        }
    }
    return std::nullopt;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
