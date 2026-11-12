/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays FireRed from a new game to the Boulder Badge, as the story
 *  checkpoints of PokemonFRLG_AutoStoryTable.h run by the shared story
 *  engine. Each checkpoint saves at its end; one that fails is retried from
 *  that save, as the shared retry options say.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_H
#define PokemonAutomation_PokemonFRLG_AutoStory_H

#include <optional>
#include <string>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"
#include "PokemonFRLG_AutoStoryRngOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Program descriptor for FRLG AutoStory.
class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

//  Program instance executing FRLG AutoStory.
class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    //  The RNG programs' handoffs the run takes, from the options.
    StoryHandoffs story_handoffs() const;
    //  Runs `plan`'s rows, then the RNG handoff they stop at and, for a
    //  shiny, the rows after it, to the end checkpoint. Returns the last
    //  checkpoint run, or nothing once the run has stopped.
    std::optional<std::string> run_story_plan(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, StoryExecutor& executor, StoryRunPlan plan
    );
    //  After 01.05 saved under Squirtle's ball (row `ball_row`): the stop
    //  for Starter RNG, or its attempts run here. Returns the rows to run
    //  after a shiny, or nothing once the run has stopped.
    std::optional<StoryRunPlan> after_starter_ball(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, const StarterRngPlanned& planned, size_t ball_row
    );
    //  Started at a gift's live-start row (04.03, 09.04; row `first`) with
    //  Run Gift RNG in AutoStory on, the save where the gift is taken
    //  loaded: Gift RNG's attempts, then for a shiny the rows after them.
    //  As run_story_plan() returns.
    std::optional<std::string> run_from_gift_rng_save(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, StoryExecutor& executor, size_t first
    );
    //  After a GIFT_RNG row (04.02 beside the Magikarp salesman, 09.03
    //  facing the Eevee's ball; row `gift_row`) saved: Gift RNG's attempts
    //  for its gift run here. Returns the rows to run after a shiny, or
    //  nothing once the run has stopped.
    std::optional<StoryRunPlan> after_gift_row(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, const StarterRngPlanned& planned, size_t gift_row
    );

    //  The start and end checkpoints, attempts, checks and language, as in
    //  every game's AutoStory.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_FROM_TITLE;
    EnumDropdownOption<PlayerGender> PLAYER_GENDER;
    StringOption PLAYER_NAME;
    //  The RNG programs the run stops for (StoryHandoff).
    BooleanCheckBoxOption STOP_FOR_SID_HELPER;
    BooleanCheckBoxOption STOP_FOR_STARTER_RNG;
    //  Runs SID Helper's steps and Starter RNG's attempts in the program
    //  instead of stopping for them.
    BooleanCheckBoxOption RUN_STARTER_RNG;
    //  Runs Gift RNG's attempts in the program after 04.02 (the Magikarp)
    //  and 09.03 (the Eevee).
    BooleanCheckBoxOption RUN_GIFT_RNG;
    StarterRngPlanOptions RNG_PLAN;
    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    //  The save loaded was made by hand: keep the screen as its start frame.
    BooleanCheckBoxOption KEEP_START_FRAME;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
