/*  BDSP AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's way of doing the shared story steps
 *  (Pokemon/Navigation/Pokemon_StorySteps.h) on the console. A route is
 *  planned on the joined maps that hold its start and its target
 *  (story_plan_world()), from the tiles the run's pose holds, and walked by
 *  the walker, which fights the trainers who see us and runs from wild
 *  battles. A step whose variant names one of the story's events
 *  (story_events(), PokemonBDSP_AutoStory_Events.h) runs that event,
 *  whatever its kind: the scripted scenes and the console-measured walks
 *  of segment 01.
 *
 *  Every step runs inside a guard that turns a failed console step into a
 *  failed Outcome, with a screenshot and the "checkpoint_reattempt_loop:
 *  attempt N failed" line the lab's run watcher reads; the segment then
 *  reloads the row's save and runs it again.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


enum class StarterChoice{
    TURTWIG,
    CHIMCHAR,
    PIPLUP,
};

//  What a row does when one of its steps fails.
enum class StoryFailure{
    //  Save before the first step; on a failure reset to that save and run
    //  the row again from its first step.
    RELOAD_AND_RETRY,
    //  No save and no retry: the failure ends the program (00.00, before
    //  the game has anything worth saving).
    STOP,
};

//  A row of the story table: the shared checkpoint, and what a failure does.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
};

//  What the story's steps act on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StarterChoice starter = StarterChoice::CHIMCHAR;
    std::string player_name;
};


//  The plan costs a ROUTE step's variant names: "" the route costs,
//  "town" town_plan_costs(), "robust" the planner's defaults. Nothing for
//  another name.
std::optional<RobustPlanCosts> story_route_costs(const std::string& variant);


//  Does BDSP's kinds of story step on the console.
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);

    StoryEnv& env(){
        return m_env;
    }
    VideoStream& stream(){
        return m_env.env.console;
    }
    ProControllerContext& context(){
        return m_env.context;
    }
    //  The row running now.
    const std::string& checkpoint() const{
        return m_checkpoint;
    }
    //  The rows whose failure ends the program (StoryFailure::STOP): a
    //  failed step there throws on instead of failing the step.
    void set_stop_rows(std::set<std::string> rows){
        m_stop_rows = std::move(rows);
    }
    //  Walks from the tiles of `from` on its map to any of `goal`'s tiles,
    //  fighting the trainers who see us this attempt. Returns where it left
    //  us, as a pose on `goal`'s map.
    Pokemon::Pose walk_to(const Pokemon::Pose& from, const Pokemon::Pose& goal, const Pokemon::StoryStep& step);

    //  A new attempt of a row: every trainer can see us again.
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override;
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

    //  The intro of a new game, from the title screen to the bedroom.
    virtual Pokemon::Outcome new_game(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  verify_tutorial_settings(): fast text, Autosave off.
    virtual Pokemon::Outcome options(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  Variant: the plan's costs ("" route, "town", "robust").
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  Each leg in D-pad taps, turning first. Variant "one_short_ok": a leg
    //  may end a tile short (a door lined up by sight after). Variant
    //  "to_wall": each leg walks until something stops it, for at most its
    //  tiles' time, as measured on the console; the pose is then unknown
    //  unless the step gives one.
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  One D-pad step `direction`, turning first: onto a scene's trigger or
    //  into a door.
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  A at what we face; a dialogue must open.
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  A for YES, B for NO, once the question's box shows.
    virtual Pokemon::Outcome answer(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  A dialogue opens (or has), then B through it to the overworld;
    //  variant "a": A through it, for lines that give an item.
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  A trainer's text opens (or has) and ends in a battle, fought with the
    //  best move; one lost fails the step unless `may_lose`. Variant
    //  "doubles" for a tag battle.
    virtual Pokemon::Outcome battle(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  `battles` wild battles in the grass we stand in, with the best move.
    virtual Pokemon::Outcome train(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  In front of a Pokemon Center, its lit doorway on screen: in, the
    //  nurse, and out (heal_at_pokemon_center()).
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome save(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  The event its variant names (story_events()).
    virtual Pokemon::Outcome game(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

private:
    Pokemon::Outcome run_guarded(Pokemon::RunState& state, const Pokemon::StoryStep& step);

    StoryEnv& m_env;
    std::string m_checkpoint;
    //  Route 202's trainers who can still see us, in story_plan_world()'s
    //  world tiles; all of them again at each attempt.
    std::vector<TrainerSight> m_trainers;
    std::map<std::string, size_t> m_failures;
    std::map<std::string, size_t> m_run_counts;
    std::set<std::string> m_stop_rows;
};


//  `rows` (kept alive by the caller) as one segment. A row that reloads on
//  failure saves first, unless `saved` says it has in this run: a retry
//  starts from that save. `executor` and `saved` must outlive the segment.
Pokemon::Segment make_story_segment(
    const std::vector<StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor,
    std::set<std::string>& saved
);



}
}
}
#endif
