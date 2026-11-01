#include <algorithm>
#include <format>
#include <memory>

#include "PokemonBDSP_AutoStory.h"
#include "Data/PokemonBDSP_AutoStory_MapFiles.h"
#include "Utils/PokemonBDSP_AutoStory_NewGame.h"

#include "CommonFramework/Globals.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"

#include "StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
#include "StorySegments/PokemonBDSP_AutoStory_Segment_01.h"
#include "StorySegments/PokemonBDSP_AutoStory_SandgemCenterEvents.h"

#include "CommonTools/Options/StringSelectOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


// ---------------------------------------------------------------------------
//  Segment list
// ---------------------------------------------------------------------------

static std::vector<std::unique_ptr<AutoStory_Segment>> make_segment_list(){
    std::vector<std::unique_ptr<AutoStory_Segment>> segment_list;
    segment_list.emplace_back(std::make_unique<AutoStory_Segment_00>());
    segment_list.emplace_back(std::make_unique<AutoStory_Segment_01>());
    return segment_list;
}

static const std::vector<std::unique_ptr<AutoStory_Segment>>& ALL_SEGMENTS(){
    static std::vector<std::unique_ptr<AutoStory_Segment>> list = make_segment_list();
    return list;
}
static std::vector<std::unique_ptr<AutoStory_Checkpoint>> make_checkpoint_list(){
    std::vector<std::unique_ptr<AutoStory_Checkpoint>> checkpoint_list;

    //Segment 00
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0000>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0001>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0002>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0003>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0004>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0005>());

    //Segment 01
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0101>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0102>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0103>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0104>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0105>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0106>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0107>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0108>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0109>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0110>());
    checkpoint_list.emplace_back(std::make_unique<AutoStory_Checkpoint_0111>());

    return checkpoint_list;
}

static const std::vector<std::unique_ptr<AutoStory_Checkpoint>>& ALL_CHECKPOINTS(){
    static std::vector<std::unique_ptr<AutoStory_Checkpoint>> list = make_checkpoint_list();
    return list;
}

// ---------------------------------------------------------------------------
//  Segment database
// ---------------------------------------------------------------------------

static const StringSelectDatabase& section_database(bool checkpoints){
    static const StringSelectDatabase segments = []{
        StringSelectDatabase db;
        for (size_t i = 0; i < ALL_SEGMENTS().size(); i++)
            db.add_entry(StringSelectEntry(std::to_string(i), ALL_SEGMENTS()[i]->name()));
        return db;
    }();
    static const StringSelectDatabase checkpoint_db = []{
        StringSelectDatabase db;
        for (size_t i = 0; i < ALL_CHECKPOINTS().size(); i++)
            db.add_entry(StringSelectEntry(std::to_string(i), ALL_CHECKPOINTS()[i]->name()));
        return db;
    }();
    return checkpoints ? checkpoint_db : segments;
}

// ---------------------------------------------------------------------------
//  Descriptor
// ---------------------------------------------------------------------------

BDSPAutoStory_Descriptor::BDSPAutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
          "PokemonBDSP:AutoStory",
          "Pokemon BDSP",
          "Auto Story",
          "Programs/PokemonBDSP/AutoStory.html",
          "Automatically completes BDSP story segments.",
          PanelQualityStanding::OFFICIAL_BETA,
          ProgramControllerClass::StandardController_NoRestrictions,
          FeedbackType::REQUIRED,
          AllowCommandsWhenRunning::DISABLE_COMMANDS,
          {"Tesseract"}
          )
{}



std::unique_ptr<StatsTracker>
BDSPAutoStory_Descriptor::make_stats() const{
    return std::make_unique<AutoStoryStats>();
}


// ---------------------------------------------------------------------------
//  Program Instance
// ---------------------------------------------------------------------------

BDSPAutoStory::~BDSPAutoStory(){
    OUTSIDE_HELP_POKEMON.remove_listener(*this);
    HELP_MODE.remove_listener(*this);
    START_CHECKPOINT_TUTORIAL.remove_listener(*this);
    END_CHECKPOINT_TUTORIAL.remove_listener(*this);
    START_SEGMENT.remove_listener(*this);
    END_SEGMENT.remove_listener(*this);
    ENABLE_ADVANCED_MODE.remove_listener(*this);


}


