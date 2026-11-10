/*  PokemonLZA AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <set>
#include "Common/Cpp/Color.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::story_arg;
using Pokemon::StoryStepKind;


const std::map<std::string, std::string, std::less<>>& story_actions_not_implemented(){
    static const std::map<std::string, std::string, std::less<>> todos{
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

//  The actions each kind takes ("" its default); GAME also takes story_moves()' names,
//  choice_rows() and story_actions_not_implemented().
const std::map<StoryStepKind, std::set<std::string, std::less<>>>& step_actions(){
    static const std::map<StoryStepKind, std::set<std::string, std::less<>>> actions{
        {StoryStepKind::ROUTE, {"fly"}},
        {StoryStepKind::TALK, {""}},
        {StoryStepKind::ANSWER, {""}},
        {StoryStepKind::MASH, {"", "a"}},
        {StoryStepKind::BATTLE, {""}},
        {StoryStepKind::HEAL, {""}},
        {StoryStepKind::SHOP, {""}},
        {StoryStepKind::SAVE, {""}},
        {StoryStepKind::GAME, {
            "approach_starter", "story_battle",
            "new_game_language", "pick_look", "type_player_name",
            "mash_to_move_hints", "text_to_choice", "story_battle_start",
            "talk_through_map_screen", "talk_to_pokedex_screen", "climb_ladder",
        }},
    };
    return actions;
}

} // namespace

bool story_step_supported(const StoryStep& step){
    if (step.kind == StoryStepKind::GAME && (
        story_moves().contains(step.action) ||
        choice_rows().contains(step.action) ||
        story_actions_not_implemented().contains(step.action)
    )){
        return true;
    }
    const auto kind = step_actions().find(step.kind);
    return kind != step_actions().end() && kind->second.contains(step.action);
}


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
{}

void StoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
}

Outcome StoryExecutor::run(RunState&, const StoryStep& step){
    console().log(
        std::format("[{}] {} {}: {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name, step.why),
        COLOR_BLUE
    );
    if (!story_step_supported(step)){
        return Pokemon::unsupported_step(step);
    }
    try{
        return run_kind(step);
    }catch (StoryActionNotImplemented&){
        throw;
    }catch (OperationFailedException& e){
        console().log(std::format("[{}] {} failed: {}", m_checkpoint, step.name, e.message()), COLOR_RED);
        return Pokemon::outcome_failed("operation_failed", e.message());
    }
}

Outcome StoryExecutor::run_kind(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::ROUTE:
        return route(step);
    case StoryStepKind::TALK:
        talk_to_facing(console(), context());
        return Pokemon::outcome_ok();
    case StoryStepKind::ANSWER:
        return answer(step);
    case StoryStepKind::MASH:
        return mash(step);
    case StoryStepKind::BATTLE:
        fight_battle(m_env.env, console(), context(), m_env.battle_ai);
        return Pokemon::outcome_ok();
    case StoryStepKind::HEAL:
        heal_at_pokemon_center(console(), context());
        return Pokemon::outcome_ok();
    case StoryStepKind::SHOP:
        return shop(step);
    case StoryStepKind::SAVE:
        save_story_game(console(), context());
        return Pokemon::outcome_ok();
    case StoryStepKind::GAME:
        return game(step);
    default:
        return Pokemon::unsupported_step(step);
    }
}

Outcome StoryExecutor::route(const StoryStep& step){
    //  story_step_supported(): the action is "fly", to the target's map.
    const Pokemon::GridPose& target = story_arg<Pokemon::GridPose>(step.args, "target");
    const LocationItem& place = get_location_item_from_slug(target.map);
    fly_to(console(), context(), m_env.language, place.location);
    return Pokemon::outcome_ok(Pokemon::pose_of(target));
}

Outcome StoryExecutor::answer(const StoryStep& step){
    choose_dialog_option(console(), context(), story_arg<bool>(step.args, "yes") ? 0 : 1);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(const StoryStep& step){
    //  120 s: 02.01's last scene holds a ~25 s black screen.
    mash_scene(console(), context(), SceneMashGoal::OVERWORLD, step.action == "a" ? BUTTON_A : BUTTON_B, 120s);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::shop(const StoryStep& step){
    std::vector<std::string> items;
    for (const Pokemon::StoryItem& item : story_arg<std::vector<Pokemon::StoryItem>>(step.args, "items")){
        items.push_back(item.item);
    }
    buy_items(console(), context(), items);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::game(const StoryStep& step){
    const auto todo = story_actions_not_implemented().find(step.action);
    if (todo != story_actions_not_implemented().end()){
        throw_not_implemented(console(), step.action, todo->second);
    }
    const auto choice = choice_rows().find(step.action);
    if (choice != choice_rows().end()){
        choose_dialog_option(console(), context(), choice->second);
    }else if (step.action == "approach_starter"){
        approach_starter();
    }else if (step.action == "new_game_language"){
        new_game_language(console(), context());
    }else if (step.action == "pick_look"){
        pick_first_look(console(), context());
    }else if (step.action == "type_player_name"){
        type_player_name(console(), context(), m_env.player_name);
    }else if (step.action == "mash_to_move_hints"){
        //  The tutorial shows no party bar: 8 s with no box is the overworld,
        //  once the scene's rough length has passed (its cutscenes hold
        //  still frames longer than 8 s). Rough guesses, to tune on console B.
        static const std::map<std::string, Milliseconds, std::less<>> MIN_LENGTH{
            {"opening", 60000ms},
            {"arrival_scenes", 40000ms},
            {"battle_area", 20000ms},
        };
        const auto min_length = MIN_LENGTH.find(step.name);
        mash_scene(
            console(), context(), SceneMashGoal::OVERWORLD, BUTTON_A, 180s, 8000ms,
            min_length == MIN_LENGTH.end() ? Milliseconds(0) : min_length->second
        );
    }else if (step.action == "text_to_choice"){
        mash_scene(console(), context(), SceneMashGoal::CHOICE, BUTTON_B, 60s);
    }else if (step.action == "story_battle_start"){
        mash_scene(console(), context(), SceneMashGoal::BATTLE, BUTTON_A, 90s);
    }else if (step.action == "talk_through_map_screen" || step.action == "talk_to_pokedex_screen"){
        //  B through the talk closes the map (A would place a pin) and the
        //  Pokedex entry (A only plays its motions).
        mash_scene(console(), context(), SceneMashGoal::OVERWORLD, BUTTON_B, 90s);
    }else if (step.action == "climb_ladder"){
        climb_ladder(console(), context());
    }else if (step.action == "story_battle"){
        fight_battle_in_progress(m_env.env, console(), context(), m_env.battle_ai, 120s);
    }else{
        //  story_step_supported(): any other action is a move.
        run_story_move(console(), context(), story_moves().find(step.action)->second);
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
            .id = row.id, .title = row.title, .saves = story_row_saves(row),
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
