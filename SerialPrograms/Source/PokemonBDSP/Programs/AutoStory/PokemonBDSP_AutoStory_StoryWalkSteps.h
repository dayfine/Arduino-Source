/*  BDSP AutoStory - Story walk steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story steps that walk and talk (ROUTE, LEGS, STEP, TALK, MASH), on a
 *  StoryStepIO: ConsoleStory runs them on the console, the story's
 *  simulation tests on a simulated player.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryWalkSteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryWalkSteps_H

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_CenterVisit.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvent.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegBattle.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegTraceIO.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A Pokemon Center visit (Pokemon::run_center_visit()) on a StoryStepIO:
//  the doors are runs north in and south out, running with `run`, and the
//  settles are the step IO's, so the inside's fade-in is waited out before
//  the walk to the nurse (lab-B 2026-11-10, 00.04b). The console and the
//  simulation give the reads, the lineup and the nurse.
class BdspCenterVisit : public Pokemon::CenterVisitIO{
public:
    BdspCenterVisit(StoryStepIO& io, bool run)
        : m_io(io)
        , m_run(run)
    {}

    virtual WarpSeen walk_in() override{
        return m_io.run_into_door(Pokemon::Direction::NORTH, m_run);
    }
    virtual WarpSeen walk_out() override{
        return m_io.run_into_door(Pokemon::Direction::SOUTH, m_run);
    }
    virtual void settle(SettleKind kind, std::string_view label) override{
        m_io.settle(BUTTON_B, std::string(label), kind, "");
    }

protected:
    StoryStepIO& m_io;
    bool m_run;
};

//  Where a leg's taps end, and how many of them moved.
struct TappedEnd{
    Pokemon::TilePos end;
    size_t moved = 0;
};
//  Where `leg`'s taps from `pos` (a tile of `map`) end on `world`: each tap
//  moves a tile until a wall stops them.
TappedEnd tapped_end(const StoryWorld& world, const std::string& map, const Pokemon::TilePos& pos, const Pokemon::StoryLeg& leg);
//  The tiles a "measured" LEGS step may end on from `from`, one a tile of
//  it; the step measures which. Nothing if `from`'s map has no world.
std::optional<Pokemon::Pose> measured_ends(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
//  The trainers of `world` who can see us in row `row`: story_trainers(),
//  less those an earlier row fights, whom its save and every later one's
//  keep beaten.
std::vector<TrainerSight> row_trainers(const StoryWorld& world, const std::string& row);


//  Executes walking and movement-related story steps via StoryStepIO.
class StoryWalkSteps{
public:
    explicit StoryWalkSteps(StoryStepIO& io)
        : m_io(io)
        , m_legs(legs_to_log(io.route()))
        , m_route(io.route(), m_legs)
    {}

    //  A new attempt of the row `row`: every trainer can see us again, and
    //  the pace is the row's save's (a run after the Running Shoes). With
    //  the row's `steps`, a route followed by a scene, or by a plain step onto
    //  the scene's trigger, hands a dialogue that starts on the way to that
    //  scene (RouteWalkPlan::scene_next).
    void begin_attempt(const std::string& row, const std::vector<Pokemon::StoryStep>& steps = {});
    //  Whether the player has the Running Shoes: the pace routes are timed at.
    bool can_run() const{
        return m_can_run;
    }

    //  Walks the planned route to `step.target`. Variant: the plan's costs.
    //  With a leg, then runs on toward its direction until a dialogue starts,
    //  for its tiles at most (run_to_dialogue()).
    Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  See ConsoleStory's LEGS.
    Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  One D-pad step, or a run through a door when the step changes map.
    Pokemon::Outcome step(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  A at what we face; a dialogue must open.
    Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  Text to a quiet overworld; the Running Shoes' scene gives the shoes.
    Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step);

private:
    Pokemon::Pose walk_to(const Pokemon::Pose& from, const Pokemon::Pose& goal, const Pokemon::StoryStep& step);
    //  After a wild battle on `leg`'s taps from `starts`: walks to one of
    //  `goals` from wherever the leg could have left us, then faces `facing`.
    Pokemon::Pose recover_tapped_leg(const Pokemon::Pose& from,
        const std::vector<Pokemon::TilePos>& starts, const std::vector<Pokemon::TilePos>& goals,
        Pokemon::Direction facing, const Pokemon::StoryLeg& leg, const Pokemon::StoryStep& step,
        const std::optional<TileSpan>& moved);
    Pokemon::Outcome tap_legs(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
    Pokemon::Outcome counted_wall(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
    Pokemon::Outcome measured_legs(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
    Pokemon::Outcome run_to_dialogue(const Pokemon::StoryStep& step);
    Pokemon::Outcome leg_to_map(const Pokemon::StoryStep& step);
    Pokemon::Outcome one_step(const Pokemon::RunState& state, const Pokemon::StoryStep& step);
    Pokemon::Outcome tap_to_banner(const Pokemon::Pose& from, const Pokemon::StoryStep& step);
    bool through_already(const Pokemon::StoryStep& step);
    WarpSeen door_from_beside(const Pokemon::StoryStep& step, const WarpSeen& seen, Milliseconds timeout, const std::string& banner);
    void turn(Pokemon::Direction direction);
    void log(const std::string& text);

    StoryStepIO& m_io;
    //  The leg records of the walks, written to m_io's route log.
    Pokemon::LegTracer m_legs;
    //  m_io's route walker console, telling m_legs of each leg walked.
    LegTraceRouteIO m_route;
    std::string m_row;
    bool m_can_run = false;
    //  The trainers who can still see us on each story_plan_world() world,
    //  in its tiles, from its first walk this attempt; all of them again at
    //  each attempt.
    std::map<const StoryWorld*, std::vector<TrainerSight>> m_trainers;
    //  The warp the last route ran on through at its end (walk_route_to_warp()),
    //  for the step after it, which goes through that warp.
    WarpSeen m_warp;
    //  The row's steps followed by a scene (begin_attempt()).
    std::set<std::string> m_scene_next;
    //  The last route ended in the scene of the step after it, or after the
    //  plain step onto its trigger (RouteEnd::scene); that step is not taken.
    bool m_scene_open = false;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
