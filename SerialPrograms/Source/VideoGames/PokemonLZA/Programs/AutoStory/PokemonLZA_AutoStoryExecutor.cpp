/*  PokemonLZA AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <set>
#include "Common/Cpp/Color.h"
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


const std::map<std::string, std::string, std::less<>>& story_actions_not_implemented(){
    static const std::map<std::string, std::string, std::less<>> todos{
        {"new_game_language", "from the title screen with no save: A at the language list (English is the default)."},
        {"pick_look", "a grid pick for a menu whose highlight starts at random: D-pad UP, LEFT x3 to the first look (assumes the edges clamp)."},
        {"type_player_name", "wait for the keyboard, type the player's name (Kosmos in the lab), then DOWN x2, A on \"Ready!\"."},
        {"mash_to_move_hints", "a \"player can move\" detector (the Move / Control Camera hints): the tutorial has no party HUD for mash_to_overworld(). A picks a choice's top option."},
        {"text_to_choice", "advance a scene's text until a choice's selection arrow shows, pressing nothing that would pick one."},
        {"story_battle_start", "A through the text into a story battle: start on the VS splash or the \"sent out\" line (no Run prompt), A on tutorial cards (B does not close them)."},
        {"talk_through_map_screen", "a map screen detector (\"MAP\" header, \"A Place Pin / B Close\"): close it with B, never A (A places a pin); A through the talk around it."},
        {"climb_ladder", "a ladder: sidestep along the wall to it, face it, A at \"A Climb Up\", hold up until off the top."},
        {"talk_to_pokedex_screen", "a Pokedex entry screen detector (its header): A through the talk until the entry screen opens, then B (A only plays motions there)."},
        {"catch_tutorial", "a throw: ZL+ZR held 1500 ms, then release (500 ms only aims); retry while the catch HUD comes back (it broke out) until \"You caught\"; turn to find the target; balls as a budget."},
        {"battle_then_catch", "fight at once (the target attacks first) until the \"catch window\" line, A twice (a throw is ignored while a dialog shows), then the 1500 ms ZL+ZR throw."},
        {"register_five_species", "find a wild Pokemon of a new species, approach, ZL lock and throw, read caught / broke out, count from the quest panel to 5/5; then back to the lab gate (route not recorded)."},
        {"mash_to_instruction", "a mash that stops at an instruction line (\"Press X to open the main menu...\"): A once to close it, then X; X is ignored while the line shows."},
        {"claim_mable_research_reward", "the main menu: DOWN x3 to \"Mable's Research\", A, the Rewards page (TM004 Rock Smash)."},
    };
    return todos;
}


namespace{

//  The choices a GAME step picks: the row, 0 at the top.
const std::map<std::string, size_t, std::less<>>& choice_rows(){
    static const std::map<std::string, size_t, std::less<>> rows{
        {"choose_0", 0},
        {"choose_1", 1},
    };
    return rows;
}

//  The variants each kind takes; GAME also takes story_moves()' names,
//  choice_rows() and story_actions_not_implemented().
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
        {StoryStepKind::GAME, {"approach_starter", "story_battle"}},
    };
    return variants;
}

} // namespace

bool story_step_supported(const StoryStep& step){
    if (step.kind == StoryStepKind::GAME && (
        story_moves().contains(step.variant) ||
        choice_rows().contains(step.variant) ||
        story_actions_not_implemented().contains(step.variant)
    )){
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
    const auto todo = story_actions_not_implemented().find(step.variant);
    if (todo != story_actions_not_implemented().end()){
        throw_not_implemented(console(), step.variant, todo->second);
    }
    const auto choice = choice_rows().find(step.variant);
    if (choice != choice_rows().end()){
        choose_dialog_option(console(), context(), choice->second);
    }else if (step.variant == "approach_starter"){
        approach_starter();
    }else if (step.variant == "story_battle"){
        fight_battle_in_progress(m_env.env, console(), context(), m_env.battle_ai, 120s);
    }else{
        //  story_step_supported(): any other variant is a move.
        run_story_move(console(), context(), story_moves().find(step.variant)->second);
    }
    return Pokemon::outcome_ok();
}

void StoryExecutor::approach_starter(){
    const auto walk = starter_moves().find(m_env.starter);
    if (walk == starter_moves().end()){
        throw_not_implemented(
            console(), "approach_starter",
            "the walk to this starter: only Chikorita's was recorded (console B, 2026-11-08)."
        );
    }
    run_story_move(console(), context(), walk->second);
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
