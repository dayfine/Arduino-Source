/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryNewGame.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        std::format("{} LZA", STRING_POKEMON), "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A. Prototype: from a new game to the end of Main Mission 01, picking Chikorita. "
        "Checkpoint 01.00 starts a new game (delete the save first) from the HOME screen with the game closed and its tile selected, "
        "from the language list, or from Confirm Choices, with the language, look and name below. "
        "Later checkpoints load the latest save first, unless that is turned off.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


namespace{

//  The start and end dropdowns' entries: "01.00 New game from ...", by id.
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
        {NewGameLanguage::Korean,              "ko",      "Korean (not supported yet)"},
        {NewGameLanguage::ChineseSimplified,   "zh-Hans", "Chinese (Simplified)"},
        {NewGameLanguage::ChineseTraditional,  "zh-Hant", "Chinese (Traditional)"},
    };
    return database;
}

//  Throws UserSetupError, before any press, if a run from `first_row` plays
//  a new game that `choices` cannot.
void check_new_game_choices(ConsoleHandle& console, const StoryCheckpoint& first_row, const NewGameChoices& choices){
    if (!story_row_starts_without_save(first_row)){
        return;
    }
    if (const std::optional<std::string> problem = new_game_problem(choices)){
        throw UserSetupError(console, *problem);
    }
}

//  Connects the controller for a run from `first_row`. 01.00 starts on a
//  screen that proves the save is fresh, so it is checked before any press,
//  the connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (story_row_starts_without_save(first_row)){
        require_new_game_start(console, context);
    }
    require_player(console, context, BUTTON_L);
}

} // namespace


AutoStory::AutoStory()
    : RANGE("Checkpoints", checkpoint_database())
    , GAME_LANGUAGE(
        "<b>Game Language:</b><br>01.00 picks it on the new game's language list.",
        new_game_language_database(),
        LockMode::LOCK_WHILE_RUNNING,
        NewGameLanguage::English
    )
    , PLAYER_LOOK(
        "<b>Player Look:</b><br>01.00 picks it on the look picker: 1 to 4 along the top row, 5 to 8 along the bottom.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, 8
    )
    , PLAYER_NAME(
        /*is_password=*/false, "<b>Player Name:</b><br>Letters, digits and - / : ' , . ? ! (the keyboard's first page); at most 12 characters, 6 in Japanese and Chinese.",
        LockMode::LOCK_WHILE_RUNNING, /*default_value=*/"Kosmos", /*placeholder_text=*/"Kosmos"
    )
    , CAPTURE_INTRO_SCREENS(
        "<b>Developer: capture intro screens:</b><br>Instead of the story: for each language, close the game, launch it from HOME and save its screens up to the name keyboard to the Screenshots folder. Nothing is typed, so no save is made.",
        LockMode::LOCK_WHILE_RUNNING, /*default_value=*/false
    )
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per Checkpoint:</b><br>A failed checkpoint that started from a save reloads it and tries again, up to this many times in all.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, 5
    )
    , LOAD_SAVE_FIRST(
        "Load the save first: a run from a checkpoint after 01.00 loads the latest save before it starts. "
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
    PA_ADD_OPTION(CAPTURE_INTRO_SCREENS);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(LOAD_SAVE_FIRST);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (CAPTURE_INTRO_SCREENS){
        require_player(env.console, context, BUTTON_L);
        capture_intro_screens(env.console, context, PLAYER_LOOK);
        return;
    }
    const NewGameChoices new_game{.language = GAME_LANGUAGE, .look = PLAYER_LOOK, .name = PLAYER_NAME};
    const StoryData& story_data = builtin_story_data();
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RANGE.start_index();
    const size_t last = RANGE.end_index();
    check_checkpoint_range(env.console, first, last, rows.size());
    check_new_game_choices(env.console, rows[first], new_game);
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{.env = env, .context = context, .battle_ai = BATTLE_AI, .new_game = new_game};
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(story, story_data);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;

    //  A run that starts after the new game starts from the latest save
    //  (01.01: the platform autosave), not from wherever the game is, and
    //  stops if that save is older than its start; unless LOAD_SAVE_FIRST
    //  is off.
    if (!story_row_starts_without_save(rows[first]) && LOAD_SAVE_FIRST){
        const Outcome loaded = load_checkpoint_start(game, backend, rows[first]);
        if (!loaded.succeeded()){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format("AutoStory: could not load the save for {}: {}", rows[first].id, to_string(loaded))
            );
        }
        env.console.log(std::format("AutoStory: loaded the latest save for {}.", rows[first].id));
    }

    const std::vector<StoryCheckpoint> checkpoints = story_run_rows(std::span<const StoryCheckpoint>(rows), first, last);
    const SegmentReport report = run_plza_segments(game, story_data, checkpoints, ATTEMPTS_PER_CHECKPOINT, engine, backend, state);
    if (!report.completed){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure)
            )
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", rows[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
