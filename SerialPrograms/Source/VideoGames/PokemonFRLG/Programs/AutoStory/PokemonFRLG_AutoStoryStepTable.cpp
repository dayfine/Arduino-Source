/*  AutoStory Step Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::string_view story_action_name(StoryActionId id){
    switch (id){
    case StoryActionId::NEW_GAME:           return "new_game";
    case StoryActionId::OPTIONS:            return "options";
    case StoryActionId::ROUTE:              return "route";
    case StoryActionId::LEGS:               return "legs";
    case StoryActionId::STEP:               return "step";
    case StoryActionId::TALK:               return "talk";
    case StoryActionId::ANSWER:             return "answer";
    case StoryActionId::MASH:               return "mash";
    case StoryActionId::BATTLE:             return "battle";
    case StoryActionId::HEAL:               return "heal";
    case StoryActionId::SHOP:               return "shop";
    case StoryActionId::READY:              return "ready";
    case StoryActionId::CLOSE_TRAINER_CARD: return "close_trainer_card";
    case StoryActionId::LEAVE_GIFT_SUMMARY: return "leave_gift_summary";
    case StoryActionId::CUT_TREE:           return "cut_tree";
    case StoryActionId::PUSH_BOULDER:       return "push_boulder";
    case StoryActionId::TRASH_CANS:         return "trash_cans";
    case StoryActionId::GAME:               return "game";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown story action.");
}

StoryActionId story_action_id_of(std::string_view action){
    if (action == "new_game"){
        return StoryActionId::NEW_GAME;
    }
    if (action == "options"){
        return StoryActionId::OPTIONS;
    }
    if (action == "route"){
        return StoryActionId::ROUTE;
    }
    if (action == "legs"){
        return StoryActionId::LEGS;
    }
    if (action == "step"){
        return StoryActionId::STEP;
    }
    if (action == "talk"){
        return StoryActionId::TALK;
    }
    if (action == "answer"){
        return StoryActionId::ANSWER;
    }
    if (action == "scene"){
        return StoryActionId::MASH;
    }
    if (action == "battle"){
        return StoryActionId::BATTLE;
    }
    if (action == "heal"){
        return StoryActionId::HEAL;
    }
    if (action == "shop"){
        return StoryActionId::SHOP;
    }
    if (action == "ready"){
        return StoryActionId::READY;
    }
    if (action == CLOSE_TRAINER_CARD){
        return StoryActionId::CLOSE_TRAINER_CARD;
    }
    if (action == LEAVE_GIFT_SUMMARY){
        return StoryActionId::LEAVE_GIFT_SUMMARY;
    }
    if (action == CUT_TREE_VARIANT){
        return StoryActionId::CUT_TREE;
    }
    if (action == PUSH_BOULDER_VARIANT){
        return StoryActionId::PUSH_BOULDER;
    }
    if (action == TRASH_CANS_VARIANT){
        return StoryActionId::TRASH_CANS;
    }
    return StoryActionId::GAME;
}

FrlgActionTable make_story_action_table(const StoryActionBinding& bind){
    FrlgActionTable table;
    for (const std::string& name : frlg_action_names()){
        table.emplace(name, bind(story_action_id_of(name)));
    }
    return table;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
