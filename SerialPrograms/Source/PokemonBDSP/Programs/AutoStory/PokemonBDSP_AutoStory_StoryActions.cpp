/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryAction;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

//  Adds `action` for `kind`'s default and for each of its named actions in
//  bdsp_story_actions(), each with that spec's args, so lint_story_checkpoint()
//  checks a step's args against the action it names.
void add_with_specs(Pokemon::StoryActionTable& table, StoryStepKind kind, const StoryAction& action){
    const std::span<const Pokemon::StoryActionSpec> specs = bdsp_story_actions();
    StoryAction fallback = action;
    if (const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(specs, kind, "")){
        fallback.args = spec->args;
    }
    table.add(kind, "", std::move(fallback));
    for (const Pokemon::StoryActionSpec& spec : specs){
        //  A name that is also a story event stays the event's.
        if (spec.kind != kind || spec.name.empty() || story_events().contains(spec.name)){
            continue;
        }
        StoryAction named = action;
        named.args = spec.args;
        table.add(kind, spec.name, std::move(named));
    }
}

} // namespace


Pokemon::StoryActionTable story_action_table(StoryWalkSteps& walks, const StoryBoundActions& bound){
    Pokemon::StoryActionTable table;
    using WalkMethod = Outcome (StoryWalkSteps::*)(RunState&, const StoryStep&);
    const auto walk = [&](StoryStepKind kind, WalkMethod method){
        add_with_specs(table, kind, StoryAction{
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
        add_with_specs(table, kind, StoryAction{.act = act, .needs_console = true});
    };
    console(StoryStepKind::NEW_GAME, bound.new_game);
    console(StoryStepKind::OPTIONS, bound.options);
    console(StoryStepKind::ANSWER, bound.answer);
    console(StoryStepKind::BATTLE, bound.battle);
    console(StoryStepKind::TRAIN, bound.train);
    console(StoryStepKind::HEAL, bound.heal);
    console(StoryStepKind::SAVE, bound.save);

    for (const auto& item : story_events()){
        StoryAction event{.act = bound.event(item.first), .needs_console = true};
        for (const Pokemon::StoryActionSpec& spec : bdsp_story_actions()){
            if (spec.name == item.first && !spec.args.empty()){
                event.args = spec.args;
                break;
            }
        }
        table.add_any_kind(item.first, std::move(event));
    }
    return table;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
