/*  Story Builtin Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon_StoryBuiltinActions.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  Looks before a text or scene counts as stuck.
inline constexpr size_t MAX_TEXT_LOOKS = 80;
//  Looks for a text box to open after a talk's A.
inline constexpr size_t MAX_OPEN_LOOKS = 20;


//  The action of a built-in that needs the game's own code.
template <StoryPose PoseType>
StoryActionHandler<PoseType> needs_game(std::string name){
    return [name = std::move(name)](StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
        return outcome_failed(
            "unsupported_step",
            std::format("{} needs the game's own action: register one named \"{}\" in StoryGame::actions", name, name)
        );
    };
}

//  The SAVE step outside StoryEngine.
template <StoryPose PoseType>
Outcome needs_engine_save(StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
    return outcome_failed("unsupported_step", "a SAVE step runs through StoryEngine, which calls StoryGame::save");
}

//  Presses A through text and scenes until the player can move.
template <StoryPose PoseType>
Outcome mash_to_overworld(StoryBackend<PoseType>& backend){
    using namespace std::chrono_literals;
    for (size_t c = 0; c < MAX_TEXT_LOOKS; c++){
        const Observation<PoseType> now = backend.observe();
        if (now.events.has(ScreenEvent::CAN_MOVE) && !now.events.has(ScreenEvent::TEXT_BOX)){
            return outcome_ok();
        }
        backend.press(NintendoSwitch::BUTTON_A, 80ms, 120ms);
    }
    return outcome_failed("mash_no_overworld", std::format("no overworld after {} looks", MAX_TEXT_LOOKS));
}

template <StoryPose PoseType>
Outcome mash(StoryBackend<PoseType>& backend, const StoryActionCall&, RunState&){
    return mash_to_overworld(backend);
}

//  A at what the player faces must open a text box; then text to the
//  overworld. No box is a failure: nobody was there to talk to.
template <StoryPose PoseType>
Outcome talk(StoryBackend<PoseType>& backend, const StoryActionCall&, RunState&){
    using namespace std::chrono_literals;
    backend.press(NintendoSwitch::BUTTON_A, 80ms, 120ms);
    for (size_t c = 0; c < MAX_OPEN_LOOKS; c++){
        if (backend.observe().events.has(ScreenEvent::TEXT_BOX)){
            return mash_to_overworld(backend);
        }
        backend.wait(100ms);
    }
    return outcome_failed("talk_no_text", "A opened no text box");
}

//  Where `legs` leave a run that stood on one tile; nothing otherwise.
inline std::optional<Pose> after_legs(const Pose& start, const std::vector<StoryLeg>& legs){
    const std::vector<TilePos> tiles = tiles_of(start);
    if (tiles.size() != 1 || legs.empty()){
        return std::nullopt;
    }
    TilePos tile = tiles[0];
    for (const StoryLeg& leg : legs){
        for (int c = 0; c < leg.tiles; c++){
            tile = tile.next(leg.direction);
        }
    }
    return tile_pose(start.map, {tile}, legs.back().direction);
}

template <StoryPose PoseType>
Outcome walk_legs(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState& state){
    const std::vector<StoryLeg>& legs = story_arg<std::vector<StoryLeg>>(call.args, "legs");
    const std::vector<TilePos> start = tiles_of(state.pose);
    std::optional<TilePos> tile = start.size() == 1 ? std::optional<TilePos>(start[0]) : std::nullopt;
    for (size_t index = 0; index < legs.size(); index++){
        const StoryLeg& leg = legs[index];
        for (int c = 0; tile && c < leg.tiles; c++){
            tile = tile->next(leg.direction);
        }
        call.pause.before_leg(story_leg_pause_label(index, legs.size(), leg.direction, leg.tiles, tile));
        backend.scope().throw_if_cancelled();
        for (int c = 0; c < leg.tiles; c++){
            backend.tap(leg.direction);
        }
    }
    return outcome_ok(after_legs(state.pose, legs));
}

template <StoryPose PoseType>
Outcome face(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState&){
    backend.tap(story_arg<Direction>(call.args, "direction"));
    return outcome_ok();
}

template <StoryPose PoseType>
Outcome answer(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState&){
    using namespace std::chrono_literals;
    backend.press(story_arg<bool>(call.args, "yes") ? NintendoSwitch::BUTTON_A : NintendoSwitch::BUTTON_B, 80ms, 120ms);
    return outcome_ok();
}

template <StoryPose PoseType>
Outcome fight(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState&){
    return backend.battle(call.args);
}

//  The built-in action named `name`, or the hook for a game's own. Null
//  for a tile action ("legs", "face") of a pose type with no tiles: the
//  game has no such action, so a step naming one fails the lint.
template <StoryPose PoseType>
StoryActionHandler<PoseType> builtin_handler(std::string_view name){
    std::map<std::string, StoryActionHandler<PoseType>, std::less<>> handlers{
        {"answer", &answer<PoseType>},
        {"fight", &fight<PoseType>},
    };
    if constexpr (StoryPoseTraits<PoseType>::TILE_MOVES){
        handlers.emplace("legs", &walk_legs<PoseType>);
        handlers.emplace("face", &face<PoseType>);
    }else{
        if (name == "legs" || name == "face"){
            return nullptr;
        }
    }
    const auto it = handlers.find(name);
    return it != handlers.end() ? it->second : needs_game<PoseType>(std::string(name));
}


} // namespace


template <StoryPose PoseType>
std::vector<StoryAction<PoseType>> builtin_story_handlers(){
    std::vector<StoryAction<PoseType>> actions;
    for (const StoryActionSpec& spec : builtin_story_actions()){
        StoryActionHandler<PoseType> handler = builtin_handler<PoseType>(spec.name);
        if (handler){
            actions.push_back({.spec = spec, .handler = std::move(handler)});
        }
    }
    actions.push_back({.spec = {.kind = StoryStepKind::TALK, .name = "talk", .args = {}}, .handler = &talk<PoseType>});
    actions.push_back({.spec = {.kind = StoryStepKind::MASH, .name = "mash", .args = {}}, .handler = &mash<PoseType>});
    //  StoryEngine replaces it: the engine owns the save, to hold it back
    //  while a checkpoint is under test.
    actions.push_back({
        .spec = {.kind = StoryStepKind::SAVE, .name = "save", .args = {}},
        .handler = needs_engine_save<PoseType>,
    });
    return actions;
}

template std::vector<StoryAction<GridPose>> builtin_story_handlers<GridPose>();
template std::vector<StoryAction<ZonePose>> builtin_story_handlers<ZonePose>();



} // namespace Pokemon
} // namespace PokemonAutomation
