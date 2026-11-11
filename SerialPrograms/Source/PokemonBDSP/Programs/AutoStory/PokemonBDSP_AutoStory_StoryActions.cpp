/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryAction;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


Pokemon::StoryActionTable story_action_table(StoryWalkSteps& walks, const StoryBoundActions& bound){
    Pokemon::StoryActionTable table;
    using WalkMethod = Outcome (StoryWalkSteps::*)(RunState&, const StoryStep&);
    const auto walk = [&](StoryStepKind kind, WalkMethod method){
        table.add(kind, "", StoryAction{
            .act = [&walks, method](const StoryStep& step, RunState& state){
                return (walks.*method)(state, step);
            },
        });
    };
    walk(StoryStepKind::ROUTE, &StoryWalkSteps::route);
    walk(StoryStepKind::LEGS, &StoryWalkSteps::legs);
    walk(StoryStepKind::STEP, &StoryWalkSteps::step);
    walk(StoryStepKind::TALK, &StoryWalkSteps::talk);
    walk(StoryStepKind::MASH, &StoryWalkSteps::mash);

    const auto console = [&](StoryStepKind kind, const Pokemon::StoryActionFunction& act){
        table.add(kind, "", StoryAction{.act = act, .needs_console = true});
    };
    console(StoryStepKind::NEW_GAME, bound.new_game);
    console(StoryStepKind::OPTIONS, bound.options);
    console(StoryStepKind::ANSWER, bound.answer);
    console(StoryStepKind::BATTLE, bound.battle);
    console(StoryStepKind::TRAIN, bound.train);
    console(StoryStepKind::HEAL, bound.heal);
    console(StoryStepKind::SAVE, bound.save);

    for (const auto& item : story_events()){
        table.add_any_kind(item.first, StoryAction{.act = bound.event(item.first), .needs_console = true});
    }
    return table;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
