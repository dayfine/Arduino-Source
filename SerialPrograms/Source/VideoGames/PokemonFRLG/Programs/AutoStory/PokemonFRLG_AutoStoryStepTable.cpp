/*  AutoStory Step Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryStepKind;


const char* story_action_name(StoryActionId id){
    switch (id){
    case StoryActionId::NEW_GAME:   return "new_game";
    case StoryActionId::OPTIONS:    return "options";
    case StoryActionId::ROUTE:      return "route";
    case StoryActionId::LEGS:       return "legs";
    case StoryActionId::STEP:       return "step";
    case StoryActionId::TALK:       return "talk";
    case StoryActionId::ANSWER:     return "answer";
    case StoryActionId::MASH:       return "mash";
    case StoryActionId::BATTLE:     return "battle";
    case StoryActionId::HEAL:       return "heal";
    case StoryActionId::SHOP:       return "shop";
    case StoryActionId::READY:      return "ready";
    case StoryActionId::GAME:       return "game";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown story action.");
}

const std::vector<StoryActionRow>& story_action_rows(){
    static const std::vector<StoryActionRow> ROWS{
        {StoryStepKind::NEW_GAME,   "", StoryActionId::NEW_GAME},
        {StoryStepKind::OPTIONS,    "", StoryActionId::OPTIONS},
        {StoryStepKind::ROUTE,      "", StoryActionId::ROUTE},
        {StoryStepKind::LEGS,       "", StoryActionId::LEGS},
        {StoryStepKind::STEP,       "", StoryActionId::STEP},
        {StoryStepKind::TALK,       "", StoryActionId::TALK},
        {StoryStepKind::ANSWER,     "", StoryActionId::ANSWER},
        {StoryStepKind::MASH,       "", StoryActionId::MASH},
        {StoryStepKind::BATTLE,     "", StoryActionId::BATTLE},
        {StoryStepKind::HEAL,       "", StoryActionId::HEAL},
        {StoryStepKind::SHOP,       "", StoryActionId::SHOP},
        {StoryStepKind::READY,      "", StoryActionId::READY},
        {StoryStepKind::GAME,       "", StoryActionId::GAME},
    };
    return ROWS;
}

Pokemon::StoryActionTable make_story_action_table(const StoryActionBinding& bind){
    Pokemon::StoryActionTable table;
    for (const StoryActionRow& row : story_action_rows()){
        table.add(row.kind, row.variant, bind(row.id));
    }
    return table;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
