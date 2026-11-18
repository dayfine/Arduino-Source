/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryBackupSave.h"
#include "Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "Actions/PokemonLZA_AutoStoryMapWalk.h"
#include "Actions/PokemonLZA_AutoStoryNewGame.h"
#include "Detectors/PokemonLZA_AutoStoryStartScreenReader.h"
#include "PokemonLZA_AutoStoryBackups.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryConsoleGame.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;
using namespace std::chrono_literals;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        std::format("{} LZA", STRING_POKEMON), "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A. Prototype: from a new game to the end of Main Mission 01, picking Chikorita. "
        "Checkpoint 01.00a starts a new game (delete the save first) from the HOME screen (its tile among the first 8; a running game is closed) "
        "and makes the language, look and name below, up to Confirm Choices. 01.00b starts on Confirm Choices (made by the program or by hand), "
        "checks them and confirms. "
        "Later checkpoints load the latest save first, unless that is turned off.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


namespace{

//  The start and end dropdowns' entries: "01.00a New game choices", by id.
//  The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase entries;
        for (const StoryCheckpoint& row : builtin_story_data().checkpoints){
            entries.add_entry(StringSelectEntry(row.id, std::format("{} {}", row.id, row.title)));
        }
        return entries;
    }();
    return database;
}

//  Throws UserSetupError unless rows `first` to `last` are in a table of
//  `rows` rows, in order.
void check_checkpoint_range(ConsoleHandle& console, size_t first, size_t last, size_t rows){
    if (last >= rows){
        throw UserSetupError(console, "The checkpoint selection is out of range: pick the start and end checkpoints again.");
    }
    if (first > last){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
}

//  The game language dropdown's entries, in the language list's order.
const EnumDropdownDatabase<NewGameLanguage>& new_game_language_database(){
    static const EnumDropdownDatabase<NewGameLanguage> database{
        {NewGameLanguage::English,             "en",      "English"},
        {NewGameLanguage::SpanishSpain,        "es-ES",   "Espa\xc3\xb1ol (Espa\xc3\xb1" "a)"},
        {NewGameLanguage::SpanishLatinAmerica, "es-419",  "Espa\xc3\xb1ol (Latinoam\xc3\xa9rica)"},
        {NewGameLanguage::French,              "fr",      "Fran\xc3\xa7" "ais"},
        {NewGameLanguage::German,              "de",      "Deutsch"},
        {NewGameLanguage::Italian,             "it",      "Italiano"},
        {NewGameLanguage::Japanese,            "ja",      "Japanese"},
        {NewGameLanguage::Korean,              "ko",      "Korean (choices by hand, start at 01.00b)"},
        {NewGameLanguage::ChineseSimplified,   "zh-Hans", "Chinese (Simplified)"},
        {NewGameLanguage::ChineseTraditional,  "zh-Hant", "Chinese (Traditional)"},
    };
    return database;
}

//  Throws UserSetupError, before any press, if the run of `rows` plays a
//  new game that `choices` cannot: choices no new game takes
//  (new_game_problem()), or ones 01.00a cannot make (new_game_run_problem()).
void check_new_game_choices(ConsoleHandle& console, std::span<const StoryCheckpoint> rows, const NewGameChoices& choices){
    if (rows.empty() || !story_row_starts_without_save(rows.front())){
        return;
    }
    if (const std::optional<std::string> problem = new_game_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    if (const std::optional<std::string> problem = new_game_run_problem(rows, choices)){
        throw UserSetupError(console, *problem);
    }
}

//  Connects the controller for a run from `first_row`. A new game's row
//  starts on a screen that proves the save is fresh (01.00a: HOME, 01.00b:
//  Confirm Choices), so that screen is checked before any press, the
//  connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (story_row_makes_new_game_choices(first_row)){
        require_new_game_home(console, context);
    }else if (story_row_starts_without_save(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

constexpr std::string_view ATTEMPTS_PER_CHECKPOINT_LABEL =
    "<b>Attempts per Checkpoint:</b><br>A failed checkpoint that started from a save reloads it and tries again, up to this many times in all.";
constexpr std::string_view REWINDS_TO_BACKUP_LABEL =
    "<b>Rewinds to the Backup Save:</b><br>A checkpoint that fails after the game autosaved in it cannot reload its start. "
    "The run then closes the game, opens the backup save's card at the title (Up + B + X), and when the card matches a checkpoint's start "
    "(\"start_signature\" in its plan), loads it and plays on from that checkpoint, up to this many times. An unknown backup is never loaded.";

//  Loads the latest save for `row` with `game`: a run that starts after
//  the new game starts from it (01.01: the platform autosave), not from
//  wherever the game is. Throws OperationFailedException, with an error
//  report (the screenshot and the stream history's clip), if that save
//  does not load.
void load_first_save(ConsoleHandle& console, const StoryGame<ZonePose>& game, PlzaBackend& backend, const StoryCheckpoint& row){
    const Outcome loaded = load_checkpoint_start(game, backend, row);
    if (!loaded.succeeded()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", row.id, to_string(loaded)),
            console
        );
    }
    console.log(std::format("AutoStory: loaded the latest save for {}.", row.id));
}

//  How the start signature is read: the same reading on this many frames
//  in a row, one poll apart, within the frame budget.
constexpr size_t SIGNATURE_AGREEING_FRAMES = 3;
constexpr size_t SIGNATURE_FRAME_BUDGET = 20;
constexpr Milliseconds SIGNATURE_POLL = 500ms;

//  What the screen shows where the player stands (read_start_screen()):
//  the same reading on SIGNATURE_AGREEING_FRAMES frames in a row. Nothing
//  read when the frames never agree. Presses nothing.
StartSignature read_start_signature(ConsoleHandle& console, ProControllerContext& context){
    std::optional<StartSignature> last;
    size_t agreeing = 0;
    for (size_t frame = 0; frame < SIGNATURE_FRAME_BUDGET; frame++){
        context.wait_for(SIGNATURE_POLL);
        const VideoSnapshot snapshot = console.video().snapshot();
        const StartSignature read = snapshot ? read_start_screen(*snapshot.frame) : StartSignature{};
        agreeing = last == read ? agreeing + 1 : 1;
        last = read;
        if (agreeing >= SIGNATURE_AGREEING_FRAMES && read != StartSignature{}){
            return read;
        }
    }
    return StartSignature{};
}

//  Where the player stands: read_start_signature() and, when the overworld
//  reads, wild_zone from the player's pin on the full map (PLUS opens it,
//  B closes it: read_pin_on_map()).
StartSignature read_start_position(StoryEnv& story, PlzaBackend& backend){
    StartSignature read = read_start_signature(story.env.console, story.context);
    if (read != StartSignature{}){
        read.wild_zone = wild_zone_from_map(read_pin_on_map(story, backend));
    }
    return read;
}

//  The checkpoint a run asked to start at `plan.first` plays from: where the
//  screen and the map say the player is (run_start()), `first` or an
//  earlier checkpoint. Throws OperationFailedException, with a screenshot, as
//  "start_position_unknown" when the position matches no start: no
//  checkpoint's events run from the wrong place. When `plan.max_rewinds`
//  allows, an unknown start first rewinds to `backup` (start_or_rewind()).
size_t checked_first_row(StoryEnv& story, PlzaBackend& backend, const RewindPlan& plan, const BackupSave& backup){
    ConsoleHandle& console = story.env.console;
    const std::span<const StoryCheckpoint> rows = plan.rows;
    const size_t first = plan.first;
    const std::span<const CheckpointStart> starts = plan.starts;
    //  A row with no start signature runs as it is: no screen read, no map
    //  (03.11's start indoors, the user 2026-11-17: "we don't need a start
    //  check for this checkpoint").
    if (find_start(starts, rows[first].id) == nullptr){
        console.log(std::format("AutoStory: {} has no start signature: running it, no start check", rows[first].id));
        return first;
    }
    const StartSignature read = read_start_position(story, backend);
    const std::optional<size_t> from = start_or_rewind(backend, plan, read, backup);
    if (!from){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory: start_position_unknown: the screen ({}) matches no checkpoint start up to {}; nothing was run",
                to_string(read), rows[first].id
            ),
            console
        );
    }
    if (*from != first){
        console.log(std::format(
            "AutoStory: the save is at {}'s start ({}), not {}'s: running from {}", rows[*from].id, to_string(read), rows[first].id, rows[*from].id
        ));
    }else{
        console.log(std::format("AutoStory: the screen ({}) is {}'s start", to_string(read), rows[first].id));
    }
    return *from;
}

} // namespace


