/*  Story Step Kind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* story_step_kind_name(StoryStepKind kind){
    switch (kind){
    case StoryStepKind::NEW_GAME:
        return "NEW_GAME";
    case StoryStepKind::OPTIONS:
        return "OPTIONS";
    case StoryStepKind::ROUTE:
        return "ROUTE";
    case StoryStepKind::LEGS:
        return "LEGS";
    case StoryStepKind::STEP:
        return "STEP";
    case StoryStepKind::TALK:
        return "TALK";
    case StoryStepKind::ANSWER:
        return "ANSWER";
    case StoryStepKind::MASH:
        return "MASH";
    case StoryStepKind::BATTLE:
        return "BATTLE";
    case StoryStepKind::TRAIN:
        return "TRAIN";
    case StoryStepKind::HEAL:
        return "HEAL";
    case StoryStepKind::SHOP:
        return "SHOP";
    case StoryStepKind::SAVE:
        return "SAVE";
    case StoryStepKind::READY:
        return "READY";
    case StoryStepKind::GAME:
        return "GAME";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation
