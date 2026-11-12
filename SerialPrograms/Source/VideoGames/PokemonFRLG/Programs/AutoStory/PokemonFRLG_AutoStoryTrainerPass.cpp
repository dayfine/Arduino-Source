/*  AutoStory Trainer Pass
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryTrainerLine.h"
#include "PokemonFRLG_AutoStoryTrainerPass.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;
using Pokemon::Outcome;


bool wait_for_turn_away(
    int polls, Direction away,
    const std::function<std::optional<Direction>()>& read, const std::function<void()>& wait
){
    bool other_way = false;
    for (int poll = 0; poll < polls; poll++){
        if (poll > 0){
            wait();
        }
        const std::optional<Direction> facing = read();
        if (facing && *facing == away && other_way){
            return true;
        }
        other_way = other_way || (facing && *facing != away);
    }
    return false;
}

namespace{

template <typename Type>
const Type& arg(const StoryStep& step, std::string_view key){
    return Pokemon::story_arg<Type>(step.args, key);
}

//  The battle of a TRAINER_PASS_VARIANT step's trainer, as a BATTLE step.
StoryStep pass_fallback_battle(const StoryStep& step){
    StoryStep fight;
    fight.kind = StoryStepKind::BATTLE;
    fight.name = std::format("{} (fallback battle)", step.name);
    fight.args = {
        {"trainer", arg<std::string>(step, "trainer")},
        {"foes", arg<std::vector<std::string>>(step, "foes")},
        {"use_items", std::vector<Pokemon::StoryItem>()},
        {"may_lose", false},
        {"run", false},
    };
    return fight;
}

//  The tiles crossed without a stop on the line, the last step watched for
//  the trainer's message. Returns whether the trainer saw the player.
bool cross_line(StoryEnv& env, std::optional<Direction> facing, Direction direction, int tiles){
    for (int c = 1; c < tiles; c++){
        step_once(env.console, env.context, direction, facing);
        facing = direction;
    }
    return step_into_line(env.console, env.context, direction, facing, 1).has_value();
}

} // namespace

Outcome pass_trainer(
    StoryEnv& env, const Pokemon::RunState& state, const StoryStep& step,
    const std::function<Outcome(const StoryStep&)>& fight
){
    const Direction direction = arg<Direction>(step, "direction");
    const int tiles = (int)arg<int64_t>(step, "tiles");
    env.console.log(std::format("Story: {}: the trainer's facing can't be read on the console yet; crossing unwatched.", step.name), COLOR_ORANGE);
    if (cross_line(env, Pokemon::facing_of(state.pose), direction, tiles)){
        //  Seen on the line's tile, the crossing's first: the steps after
        //  it were lost to the trainer's "!".
        env.console.log(std::format("Story: {}: the trainer saw the player; fighting them (fallback).", step.name), COLOR_ORANGE);
        const Outcome fought = fight(pass_fallback_battle(step));
        if (!fought.succeeded()){
            return fought;
        }
        for (int c = 1; c < tiles; c++){
            step_once(env.console, env.context, direction, direction);
        }
    }
    const std::vector<TilePos> at = Pokemon::tiles_of(state.pose);
    if (at.size() != 1){
        return Pokemon::outcome_ok();
    }
    TilePos onto = at[0];
    for (int c = 0; c < tiles; c++){
        onto = onto.next(direction);
    }
    return Pokemon::outcome_ok(to_pose(StoryPose{state.pose.map, onto, direction}));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