AutoStory::AutoStory()
    : RANGE("Checkpoints", checkpoint_database())
    , GAME_LANGUAGE(
        "<b>Game Language:</b><br>01.00a picks it on the new game's language list; 01.00b checks it on Confirm Choices. "
        "Korean: make the choices by hand up to Confirm Choices and start at 01.00b.",
        new_game_language_database(),
        LockMode::LOCK_WHILE_RUNNING,
        NewGameLanguage::English
    )
    , PLAYER_LOOK(
        "<b>Player Look:</b><br>01.00a picks it on the look picker: 1 to 4 along the top row, 5 to 8 along the bottom.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, 8
    )
    , PLAYER_NAME(
        /*is_password=*/false, "<b>Player Name:</b><br>Letters, digits and - / : ' , . ? ! (the keyboard's first page); at most 12 characters, 6 in Japanese and Chinese.",
        LockMode::LOCK_WHILE_RUNNING, /*default_value=*/"Kosmos", /*placeholder_text=*/"Kosmos"
    )
    , ATTEMPTS_PER_CHECKPOINT(std::string(ATTEMPTS_PER_CHECKPOINT_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/1, /*min_value=*/1, /*max_value=*/5)
    , REWINDS_TO_BACKUP(std::string(REWINDS_TO_BACKUP_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/2, /*min_value=*/0, /*max_value=*/5)
    , LOAD_SAVE_FIRST(
        "Load the save first: a run from a checkpoint after 01.00b loads the latest save before it starts. "
        "Off: it starts from wherever the game is, which must be that checkpoint's start.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RANGE);
    PA_ADD_OPTION(GAME_LANGUAGE);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(REWINDS_TO_BACKUP);
    PA_ADD_OPTION(LOAD_SAVE_FIRST);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const NewGameChoices new_game{.language = GAME_LANGUAGE, .look = PLAYER_LOOK, .name = PLAYER_NAME};
    const StoryData& story_data = builtin_story_data();
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RANGE.start_index();
    const size_t last = RANGE.end_index();
    check_checkpoint_range(env.console, first, last, rows.size());
    check_new_game_choices(env.console, std::span<const StoryCheckpoint>(rows).subspan(first, last - first + 1), new_game);
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{.env = env, .context = context, .battle_ai = BATTLE_AI, .new_game = new_game};
    PlzaConsoleBackend backend(story);
    AutosaveSightings sightings;
    const StoryGame<ZonePose> game = make_plza_story_game(story, story_data, sightings);
    RunState state;

    const SegmentRunOptions segments{
        .attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT,
        .reload_when_chained = story_data.reload_when_chained,
        .starts = builtin_checkpoint_starts(),
        .read_start = [&env, &context]{
            return read_start_signature(env.console, context);
        },
    };
    RewindPlan plan{
        .rows = rows, .first = first, .last = last, .segments = segments,
        .starts = builtin_checkpoint_starts(), .max_rewinds = REWINDS_TO_BACKUP,
    };
    if (!story_row_starts_without_save(rows[first])){
        if (LOAD_SAVE_FIRST){
            load_first_save(env.console, game, backend, rows[first]);
        }
        plan.first = checked_first_row(story, backend, plan, console_backup_save(story));
    }

    const SegmentReport report = run_plza_with_rewinds(game, plan, console_backup_save(story), backend, sightings, state);
    if (!report.completed){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure)
            ),
            env.console
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", rows[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
