/*  BDSP AutoStory - Story step executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP_AutoStory_PokemonCenter.h"
#include "PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::TilePos;


std::optional<Pokemon::RobustPlanCosts> story_route_costs(const std::string& variant){
    if (variant.empty()){
        return route_plan_costs();
    }
    if (variant == "town"){
        return town_plan_costs();
    }
    if (variant == "robust"){
        return Pokemon::RobustPlanCosts();
    }
    return std::nullopt;
}

std::pair<double, double> dpad_of(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return {0, 1};
    case Direction::EAST:
        return {1, 0};
    case Direction::SOUTH:
        return {0, -1};
    case Direction::WEST:
        return {-1, 0};
    }
    return {0, 0};
}


ConsoleStoryExecutor::ConsoleStoryExecutor(
    StoryStepRunner& runner, std::set<std::string> stop_rows,
    const Pokemon::MapSet& maps, std::vector<Pokemon::TrainerSight> trainers,
    TrainerFight fight, std::map<std::string, Variant> variants
)
    : m_runner(runner)
    , m_stop_rows(std::move(stop_rows))
    , m_maps(maps)
    , m_all_trainers(std::move(trainers))
    , m_trainers(m_all_trainers)
    , m_fight(std::move(fight))
    , m_variants(std::move(variants))
{}

void ConsoleStoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
    m_trainers = m_all_trainers;
}

RouteTrainerFight ConsoleStoryExecutor::fight(StoryConsole& console){
    return [this, &console](const std::string& trainer){
        m_fight(console, trainer);
    };
}

StoryConsole& ConsoleStoryExecutor::console(){
    if (m_console == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "A story step ran outside ConsoleStoryExecutor::run().");
    }
    return *m_console;
}

Outcome ConsoleStoryExecutor::run(RunState& state, const Pokemon::StoryStep& step){
    Outcome result = Pokemon::outcome_ok();
    const StoryFailure on_failure = m_stop_rows.contains(m_checkpoint) ? StoryFailure::STOP : StoryFailure::RELOAD_AND_RETRY;
    const Outcome ran = m_runner.run_step(m_checkpoint, on_failure, [&](StoryConsole& console){
        m_console = &console;
        set_checks(console.options.checks);
        const auto variant = m_variants.find(step.variant);
        result = variant == m_variants.end()
            ? Pokemon::StoryStepExecutor::run(state, step)
            : variant->second(*this, console, state, step);
        m_console = nullptr;
    });
    m_console = nullptr;
    return ran.succeeded() ? result : ran;
}

Outcome ConsoleStoryExecutor::route(RunState& state, const Pokemon::StoryStep& step){
    const std::vector<TilePos> from = Pokemon::tiles_of(state.pose);
    if (from.empty()){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const std::optional<Pokemon::RobustPlanCosts> costs = story_route_costs(step.variant);
    if (!costs){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    const std::vector<TilePos> goal = Pokemon::tiles_of(*step.target);
    StoryConsole& c = console();
    const std::vector<TilePos> at = walk_route(
        c.env.console, c.context, m_maps, from,
        [&](const TilePos& pos){ return std::find(goal.begin(), goal.end(), pos) != goal.end(); },
        m_trainers, fight(c), step.name, {}, *costs
    );
    return Pokemon::outcome_ok(Pokemon::tile_pose(step.target->map, at));
}

Outcome ConsoleStoryExecutor::legs(RunState& state, const Pokemon::StoryStep& step){
    const bool one_short_ok = step.variant == "one_short_ok";
    if (!one_short_ok && !step.variant.empty()){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    StoryConsole& c = console();
    std::vector<TilePos> at = Pokemon::tiles_of(state.pose);
    for (const Pokemon::StoryLeg& leg : step.legs){
        //  Facing another way, a tap only turns (r256): turn first.
        route_turn(c.env.console, c.context, leg.direction);
        const auto [x, y] = dpad_of(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        const WalkResult walk = walk_tiles_dpad(c.env.console, c.context, x, y, tiles, step.name);
        c.env.console.log(std::format("[{}] {}: taps {} moved {} of {} tiles.", m_checkpoint, step.name, direction_name(leg.direction), walk.moved_chunks, tiles));
        if (walk.moved_chunks + (one_short_ok ? 1 : 0) < tiles){
            return Pokemon::outcome_failed("short_leg", std::format("{} {} moved {}", direction_name(leg.direction), tiles, walk.moved_chunks));
        }
        //  Where we may be: the end, or a tile short of it.
        std::vector<TilePos> next;
        for (const TilePos& start : at){
            TilePos end = start;
            for (size_t i = 0; i < tiles; i++){
                if (one_short_ok && i + 1 == tiles){
                    next.push_back(end);
                }
                end = end.next(leg.direction);
            }
            next.push_back(end);
        }
        at = std::move(next);
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, step.legs.back().direction));
}

Outcome ConsoleStoryExecutor::train(RunState&, const Pokemon::StoryStep& step){
    StoryConsole& c = console();
    for (size_t battle = 0; battle < step.battles; battle++){
        const std::string label = std::format("Training battle {}/{}", battle + 1, step.battles);
        route_status(c.env.console, label);
        battle_1_pokemon(c.env.console, c.context);
        fail_if_blacked_out(c.env.console, c.context, label);
    }
    return Pokemon::outcome_ok();
}

Outcome ConsoleStoryExecutor::heal(RunState&, const Pokemon::StoryStep& step){
    StoryConsole& c = console();
    heal_at_pokemon_center(c.env.console, c.context, step.why.empty() ? step.name : step.why);
    return Pokemon::outcome_ok();
}

Outcome ConsoleStoryExecutor::save(RunState&, const Pokemon::StoryStep&){
    StoryConsole& c = console();
    checkpoint_save(c.env, c.context, c.options.notif_status_update, c.stats, c.options.checkpoint_save_mode);
    return Pokemon::outcome_ok();
}

Outcome ConsoleStoryExecutor::mash(RunState&, const Pokemon::StoryStep& step){
    StoryConsole& c = console();
    wait_for_dialogue(c.env.console, c.context, step.name);
    pbf_mash_button(c.context, BUTTON_B, 5000ms);
    c.context.wait_for_all_requests();
    return Pokemon::outcome_ok();
}


ConsoleStoryExecutor::Variant story_call(StoryAction action){
    return [action = std::move(action)](ConsoleStoryExecutor&, StoryConsole& console, RunState&, const Pokemon::StoryStep&){
        action(console);
        return Pokemon::outcome_ok();
    };
}



}
}
}
