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
    static const std::pair<std::string_view, StoryActionId> TABLE[] = {
        {"new_game", StoryActionId::NEW_GAME},
        {"options", StoryActionId::OPTIONS},
        {"route", StoryActionId::ROUTE},
        {"legs", StoryActionId::LEGS},
        {"step", StoryActionId::STEP},
        {"talk", StoryActionId::TALK},
        {"answer", StoryActionId::ANSWER},
        {"scene", StoryActionId::MASH},
        {"battle", StoryActionId::BATTLE},
        {"heal", StoryActionId::HEAL},
        {"shop", StoryActionId::SHOP},
        {"ready", StoryActionId::READY},
        {CLOSE_TRAINER_CARD, StoryActionId::CLOSE_TRAINER_CARD},
        {LEAVE_GIFT_SUMMARY, StoryActionId::LEAVE_GIFT_SUMMARY},
        {CUT_TREE_VARIANT, StoryActionId::CUT_TREE},
        {PUSH_BOULDER_VARIANT, StoryActionId::PUSH_BOULDER},
        {TRASH_CANS_VARIANT, StoryActionId::TRASH_CANS},
    };
    for (const auto& [name, id] : TABLE){
        if (action == name){
            return id;
        }
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
