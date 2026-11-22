/*  BDSP AutoStory - Story walk steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story steps that walk and talk (route, legs, step, talk, scene), on a
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
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegBattle.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegTraceIO.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"
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
//  The tiles a "measured" legs step may end on from `from`, one a tile of
//  it; the step measures which. Nothing if `from`'s map has no world.
std::optional<Pokemon::GridPose> measured_ends(const Pokemon::GridPose& from, const BdspStep& step);
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
    void begin_attempt(const std::string& row, const std::vector<BdspStep>& steps = {});
    //  Whether the player has the Running Shoes: the pace routes are timed at.
    bool can_run() const{
        return m_can_run;
    }

    //  Walks the planned route to `target`. Variant: the plan's costs.
    //  With a leg, then runs on toward its direction until a dialogue starts,
    //  for its tiles at most (run_to_dialogue()).
    std::optional<Pokemon::StoryFailure> route(BdspRunState& state, const BdspStep& step);
    //  See ConsoleStory's legs description.
    std::optional<Pokemon::StoryFailure> legs(BdspRunState& state, const BdspStep& step);
    //  One D-pad step, or a run through a door when the step changes map.
    std::optional<Pokemon::StoryFailure> step(BdspRunState& state, const BdspStep& step);
    //  A at what we face; a dialogue must open.
    std::optional<Pokemon::StoryFailure> talk(BdspRunState& state, const BdspStep& step);
    //  Text to a quiet overworld; the Running Shoes' scene gives the shoes.
    std::optional<Pokemon::StoryFailure> mash(BdspRunState& state, const BdspStep& step);

private:
    friend void begin_walk_step(StoryWalkSteps& walks, const BdspStep& step);
    void begin_step(const BdspStep& step);
    Pokemon::GridPose walk_to(const Pokemon::GridPose& from, const Pokemon::GridPose& goal, const BdspStep& step, const std::string& variant);
    //  After a wild battle on `leg`'s taps from `starts`: walks to one of
    //  `goals` from wherever the leg could have left us, then faces `facing`.
    Pokemon::GridPose recover_tapped_leg(const Pokemon::GridPose& from,
        const std::vector<Pokemon::TilePos>& starts, const std::vector<Pokemon::TilePos>& goals,
        Pokemon::Direction facing, const Pokemon::StoryLeg& leg, const BdspStep& step,
        const std::optional<TileSpan>& moved);
    std::optional<Pokemon::StoryFailure> tap_legs(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> counted_wall(const Pokemon::GridPose& from, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> measured_legs(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> run_to_dialogue(const BdspStep& step, const std::vector<Pokemon::StoryLeg>& legs_list);
    std::optional<Pokemon::StoryFailure> leg_to_map(const BdspStep& step);
    std::optional<Pokemon::StoryFailure> one_step(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> tap_to_banner(const Pokemon::GridPose& from, const BdspStep& step);
    bool through_already(const BdspStep& step);
    SettleKind door_settle_kind(const BdspStep& step) const;
    WarpSeen door_from_beside(const BdspStep& step, const WarpSeen& seen, Milliseconds timeout, const std::string& banner);
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
    //  Map-changing door steps followed by a scene step (begin_attempt()).
    std::set<std::string> m_door_scene_steps;
    //  The last route ended in the scene of the step after it, or after the
    //  plain step onto its trigger (RouteEnd::scene); that step is not taken.
    bool m_scene_open = false;
    //  The step just completed observed dialogue while settling.
    bool m_previous_settle_saw_dialogue = false;
    //  The scene now running immediately follows that settle.
    bool m_scene_start_already_played = false;
    //  Actual player facing from the last walk/turn, distinct from a step's
    //  declared target facing (which talk() uses as the direction to turn).
    std::optional<Pokemon::Direction> m_actual_facing;
};

//  Starts one dispatched step. Only an immediately following scene may reuse
//  dialogue observed by the preceding step's settle().
void begin_walk_step(StoryWalkSteps& walks, const BdspStep& step);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