// ---------------------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------------------

BDSPAutoStory::BDSPAutoStory()
    : SingleSwitchProgramInstance()
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , STOP_AT_FIRST_FAILURE(
          "<b>Lab: Stop at the first failure</b><br>"
          "Save the failure screenshot and stop, leaving the game where it failed for a look, "
          "instead of closing it and retrying. Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING,
          false
      )
    , DIAGNOSTIC_POST_CATCH_ONLY(
          "<b>Diagnostic only: Run 00.04 after catch through 00.05</b><br>"
          "Start at the saved position immediately after catching the second Pokemon. "
          "Applies only in advanced Tutorial mode when Start Checkpoint is 00.04. Runs its post-catch part, then 00.05. Other starting checkpoints run normally. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_FAKE_CHECKPOINT_SAVE(
          "<b>Diagnostic only: Fake checkpoint saves</b><br>"
          "Use the fake save helper instead of normal checkpoint saving. "
          "Use an existing in-game save at the selected starting checkpoint or segment. "
          "Any failure reloads that save and reruns the entire selected range from its start. "
          "Checkpoint saves are not verified in this mode. Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_ROUTE202_TO_SANDGEM_CENTER(
          "<b>Diagnostic only: Route 202 to Sandgem's Pokemon Center (events)</b><br>"
          "Start at Route 202's save spot under the south ledge. Walks into Sandgem Town to the front of the "
          "Pokemon Center's door, facing it, and stops there: no heal, no save. Runs only this, whatever the other options say "
          "(with the Mart lady option too, runs both in turn). "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_SANDGEM_MART_LADY(
          "<b>Diagnostic only: Sandgem's Pokemon Center to the Poke Mart lady and back to Route 202 (events)</b><br>"
          "Start in front of Sandgem's Pokemon Center door. Reads Route 201's border sign, talks to the Poke Mart lady, reads the sign "
          "again, goes back to the Center's door, then to Route 202's save spot under the south ledge and saves there. "
          "With the Route 202 option too, runs that first, from its Route 202 save. Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_CENTER(
          "<b>Diagnostic only: 00.05a Jubilife, steps to the Pokemon Center (events)</b><br>"
          "Start at 00.05's save below Jubilife's steps. Walks to the Pokemon Center and heals; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_SCHOOL(
          "<b>Diagnostic only: 00.05b Jubilife, Pokemon Center to the Trainers' School (events)</b><br>"
          "Start on the Pokemon Center's doorstep (or after 00.05a). Walks into the Trainers' School; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_BARRY(
          "<b>Diagnostic only: 00.05c Jubilife, Barry in the Trainers' School (events)</b><br>"
          "Start inside the School at its door (or after 00.05b). Talks to Barry for the Town Map and walks out; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_PRESIDENT(
          "<b>Diagnostic only: 00.05d Jubilife, the Poketch Company president (events)</b><br>"
          "Start on the Trainers' School's doorstep (or after 00.05c). Walks into the road until the president talks; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_CLOWN_MART(
          "<b>Diagnostic only: 00.05e Jubilife, the clown by the Poke Mart (events)</b><br>"
          "Start where the president leaves us (or after 00.05d). Walks to the clown and gets Coupon 1; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_CLOWN_COMPANY(
          "<b>Diagnostic only: 00.05f Jubilife, the clown by the Poketch Company (events)</b><br>"
          "Start beside the TV station clown (after his coupon). Walks to the third clown and gets Coupon 3; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    , DIAGNOSTIC_JUBILIFE_POKETCH(
          "<b>Diagnostic only: 00.05g Jubilife, the Poketch from the president (events)</b><br>"
          "Start in the gap beside the Poketch Company's sign, with the three coupons (after 00.05f). "
          "Gets the Poketch and goes up to the street east to Route 203; no save. "
          "Remove before pushing to production.",
          LockMode::LOCK_WHILE_RUNNING, false)
    , ENABLE_ADVANCED_MODE(
          "<b>Advanced mode:</b><br>"
          "Use checkpoints instead of segments.",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , START_CHECKPOINT_TUTORIAL(
          "<b>Start Checkpoint:</b>",
          section_database(true),
          LockMode::LOCK_WHILE_RUNNING,
          "0"
          )
    , END_CHECKPOINT_TUTORIAL(
          "<b>End Checkpoint:</b>",
          section_database(true),
          LockMode::UNLOCK_WHILE_RUNNING,
          "5"
          )

    , START_DESCRIPTION_SEGMENT("<b>Start:</b><br>", "")
    , END_DESCRIPTION_SEGMENT("<b>End:</b><br>", "")

    , START_DESCRIPTION_CHECKPOINT("<b>Start:</b><br>", "")
    , END_DESCRIPTION_CHECKPOINT("<b>End:</b><br>", "")

    , START_SEGMENT(
        "<b>Start Segment:</b>",
        section_database(false),
        LockMode::LOCK_WHILE_RUNNING,
        "0"
    )

    , END_SEGMENT(
        "<b>End Segment:</b>",
        section_database(false),
        LockMode::UNLOCK_WHILE_RUNNING,
        "0"
    )

    , NEW_GAME_INTRO(
          "<b>Start a new game first:</b><br>"
          "For a game with no save: restart it and play the intro (language, appearance, names) "
          "until the player is in control in the bedroom. Runs before the selected range.",
          LockMode::LOCK_WHILE_RUNNING, false)
    , PLAYER_NAME(false, "<b>Player name:</b><br>For a new game. Letters and digits.", LockMode::LOCK_WHILE_RUNNING, "Kosmos", "Kosmos")
    , STARTERCHOICE(
          "<b>Starter Pokemon:</b>",
          {
           {StarterChoice::TURTWIG,  "turtwig",  "Turtwig"},
           {StarterChoice::CHIMCHAR, "chimchar", "Chimchar"},
           {StarterChoice::PIPLUP,   "piplup",   "Piplup"},
           },
          LockMode::LOCK_WHILE_RUNNING,
          StarterChoice::CHIMCHAR
          )

    , STARTER_SHINY(
          "<b>Shiny Starter:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )

    , CATCH_LEGENDARY(
          "<b>Catch Box Legendary:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )

    , HELP_MODE(
          "<b>Help Mode:</b>",
          {
              {HelpMode::NO_OUTSIDE_HELP, "no_outside_help", "No Outside Help"},
              {HelpMode::OUTSIDE_HELP,    "outside_help",    "Outside Help"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          HelpMode::NO_OUTSIDE_HELP
          )

    , OUTSIDE_HELP_POKEMON(
          "<b>Pokemon:</b>",
          {
              {OutsideHelpPokemon::TORTERRA, "torterra", "Torterra"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          OutsideHelpPokemon::TORTERRA
          )

    , OUTSIDE_HELP_ATTACK_1(
          "<b>Attack 1:</b>",
          {
              {TorterraMove::EARTHQUAKE,  "earthquake",  "Earthquake"},
              {TorterraMove::WOOD_HAMMER, "wood_hammer", "Wood Hammer"},
              {TorterraMove::RAZOR_LEAF,  "razor_leaf",  "Razor Leaf"},
              {TorterraMove::CRUNCH,      "crunch",      "Crunch"},
              {TorterraMove::GIGA_DRAIN,  "giga_drain",  "Giga Drain"},
              {TorterraMove::IRON_TAIL,   "iron_tail",   "Iron Tail"},
              {TorterraMove::ROCK_TOMB,   "rock_tomb",   "Rock Tomb"},
              {TorterraMove::FACADE,      "facade",      "Facade"},
              {TorterraMove::STONE_EDGE,  "stone_edge",  "Stone Edge"},
              {TorterraMove::ROCK_SLIDE,  "rock_slide",  "Rock Slide"},
              {TorterraMove::BULLDOZE,    "bulldoze",    "Bulldoze"},
              {TorterraMove::STRENGTH,    "strength",    "Strength"},
              {TorterraMove::ROCK_SMASH,  "rock_smash",  "Rock Smash"},
              {TorterraMove::ROCK_CLIMB,  "rock_climb",  "Rock Climb"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          TorterraMove::EARTHQUAKE
          )

    , OUTSIDE_HELP_ATTACK_2(
          "<b>Attack 2:</b>",
          {
              {TorterraMove::EARTHQUAKE,  "earthquake",  "Earthquake"},
              {TorterraMove::WOOD_HAMMER, "wood_hammer", "Wood Hammer"},
              {TorterraMove::RAZOR_LEAF,  "razor_leaf",  "Razor Leaf"},
              {TorterraMove::CRUNCH,      "crunch",      "Crunch"},
              {TorterraMove::GIGA_DRAIN,  "giga_drain",  "Giga Drain"},
              {TorterraMove::IRON_TAIL,   "iron_tail",   "Iron Tail"},
              {TorterraMove::ROCK_TOMB,   "rock_tomb",   "Rock Tomb"},
              {TorterraMove::FACADE,      "facade",      "Facade"},
              {TorterraMove::STONE_EDGE,  "stone_edge",  "Stone Edge"},
              {TorterraMove::ROCK_SLIDE,  "rock_slide",  "Rock Slide"},
              {TorterraMove::BULLDOZE,    "bulldoze",    "Bulldoze"},
              {TorterraMove::STRENGTH,    "strength",    "Strength"},
              {TorterraMove::ROCK_SMASH,  "rock_smash",  "Rock Smash"},
              {TorterraMove::ROCK_CLIMB,  "rock_climb",  "Rock Climb"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          TorterraMove::ROCK_SLIDE
          )

    , OUTSIDE_HELP_ATTACK_3(
          "<b>Attack 3:</b>",
          {
              {TorterraMove::EARTHQUAKE,  "earthquake",  "Earthquake"},
              {TorterraMove::WOOD_HAMMER, "wood_hammer", "Wood Hammer"},
              {TorterraMove::RAZOR_LEAF,  "razor_leaf",  "Razor Leaf"},
              {TorterraMove::CRUNCH,      "crunch",      "Crunch"},
              {TorterraMove::GIGA_DRAIN,  "giga_drain",  "Giga Drain"},
              {TorterraMove::IRON_TAIL,   "iron_tail",   "Iron Tail"},
              {TorterraMove::ROCK_TOMB,   "rock_tomb",   "Rock Tomb"},
              {TorterraMove::FACADE,      "facade",      "Facade"},
              {TorterraMove::STONE_EDGE,  "stone_edge",  "Stone Edge"},
              {TorterraMove::ROCK_SLIDE,  "rock_slide",  "Rock Slide"},
              {TorterraMove::BULLDOZE,    "bulldoze",    "Bulldoze"},
              {TorterraMove::STRENGTH,    "strength",    "Strength"},
              {TorterraMove::ROCK_SMASH,  "rock_smash",  "Rock Smash"},
              {TorterraMove::ROCK_CLIMB,  "rock_climb",  "Rock Climb"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          TorterraMove::WOOD_HAMMER
          )

    , OUTSIDE_HELP_ATTACK_4(
          "<b>Attack 4:</b>",
          {
              {TorterraMove::EARTHQUAKE,  "earthquake",  "Earthquake"},
              {TorterraMove::WOOD_HAMMER, "wood_hammer", "Wood Hammer"},
              {TorterraMove::RAZOR_LEAF,  "razor_leaf",  "Razor Leaf"},
              {TorterraMove::CRUNCH,      "crunch",      "Crunch"},
              {TorterraMove::GIGA_DRAIN,  "giga_drain",  "Giga Drain"},
              {TorterraMove::IRON_TAIL,   "iron_tail",   "Iron Tail"},
              {TorterraMove::ROCK_TOMB,   "rock_tomb",   "Rock Tomb"},
              {TorterraMove::FACADE,      "facade",      "Facade"},
              {TorterraMove::STONE_EDGE,  "stone_edge",  "Stone Edge"},
              {TorterraMove::ROCK_SLIDE,  "rock_slide",  "Rock Slide"},
              {TorterraMove::BULLDOZE,    "bulldoze",    "Bulldoze"},
              {TorterraMove::STRENGTH,    "strength",    "Strength"},
              {TorterraMove::ROCK_SMASH,  "rock_smash",  "Rock Smash"},
              {TorterraMove::ROCK_CLIMB,  "rock_climb",  "Rock Climb"},
          },
          LockMode::LOCK_WHILE_RUNNING,
          TorterraMove::CRUNCH
          )

    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(30))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })

{
    // -----------------------------------------------------------------------
    //  Options
    // -----------------------------------------------------------------------

    PA_ADD_OPTION(STOP_AT_FIRST_FAILURE);
    PA_ADD_OPTION(DIAGNOSTIC_POST_CATCH_ONLY);
    PA_ADD_OPTION(DIAGNOSTIC_FAKE_CHECKPOINT_SAVE);
    PA_ADD_OPTION(DIAGNOSTIC_ROUTE202_TO_SANDGEM_CENTER);
    PA_ADD_OPTION(DIAGNOSTIC_SANDGEM_MART_LADY);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_CENTER);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_SCHOOL);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_BARRY);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_PRESIDENT);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_CLOWN_MART);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_CLOWN_COMPANY);
    PA_ADD_OPTION(DIAGNOSTIC_JUBILIFE_POKETCH);
    PA_ADD_OPTION(ENABLE_ADVANCED_MODE);

    // Segment UI
    PA_ADD_OPTION(START_SEGMENT);
    PA_ADD_OPTION(START_DESCRIPTION_SEGMENT);

    PA_ADD_OPTION(END_SEGMENT);
    PA_ADD_OPTION(END_DESCRIPTION_SEGMENT);

    // Checkpoint UI
    PA_ADD_OPTION(START_CHECKPOINT_TUTORIAL);
    PA_ADD_OPTION(START_DESCRIPTION_CHECKPOINT);

    PA_ADD_OPTION(END_CHECKPOINT_TUTORIAL);
    PA_ADD_OPTION(END_DESCRIPTION_CHECKPOINT);


    PA_ADD_OPTION(NEW_GAME_INTRO);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(STARTERCHOICE);
    PA_ADD_OPTION(STARTER_SHINY);
    PA_ADD_OPTION(CATCH_LEGENDARY);

    PA_ADD_OPTION(HELP_MODE);
    PA_ADD_OPTION(OUTSIDE_HELP_POKEMON);

    PA_ADD_OPTION(OUTSIDE_HELP_ATTACK_1);
    PA_ADD_OPTION(OUTSIDE_HELP_ATTACK_2);
    PA_ADD_OPTION(OUTSIDE_HELP_ATTACK_3);
    PA_ADD_OPTION(OUTSIDE_HELP_ATTACK_4);

    PA_ADD_OPTION(NOTIFICATIONS);

    HELP_MODE.add_listener(*this);
    OUTSIDE_HELP_POKEMON.add_listener(*this);
    START_CHECKPOINT_TUTORIAL.add_listener(*this);
    END_CHECKPOINT_TUTORIAL.add_listener(*this);
    ENABLE_ADVANCED_MODE.add_listener(*this);
    START_SEGMENT.add_listener(*this);
    END_SEGMENT.add_listener(*this);
    on_config_value_changed(nullptr);
}
void BDSPAutoStory::update_ui_state(){
    const bool advanced = ENABLE_ADVANCED_MODE;
    auto visible = [](ConfigOption& option, bool show){
        option.set_visibility(show ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN);
    };
    visible(START_SEGMENT, !advanced);
    visible(END_SEGMENT, !advanced);
    visible(START_CHECKPOINT_TUTORIAL, advanced);
    visible(END_CHECKPOINT_TUTORIAL, advanced);
    visible(START_DESCRIPTION_SEGMENT, !advanced);
    visible(END_DESCRIPTION_SEGMENT, !advanced);
    visible(START_DESCRIPTION_CHECKPOINT, advanced);
    visible(END_DESCRIPTION_CHECKPOINT, advanced);

    bool show_help = ((HelpMode)HELP_MODE == HelpMode::OUTSIDE_HELP);

    OUTSIDE_HELP_POKEMON.set_visibility(
        show_help ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN
    );

    OUTSIDE_HELP_ATTACK_1.set_visibility(show_help ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN);
    OUTSIDE_HELP_ATTACK_2.set_visibility(show_help ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN);
    OUTSIDE_HELP_ATTACK_3.set_visibility(show_help ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN);
    OUTSIDE_HELP_ATTACK_4.set_visibility(show_help ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN);
}
void BDSPAutoStory::on_config_value_changed(void*){
    if (ENABLE_ADVANCED_MODE){
        const auto& checkpoints = ALL_CHECKPOINTS();

        size_t start = std::stoul(START_CHECKPOINT_TUTORIAL.slug());
        size_t end   = std::stoul(END_CHECKPOINT_TUTORIAL.slug());

        if (start < checkpoints.size()){
            START_DESCRIPTION_CHECKPOINT.set_text(
                "Start: " + checkpoints[start]->start_text()
            );
        }
        if (end < checkpoints.size()){
            END_DESCRIPTION_CHECKPOINT.set_text(
                "End: " + checkpoints[end]->end_text()
            );
        }

    }else{
        const auto& segments = ALL_SEGMENTS();

        size_t start = std::stoul(START_SEGMENT.slug());
        size_t end   = std::stoul(END_SEGMENT.slug());

        if (start < segments.size()){
            START_DESCRIPTION_SEGMENT.set_text(
                "Start: " + segments[start]->start_text()
            );
        }
        if (end < segments.size()){
            END_DESCRIPTION_SEGMENT.set_text(
                "End: " + segments[end]->end_text()
            );
        }
    }

    update_ui_state();
}
// ---------------------------------------------------------------------------
//  Program logic
// ---------------------------------------------------------------------------

//  The maps every walk plans on, from their JSON files: stop here, before
//  the game is touched, if one is missing or differs from its C++ table.
static void load_story_maps(SingleSwitchProgramEnvironment& env){
    const size_t maps = load_story_map_files(story_map_folder());
    env.log(std::format("[AutoStory] Maps: {} JSON files from {}", maps, story_map_folder()));
}

void BDSPAutoStory::program(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    AutoStoryStats& stats = env.current_stats<AutoStoryStats>();
    g_stop_at_first_failure = STOP_AT_FIRST_FAILURE;
    load_story_maps(env);
    AutoStoryOptions options{
        Language::English,
        STARTERCHOICE,
        NOTIFICATION_STATUS_UPDATE,
        (HelpMode)HELP_MODE,
        OutsideHelpConfig{
            (OutsideHelpPokemon)OUTSIDE_HELP_POKEMON,
            {
                (TorterraMove)OUTSIDE_HELP_ATTACK_1,
                (TorterraMove)OUTSIDE_HELP_ATTACK_2,
                (TorterraMove)OUTSIDE_HELP_ATTACK_3,
                (TorterraMove)OUTSIDE_HELP_ATTACK_4,
            }
        },
        DIAGNOSTIC_FAKE_CHECKPOINT_SAVE ? CheckpointSaveMode::FAKE : CheckpointSaveMode::REAL
    };

    const bool fake_saves = options.checkpoint_save_mode == CheckpointSaveMode::FAKE;
    if (fake_saves) options.checkpoint_retry_scope = CheckpointRetryScope::DEFER_TO_SELECTED_RANGE;
    //  Logs what a checkpoint is for before it runs, and that it finished after.
    auto run_checkpoint_logged = [&](size_t index, const AutoStory_Checkpoint& checkpoint){
        stats.m_current = index;
        env.update_stats();
        env.log("[AutoStory] Objective: " + checkpoint.name() + ". Ends: " + checkpoint.end_text(), COLOR_BLUE);
        add_wrapped_log(env.console.overlay(), "Now: " + checkpoint.name(), COLOR_WHITE);
        add_wrapped_log(env.console.overlay(), "Goal: " + checkpoint.end_text(), COLOR_GRAY);
        checkpoint.run_checkpoint(env, context, options, stats);
        stats.m_completed++;
        stats.m_last_completed = index;
        env.update_stats();
        env.log("[AutoStory] Completed: " + checkpoint.name() + ".", COLOR_DARKGREEN);
        add_wrapped_log(env.console.overlay(), "Done: " + checkpoint.name(), COLOR_GREEN);
    };
    auto run_selected_range = [&](size_t attempt){
        if (attempt > 0) env.log("[Fake saves] Reloaded initial save; restarting the entire selected range from its start.");
        // DIAGNOSTIC ONLY: Remove this branch before pushing to production.
        if (DIAGNOSTIC_JUBILIFE_CENTER || DIAGNOSTIC_JUBILIFE_SCHOOL || DIAGNOSTIC_JUBILIFE_BARRY || DIAGNOSTIC_JUBILIFE_PRESIDENT || DIAGNOSTIC_JUBILIFE_CLOWN_MART || DIAGNOSTIC_JUBILIFE_CLOWN_COMPANY || DIAGNOSTIC_JUBILIFE_POKETCH){
            const std::pair<bool, const char*> parts[] = {
                {DIAGNOSTIC_JUBILIFE_CENTER, JUBILIFE_CENTER_NAME},
                {DIAGNOSTIC_JUBILIFE_SCHOOL, JUBILIFE_SCHOOL_NAME},
                {DIAGNOSTIC_JUBILIFE_BARRY, JUBILIFE_BARRY_NAME},
                {DIAGNOSTIC_JUBILIFE_PRESIDENT, JUBILIFE_PRESIDENT_NAME},
                {DIAGNOSTIC_JUBILIFE_CLOWN_MART, JUBILIFE_CLOWN_MART_NAME},
                {DIAGNOSTIC_JUBILIFE_CLOWN_COMPANY, JUBILIFE_CLOWN_COMPANY_NAME},
                {DIAGNOSTIC_JUBILIFE_POKETCH, JUBILIFE_POKETCH_NAME},
            };
            for (const auto& [on, name] : parts){
                if (!on){
                    continue;
                }
                env.log(std::string("[AutoStory] Objective: ") + name + ".", COLOR_BLUE);
                add_wrapped_log(env.console.overlay(), std::string("Now: ") + name, COLOR_WHITE);
                if (name == JUBILIFE_CENTER_NAME){
                    diagnostic_jubilife_center(env, context);
                }else if (name == JUBILIFE_SCHOOL_NAME){
                    diagnostic_jubilife_school(env, context);
                }else if (name == JUBILIFE_BARRY_NAME){
                    diagnostic_jubilife_barry(env, context);
                }else if (name == JUBILIFE_PRESIDENT_NAME){
                    diagnostic_jubilife_president(env, context);
                }else if (name == JUBILIFE_CLOWN_MART_NAME){
                    diagnostic_jubilife_clown_mart(env, context);
                }else if (name == JUBILIFE_CLOWN_COMPANY_NAME){
                    diagnostic_jubilife_clown_company(env, context);
                }else{
                    diagnostic_jubilife_poketch(env, context);
                }
                env.log(std::string("[AutoStory] Completed: ") + name + ".", COLOR_DARKGREEN);
                add_wrapped_log(env.console.overlay(), std::string("Done: ") + name, COLOR_GREEN);
            }
            return;
        }
        // DIAGNOSTIC ONLY: Remove this branch before pushing to production.
        if (DIAGNOSTIC_ROUTE202_TO_SANDGEM_CENTER || DIAGNOSTIC_SANDGEM_MART_LADY){
            if (DIAGNOSTIC_ROUTE202_TO_SANDGEM_CENTER){
                const std::string name = ROUTE202_TO_SANDGEM_CENTER_NAME;
                env.log("[AutoStory] Objective: " + name + ". Ends: in front of Sandgem Town's Pokemon Center door, not healed, not saved.", COLOR_BLUE);
                add_wrapped_log(env.console.overlay(), "Now: " + name, COLOR_WHITE);
                diagnostic_route202_to_sandgem_center(env, context);
                env.log("[AutoStory] Completed: " + name + ".", COLOR_DARKGREEN);
                add_wrapped_log(env.console.overlay(), "Done: " + name, COLOR_GREEN);
            }
            if (DIAGNOSTIC_SANDGEM_MART_LADY){
                const std::string name = SANDGEM_MART_LADY_NAME;
                env.log("[AutoStory] Objective: " + name + ". Ends: at Route 202's save spot, saved.", COLOR_BLUE);
                add_wrapped_log(env.console.overlay(), "Now: " + name, COLOR_WHITE);
                diagnostic_sandgem_mart_lady(env, context);
                env.log("[AutoStory] Completed: " + name + ".", COLOR_DARKGREEN);
                add_wrapped_log(env.console.overlay(), "Done: " + name, COLOR_GREEN);
            }
            return;
        }
        // DIAGNOSTIC ONLY: Remove this branch before pushing to production.
        if (DIAGNOSTIC_POST_CATCH_ONLY && ENABLE_ADVANCED_MODE &&
            START_CHECKPOINT_TUTORIAL.slug() == "4"){
            env.log("Diagnostic only: Running checkpoint 00.04 after catch.");
            stats.m_current = 4;
            env.update_stats();
            env.log("[AutoStory] Objective: " + AutoStory_Checkpoint_0004().name() + " (after the catch). Ends: " +
                AutoStory_Checkpoint_0004().end_text(), COLOR_BLUE);
            add_wrapped_log(env.console.overlay(), "Now: " + AutoStory_Checkpoint_0004().name() + " (after the catch)", COLOR_WHITE);
            add_wrapped_log(env.console.overlay(), "Goal: " + AutoStory_Checkpoint_0004().end_text(), COLOR_GRAY);
            diagnostic_checkpoint_0004_after_catch(env, context, options, stats);
            stats.m_completed++;
            stats.m_last_completed = 4;
            env.update_stats();
            env.log("[AutoStory] Completed: " + AutoStory_Checkpoint_0004().name() + ".", COLOR_DARKGREEN);
            env.log("Diagnostic continuation: Running checkpoint 00.05.");
            run_checkpoint_logged(5, AutoStory_Checkpoint_0005());
            return;
        }

        const auto& segments = ALL_SEGMENTS();

        if (ENABLE_ADVANCED_MODE){
            size_t start = std::stoul(START_CHECKPOINT_TUTORIAL.slug());
            size_t end   = std::stoul(END_CHECKPOINT_TUTORIAL.slug());

            env.log("Starting AutoStory from checkpoint " + std::to_string(start) +
                    " to " + std::to_string(end));

            const auto& checkpoints = ALL_CHECKPOINTS();

            for (size_t i = start; i <= end && i < checkpoints.size(); i++){
                env.log("Running Checkpoint: " + std::to_string(i));
                run_checkpoint_logged(i, *checkpoints[i]);
            }

        }else{
            size_t start = std::stoul(START_SEGMENT.slug());
            size_t end   = std::stoul(END_SEGMENT.slug());

            for (size_t i = start; i <= end && i < segments.size(); i++){
                env.log("Running Segment: " + std::to_string(i));
                segments[i]->run_segment(env, context, options, stats);
            }
        }
    };
    try{
        if (NEW_GAME_INTRO){
            play_new_game_intro(env, context, PLAYER_NAME);
        }
        if (fake_saves){
            checkpoint_reattempt_loop(env, context, options.notif_status_update, stats,
                std::move(run_selected_range), true, CheckpointSaveMode::FAKE,
                CheckpointRetryScope::SELECTED_RANGE);
        }else{
            run_selected_range(0);
        }
    }catch (OperationFailedException&){
        if (STOP_AT_FIRST_FAILURE){
            env.log("AutoStory failed: leaving the game where it failed (lab option).", COLOR_RED);
            throw;
        }
        //  Leave the Switch in a known state: on HOME with the game closed, so the next
        //  run (or the user) starts from there. The error already holds its screenshot.
        env.log("AutoStory failed: going to HOME and closing the game.", COLOR_RED);
        try{
            go_home(env.console, context);
            close_game_from_home(env.console, context);
        }catch (OperationFailedException& e){
            env.log("Could not close the game: " + e.message(), COLOR_RED);
        }
        throw;
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}