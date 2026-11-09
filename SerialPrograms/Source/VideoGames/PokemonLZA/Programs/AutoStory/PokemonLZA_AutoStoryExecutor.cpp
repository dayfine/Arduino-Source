/*  PokemonLZA AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <set>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


size_t starter_index(StarterChoice starter){
    switch (starter){
    case StarterChoice::CHIKORITA:
        return 0;
    case StarterChoice::TEPIG:
        return 1;
    case StarterChoice::TOTODILE:
        return 2;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown starter.");
}


namespace{

//  The variants each kind takes; GAME also takes story_moves()' names.
const std::map<StoryStepKind, std::set<std::string, std::less<>>>& step_variants(){
    static const std::map<StoryStepKind, std::set<std::string, std::less<>>> variants{
        {StoryStepKind::ROUTE, {"fly"}},
        {StoryStepKind::TALK, {""}},
        {StoryStepKind::ANSWER, {""}},
        {StoryStepKind::MASH, {"", "a"}},
        {StoryStepKind::BATTLE, {""}},
        {StoryStepKind::HEAL, {""}},
        {StoryStepKind::SHOP, {""}},
        {StoryStepKind::SAVE, {""}},
        {StoryStepKind::GAME, {"choose_starter"}},
    };
    return variants;
}

} // namespace

bool story_step_supported(const StoryStep& step){
    if (step.kind == StoryStepKind::GAME && story_moves().contains(step.variant)){
        return true;
    }
    const auto kind = step_variants().find(step.kind);
    return kind != step_variants().end() && kind->second.contains(step.variant);
}


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
{}

void StoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
}

Outcome StoryExecutor::run(RunState& state, const StoryStep& step){
    console().log(
        std::format("[{}] {} {}: {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name, step.why),
        COLOR_BLUE
    );
    if (!story_step_supported(step)){
        return Pokemon::unsupported_step(step);
    }
    try{
        return Pokemon::StoryStepExecutor::run(state, step);
    }catch (StoryActionNotImplemented&){
        throw;
    }catch (OperationFailedException& e){
        console().log(std::format("[{}] {} failed: {}", m_checkpoint, step.name, e.message()), COLOR_RED);
        return Pokemon::outcome_failed("operation_failed", e.message());
    }
}

Outcome StoryExecutor::route(RunState&, const StoryStep& step){
    //  story_step_supported(): the variant is "fly"; lint: there is a target.
    const LocationItem& place = get_location_item_from_slug(step.target->map);
    fly_to(console(), context(), language(), place.location);
    return Pokemon::outcome_ok(*step.target);
}

Outcome StoryExecutor::talk(RunState&, const StoryStep&){
    talk_to_facing(console(), context());
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::answer(RunState&, const StoryStep& step){
    choose_dialog_option(console(), context(), step.yes ? 0 : 1);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(RunState&, const StoryStep& step){
    mash_to_overworld(console(), context(), step.variant == "a" ? BUTTON_A : BUTTON_B, 60s);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::battle(RunState&, const StoryStep&){
    fight_battle(m_env.env, console(), context(), m_env.battle_ai);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::heal(RunState&, const StoryStep&){
    heal_at_pokemon_center(console(), context());
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::shop(RunState&, const StoryStep& step){
    std::vector<std::string> items;
    for (const Pokemon::StoryItem& item : step.items){
        items.push_back(item.item);
    }
    buy_items(console(), context(), items);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::save(RunState&, const StoryStep&){
    save_story_game(console(), context());
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::game(RunState&, const StoryStep& step){
    if (step.variant == "choose_starter"){
        choose_dialog_option(console(), context(), starter_index(m_env.starter));
        return Pokemon::outcome_ok();
    }
    //  story_step_supported(): any other variant is a move.
    run_story_move(console(), context(), story_moves().find(step.variant)->second);
    return Pokemon::outcome_ok();
}


Pokemon::Segment make_story_segment(const std::vector<Pokemon::StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor){
    Pokemon::Segment segment;
    segment.id = "PLZA";
    for (const Pokemon::StoryCheckpoint& row : rows){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            .id = row.id, .title = row.title, .saves = true,
            .build = [&row, &executor]{
                return Pokemon::build_story_checkpoint(row, executor);
            },
        });
    }
    return segment;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
