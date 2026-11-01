/*  BDSP AutoStory - Story step executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's way of doing the shared story steps
 *  (Pokemon/Navigation/Pokemon_StorySteps.h) on the console. Routes are
 *  planned on the story's map set from the tiles the run's pose holds,
 *  with the trainers who can still see us fought on the way; a route's
 *  variant names its plan costs. A segment adds its own steps by variant
 *  (the Mart lady, a walk through a gap in town): a variant registered
 *  here is used for its step whatever the kind.
 *
 *  Each step runs through the table's StoryStepRunner, so a failed step
 *  is answered as the table's other rows are: a screenshot, the
 *  "checkpoint_reattempt_loop: attempt N failed" line the lab watches
 *  for, and a failed Outcome that the segment's policy answers.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryTables.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The plan costs a ROUTE step's variant names: "" the route costs,
//  "town" town_plan_costs(), "robust" the planner's defaults. Nothing for
//  another name.
std::optional<Pokemon::RobustPlanCosts> story_route_costs(const std::string& variant);

//  The D-pad direction (x, y) of `direction`: y is up the screen.
std::pair<double, double> dpad_of(Pokemon::Direction direction);


//  Does BDSP's kinds of story step for one checkpoint of a table.
class ConsoleStoryExecutor : public Pokemon::StoryStepExecutor{
public:
    //  A segment's own way of doing a step, by the step's variant. It may
    //  set run flags for later steps' skip_if, and returns where it left
    //  the player if it knows.
    using Variant = std::function<Pokemon::Outcome(
        ConsoleStoryExecutor& executor, StoryConsole& console,
        Pokemon::RunState& state, const Pokemon::StoryStep& step
    )>;
    //  Fights a trainer whose dialogue has opened.
    using TrainerFight = std::function<void(StoryConsole& console, const std::string& trainer)>;

    //  `stop_rows`: the rows whose failure ends the program (rows_that_stop()).
    ConsoleStoryExecutor(
        StoryStepRunner& runner, std::set<std::string> stop_rows,
        const Pokemon::MapSet& maps, std::vector<Pokemon::TrainerSight> trainers,
        TrainerFight fight, std::map<std::string, Variant> variants
    );

    //  A new attempt of a row: every trainer can see us again.
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override;

    const Pokemon::MapSet& maps() const{
        return m_maps;
    }
    //  The trainers who can still see us, and how to fight one, for a
    //  variant that walks with walk_route().
    std::vector<Pokemon::TrainerSight>& trainers(){
        return m_trainers;
    }
    RouteTrainerFight fight(StoryConsole& console);

    virtual Pokemon::Outcome run(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

    virtual Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  Each leg in D-pad taps, turning first. Variant "one_short_ok": a leg
    //  may end a tile short (a door lined up by sight after).
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome train(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  In front of a Pokemon Center, its lit doorway on screen: in, the
    //  nurse, and out (heal_at_pokemon_center()).
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  The checkpoint save (checkpoint_save(), real or fake as the options say).
    virtual Pokemon::Outcome save(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    //  A dialogue opens (or has), then B through it.
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

private:
    StoryConsole& console();

    StoryStepRunner& m_runner;
    std::set<std::string> m_stop_rows;
    std::string m_checkpoint;
    const Pokemon::MapSet& m_maps;
    std::vector<Pokemon::TrainerSight> m_all_trainers;
    std::vector<Pokemon::TrainerSight> m_trainers;
    TrainerFight m_fight;
    std::map<std::string, Variant> m_variants;
    //  The console of the step running now.
    StoryConsole* m_console = nullptr;
};


//  A variant that calls an old console function, for a step that has no
//  declarative form yet.
ConsoleStoryExecutor::Variant story_call(StoryAction action);



}
}
}
#endif
