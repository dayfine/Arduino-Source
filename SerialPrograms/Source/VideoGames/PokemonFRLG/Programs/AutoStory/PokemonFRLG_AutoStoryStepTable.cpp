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


std::string_view story_action_name(StoryActionId id){
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

std::span<const Pokemon::StoryActionSpec> story_action_specs(){
    static const std::vector<Pokemon::StoryActionSpec> SPECS = []{
        const std::span<const Pokemon::StoryActionSpec> builtin = Pokemon::builtin_story_actions();
        std::vector<Pokemon::StoryActionSpec> specs(builtin.begin(), builtin.end());
        //  A trainer battle's plan names the trainer and its party.
        for (Pokemon::StoryActionSpec& spec : specs){
            if (spec.kind != StoryStepKind::BATTLE){
                continue;
            }
            spec.args.push_back(Pokemon::StoryArgSpec{
                .key = "trainer", .type = Pokemon::StoryArgType::WORD, .required = false, .fallback = std::string(),
                .why = "pret's constant for the trainer (\"TRAINER_LEADER_BROCK\"); sets the battle's risk",
            });
            spec.args.push_back(Pokemon::StoryArgSpec{
                .key = "foes", .type = Pokemon::StoryArgType::WORDS, .required = false, .fallback = std::vector<std::string>(),
                .why = "the trainer's party in order; attacks are chosen against the one out",
            });
        }
        for (StoryStepKind kind : {StoryStepKind::ROUTE, StoryStepKind::LEGS}){
            Pokemon::StoryActionSpec run = *Pokemon::find_story_action(builtin, kind, "");
            run.name = "run";
            specs.push_back(std::move(run));
        }
        return specs;
    }();
    return SPECS;
}

Pokemon::StoryActionTable make_story_action_table(const StoryActionBinding& bind){
    Pokemon::StoryActionTable table;
    for (const StoryActionRow& row : story_action_rows()){
        Pokemon::StoryAction action = bind(row.id);
        if (const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(story_action_specs(), row.kind, row.variant)){
            action.args = spec->args;
        }
        table.add(row.kind, row.variant, std::move(action));
    }
    return table;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
