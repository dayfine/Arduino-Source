/*  AutoStory Trainer Pass
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <vector>
#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_TrainerFacingReader.h"
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

//  Waits for the trainer's turn away (wait_for_turn_away()), reading their
//  facing on screen, if the reader has the step's "sprite"; else, or with
//  no turn seen in TRAINER_PASS_WATCH, logs that the crossing is blind.
void wait_for_trainer(StoryEnv& env, const StoryStep& step){
    const PlayerSprites* sprites = trainer_sprites(arg<std::string>(step, "sprite"));
    if (sprites == nullptr){
        env.console.log(std::format("Story: {}: no frames of the trainer's sprite to read their facing; crossing blind.", step.name), COLOR_ORANGE);
        return;
    }
    const int dx = (int)arg<int64_t>(step, "trainer_dx");
    const int dy = (int)arg<int64_t>(step, "trainer_dy");
    const Direction away = arg<Direction>(step, "away");
    const bool turned = wait_for_turn_away(
        (int)(TRAINER_PASS_WATCH / TRAINER_PASS_POLL), away,
        [&]() -> std::optional<Direction>{
            env.context.wait_for_all_requests();
            const VideoSnapshot screen = env.console.video().snapshot();
            return screen ? read_trainer_facing(screen, *sprites, dx, dy) : std::nullopt;
        },
        [&]{ pbf_wait(env.context, TRAINER_PASS_POLL); }
    );
    if (turned){
        env.console.log(std::format("Story: {}: the trainer turned {}; crossing.", step.name, direction_name(away)));
    }else{
        env.console.log(std::format("Story: {}: no turn {} seen in {} s; crossing blind.", step.name, direction_name(away), TRAINER_PASS_WATCH.count() / 1000), COLOR_ORANGE);
    }
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
    wait_for_trainer(env, step);
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
