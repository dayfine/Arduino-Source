/*  BDSP AutoStory - Story walk steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story steps that walk and talk (ROUTE, LEGS, STEP, TALK, MASH), on a
 *  StoryStepIO: StoryExecutor runs them on the console, the story's
 *  simulation tests on a simulated player.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryWalkSteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryWalkSteps_H

#include <map>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryEvent.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Executes walking and movement-related story steps via StoryStepIO.
class StoryWalkSteps{
public:
    explicit StoryWalkSteps(StoryStepIO& io)
        : m_io(io)
    {}

    //  A new attempt of the row `row`: every trainer can see us again, and
    //  the pace is the row's save's (a run after the Running Shoes).
    void begin_attempt(const std::string& row);
    //  Whether the player has the Running Shoes: the pace routes are timed at.
    bool can_run() const{
        return m_can_run;
    }

    //  Walks the planned route to `step.target`. Variant: the plan's costs.
    Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  See StoryExecutor::legs().
    Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  One D-pad step, or a run through a door when the step changes map.
    Pokemon::Outcome step(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  A at what we face; a dialogue must open.
    Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  Text to a quiet overworld; the Running Shoes' scene gives the shoes.
    Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step);

private:
    Pokemon::Pose walk_to(const Pokemon::Pose& from, const Pokemon::Pose& goal, const Pokemon::StoryStep& step);
    Pokemon::Pose recover_tapped_leg(const Pokemon::Pose& from,
        const std::vector<Pokemon::TilePos>& starts, const std::vector<Pokemon::TilePos>& goals,
        const Pokemon::StoryLeg& leg, const Pokemon::StoryStep& step);
    Pokemon::Outcome tap_legs(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
    Pokemon::Outcome leg_to_dialogue(const Pokemon::StoryStep& step);
    Pokemon::Outcome leg_to_map(const Pokemon::StoryStep& step);
    Pokemon::Outcome tap_to_banner(const Pokemon::StoryStep& step);
    void turn(Pokemon::Direction direction);
    void log(const std::string& text);

    StoryStepIO& m_io;
    std::string m_row;
    bool m_can_run = false;
    //  The trainers who can still see us on each story_plan_world() world,
    //  in its tiles, from its first walk this attempt; all of them again at
    //  each attempt.
    std::map<const StoryWorld*, std::vector<TrainerSight>> m_trainers;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
