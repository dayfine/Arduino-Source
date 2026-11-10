/*  Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include <vector>
#include "Pokemon_StoryActions.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


StoryArgSpec required(std::string key, StoryArgType type, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = type, .required = true, .fallback = std::nullopt, .why = std::move(why)};
}

StoryArgSpec optional(std::string key, StoryArgType type, StoryArgValue fallback, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = type, .required = false, .fallback = std::move(fallback), .why = std::move(why)};
}

std::vector<StoryActionSpec> make_builtin_actions(){
    const StoryArgSpec fight_wild = optional("fight_wild", StoryArgType::BOOLEAN, false, "fight wild battles on the way instead of running");
    return {
        {.kind = StoryStepKind::ROUTE, .name = "route", .args = {
            required("target", StoryArgType::GRID_POSE, "where to go; with several tiles, any of them"),
            fight_wild,
        }},
        {.kind = StoryStepKind::READY, .name = "ready", .args = {
            required("target", StoryArgType::GRID_POSE, "the nurse"),
        }},
        {.kind = StoryStepKind::LEGS, .name = "legs", .args = {
            required("legs", StoryArgType::LEGS, "the straight legs to walk"),
            fight_wild,
        }},
        {.kind = StoryStepKind::STEP, .name = "face", .args = {
            required("direction", StoryArgType::DIRECTION, "the way to step or face"),
        }},
        {.kind = StoryStepKind::ANSWER, .name = "answer", .args = {
            optional("yes", StoryArgType::BOOLEAN, true, "the answer"),
        }},
        {.kind = StoryStepKind::BATTLE, .name = "fight", .args = {
            optional("may_lose", StoryArgType::BOOLEAN, false, "losing goes on (a rival's first battle)"),
        }},
        {.kind = StoryStepKind::TRAIN, .name = "train", .args = {
            required("battles", StoryArgType::INTEGER, "the battles to fight"),
        }},
        {.kind = StoryStepKind::SHOP, .name = "shop", .args = {
            required("items", StoryArgType::ITEMS, "the items and how many of each"),
        }},
    };
}


} // namespace


std::span<const StoryActionSpec> builtin_story_actions(){
    static const std::vector<StoryActionSpec> actions = make_builtin_actions();
    return actions;
}

const StoryActionSpec* find_story_action(std::span<const StoryActionSpec> actions, StoryStepKind kind, std::string_view name){
    for (const StoryActionSpec& action : actions){
        if (action.kind == kind && (name.empty() || action.name == name)){
            return &action;
        }
    }
    return nullptr;
}



} // namespace Pokemon
} // namespace PokemonAutomation
