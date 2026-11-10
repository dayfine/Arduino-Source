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
    return [name = std::move(name)](StoryBackend<PoseType>&, RunState&, const StoryActionArgs&){
        return outcome_failed(
            "unsupported_step",
            std::format("{} needs the game's own action: register one named \"{}\" in StoryGame::actions", name, name)
        );
    };
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
Outcome mash(StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs&){
    return mash_to_overworld(backend);
}

//  A at what the player faces must open a text box; then text to the
//  overworld. No box is a failure: nobody was there to talk to.
template <StoryPose PoseType>
Outcome talk(StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs&){
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
Outcome walk_legs(StoryBackend<PoseType>& backend, RunState& state, const StoryActionArgs& args){
    const std::vector<StoryLeg>& legs = story_arg<std::vector<StoryLeg>>(args, "legs");
    for (const StoryLeg& leg : legs){
        for (int c = 0; c < leg.tiles; c++){
            backend.tap(leg.direction);
        }
    }
    return outcome_ok(after_legs(state.pose, legs));
}

template <StoryPose PoseType>
Outcome face(StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs& args){
    backend.tap(story_arg<Direction>(args, "direction"));
    return outcome_ok();
}

template <StoryPose PoseType>
Outcome answer(StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs& args){
    using namespace std::chrono_literals;
    backend.press(story_arg<bool>(args, "yes") ? NintendoSwitch::BUTTON_A : NintendoSwitch::BUTTON_B, 80ms, 120ms);
    return outcome_ok();
}

template <StoryPose PoseType>
Outcome fight(StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs& args){
    return backend.battle(args);
}

//  The built-in action named `name`, or the hook for a game's own.
template <StoryPose PoseType>
StoryActionHandler<PoseType> builtin_handler(std::string_view name){
    const std::map<std::string, StoryActionHandler<PoseType>, std::less<>> handlers{
        {"legs", &walk_legs<PoseType>},
        {"face", &face<PoseType>},
        {"answer", &answer<PoseType>},
        {"fight", &fight<PoseType>},
    };
    const auto it = handlers.find(name);
    return it != handlers.end() ? it->second : needs_game<PoseType>(std::string(name));
}


} // namespace


template <StoryPose PoseType>
std::vector<StoryAction<PoseType>> builtin_story_handlers(const StoryGame<PoseType>& game){
    std::vector<StoryAction<PoseType>> actions;
    for (const StoryActionSpec& spec : builtin_story_actions()){
        actions.push_back({.spec = spec, .handler = builtin_handler<PoseType>(spec.name)});
    }
    actions.push_back({.spec = {.kind = StoryStepKind::TALK, .name = "talk", .args = {}}, .handler = &talk<PoseType>});
    actions.push_back({.spec = {.kind = StoryStepKind::MASH, .name = "mash", .args = {}}, .handler = &mash<PoseType>});
    StoryActionHandler<PoseType> save = [save = game.save](StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs&){
        return save ? save(backend) : outcome_failed("unsupported_step", "save needs StoryGame::save");
    };
    actions.push_back({.spec = {.kind = StoryStepKind::SAVE, .name = "save", .args = {}}, .handler = std::move(save)});
    return actions;
}

template std::vector<StoryAction<GridPose>> builtin_story_handlers<GridPose>(const StoryGame<GridPose>& game);
template std::vector<StoryAction<ZonePose>> builtin_story_handlers<ZonePose>(const StoryGame<ZonePose>& game);



} // namespace Pokemon
} // namespace PokemonAutomation
