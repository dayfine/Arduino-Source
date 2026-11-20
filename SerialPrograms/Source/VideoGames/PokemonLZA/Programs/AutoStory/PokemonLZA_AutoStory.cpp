/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "Actions/PokemonLZA_AutoStoryNewGame.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: from a new game to the start of Main Mission 01. "
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

//  Where saving_frame_before() saves its frames.
std::string start_minimaps_folder(){
    return std::format("{}AutoStoryDev/", USER_FILE_PATH());
}



//  `reload`, after the frame on screen is saved in start_minimaps_folder()
//  as plza-before-reload-<time>.png: a failed attempt that is retried
//  leaves no failure screenshot, only this.
std::function<Outcome(StoryBackend<ZonePose>& backend)> saving_frame_before(
    ConsoleHandle& console, std::function<Outcome(StoryBackend<ZonePose>& backend)> reload
){
    if (!reload){
        return reload;
    }
    return [&console, reload](StoryBackend<ZonePose>& backend){
        const VideoSnapshot snapshot = console.video().snapshot();
        if (snapshot){
            const std::string folder = start_minimaps_folder();
            Filesystem::create_directories(Filesystem::Path(folder));
            const std::string path = std::format("{}plza-before-reload-{}.png", folder, now_to_filestring());
            snapshot.frame->save(path);
            console.log(std::format("AutoStory: the frame before the reload saved as {}", path));
        }
        return reload(backend);
    };
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
    StoryGame<ZonePose> game = make_plza_story_game(story, story_data, sightings);
    game.reload = saving_frame_before(env.console, game.reload);
    RunState state;

    const SegmentRunOptions segments{
        .attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT,
        .reload_when_chained = story_data.reload_when_chained,
        .starts = builtin_checkpoint_starts(),
    };
    if (!story_row_starts_without_save(rows[first]) && LOAD_SAVE_FIRST){
        load_first_save(env.console, game, backend, rows[first]);
    }

    const std::vector<StoryCheckpoint> run_rows = story_run_rows<StoryCheckpoint>(rows, first, last);
    const SegmentReport report = run_plza_segments(game, run_rows, segments, backend, state, sightings);
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
