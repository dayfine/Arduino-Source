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


class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

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
    //  After 00.04 saved under Squirtle's ball (row `ball_row`): the stop
    //  for Starter RNG, or its attempts run here. Returns the rows to run
    //  after a shiny, or nothing once the run has stopped.
    std::optional<StoryRunPlan> after_starter_ball(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, const StarterRngPlanned& planned, size_t ball_row
    );
    //  Started at 00.12b (row `first`) with Run Gift RNG in AutoStory on,
    //  the save beside the salesman loaded: Gift RNG's attempts, then for a
    //  shiny the rows after them. As run_story_plan() returns.
    std::optional<std::string> run_from_gift_rng_save(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, StoryExecutor& executor, size_t first
    );
    //  After 00.12 saved beside the Magikarp salesman (row `salesman_row`):
    //  Gift RNG's attempts run here. Returns the rows to run after a shiny,
    //  or nothing once the run has stopped.
    std::optional<StoryRunPlan> after_magikarp_salesman(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryEnv& story, const StarterRngPlanned& planned, size_t salesman_row
    );

    //  The start and end checkpoints, attempts, checks and language, as in
    //  every game's AutoStory.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_FROM_TITLE;
    StringOption PLAYER_NAME;
    //  The RNG programs the run stops for (StoryHandoff).
    BooleanCheckBoxOption STOP_FOR_SID_HELPER;
    BooleanCheckBoxOption STOP_FOR_STARTER_RNG;
    //  Runs SID Helper's steps and Starter RNG's attempts in the program
    //  instead of stopping for them.
    BooleanCheckBoxOption RUN_STARTER_RNG;
    //  Runs Gift RNG's attempts for the Magikarp in the program after 00.12.
    BooleanCheckBoxOption RUN_GIFT_RNG;
    StarterRngPlanOptions RNG_PLAN;
    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
