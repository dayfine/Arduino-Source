/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "PokemonLZA_AutoStoryExecutor.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryActionSpec;
using Pokemon::StoryStepKind;


namespace{

//  The GAME actions that are not moves, choices or todos.
const std::vector<std::string>& game_actions(){
    static const std::vector<std::string> actions{
        "approach_starter", "story_battle",
        "new_game_language", "pick_look", "type_player_name",
        "mash_to_move_hints", "text_to_choice", "story_battle_start",
        "talk_through_map_screen", "talk_to_pokedex_screen", "climb_ladder",
        "choose_0", "choose_1",
    };
    return actions;
}

//  The built-in action of `kind` named `name`: a replacement keeps its args.
StoryActionSpec builtin_spec(StoryStepKind kind, std::string name){
    const StoryActionSpec* spec = Pokemon::find_story_action(Pokemon::builtin_story_actions(), kind, name);
    if (spec != nullptr){
        return *spec;
    }
    return StoryActionSpec{.kind = kind, .name = std::move(name), .args = {}};
}

StoryActionSpec game_spec(std::string name){
    return StoryActionSpec{.kind = StoryStepKind::GAME, .name = std::move(name), .args = {}};
}

} // namespace


std::vector<StoryActionSpec> plza_story_action_specs(){
    std::vector<StoryActionSpec> specs{
        StoryActionSpec{.kind = StoryStepKind::ROUTE, .name = "fly", .args = {
            Pokemon::StoryArgSpec{
                .key = "target", .type = Pokemon::StoryArgType::GRID_POSE, .required = true,
                .fallback = std::nullopt, .why = "the fast travel place, by slug, as the map",
            },
        }},
        builtin_spec(StoryStepKind::TALK, "talk"),
        builtin_spec(StoryStepKind::ANSWER, "answer"),
        builtin_spec(StoryStepKind::MASH, "mash"),
        StoryActionSpec{.kind = StoryStepKind::MASH, .name = "a", .args = {}},
        StoryActionSpec{.kind = StoryStepKind::HEAL, .name = "heal", .args = {}},
        builtin_spec(StoryStepKind::SHOP, "shop"),
    };
    for (const std::string& name : game_actions()){
        specs.push_back(game_spec(name));
    }
    for (const auto& [name, move] : story_moves()){
        specs.push_back(game_spec(name));
    }
    for (const auto& [name, todo] : story_actions_not_implemented()){
        specs.push_back(game_spec(name));
    }
    return specs;
}

const std::vector<StoryActionSpec>& plza_story_specs(){
    static const Pokemon::StoryGame<Pokemon::ZonePose> game = []{
        Pokemon::StoryGame<Pokemon::ZonePose> specs_only;
        specs_only.id = "plza";
        for (StoryActionSpec& spec : plza_story_action_specs()){
            specs_only.actions.push_back({.spec = std::move(spec), .handler = {}});
        }
        return specs_only;
    }();
    static const Pokemon::StoryActionRegistry<Pokemon::ZonePose> registry(game);
    return registry.specs();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
