/*  PokemonLZA AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's way of doing the shared story steps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h) on the console, each kind with
 *  an action of PokemonLZA_AutoStoryActions.h:
 *
 *      ROUTE   variant "fly": fly to the target, whose map is a fast travel
 *              place's slug ("vert-pokemon-center")
 *      TALK    A at what the player faces
 *      ANSWER  the first choice for yes, the second for no
 *      MASH    B through the text ("a": A) until the overworld is back
 *      BATTLE  fight with the program's battle AI
 *      HEAL    the nurse (not implemented)
 *      SHOP    the counter (not implemented)
 *      SAVE    save where the player stands
 *      GAME    a move of story_moves(); "approach_starter" (the walk of
 *              starter_moves() to the chosen starter); "choose_0"/"choose_1"
 *              (that row of the choices on screen); "story_battle" (attack
 *              in a battle already on screen until a dialog shows); the new
 *              game's "new_game_language", "pick_look", "type_player_name";
 *              the scene mashes "mash_to_move_hints", "text_to_choice",
 *              "story_battle_start", "talk_through_map_screen",
 *              "talk_to_pokedex_screen" (mash_scene()); "climb_ladder"; or an
 *              action of story_actions_not_implemented(), which throws
 *              StoryActionNotImplemented naming the code it waits for
 *
 *  Any other kind or variant fails by name: tile walks (LEGS, STEP) mean
 *  nothing in Lumiose. A failed console step becomes a failed Outcome, so
 *  the segment reloads the row's save and tries again; an action that is not
 *  implemented ends the run instead.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryExecutor_H
#define PokemonAutomation_PokemonLZA_AutoStoryExecutor_H

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The starters Mission 1 offers. The player picks one by walking up to it
//  and talking to it.
enum class StarterChoice{
    CHIKORITA,
    TEPIG,
    TOTODILE,
};

//  What the story's steps act on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StarterChoice starter = StarterChoice::CHIKORITA;
    //  The new game's name: upper case letters and digits, as
    //  type_player_name() types them. The lab's player is Kosmos.
    std::string player_name = "KOSMOS";
    BattleAIOption& battle_ai;
};

//  The GAME steps whose console code does not exist yet, by variant, each
//  with the code it waits for. The executor throws StoryActionNotImplemented
//  with that text instead of pressing buttons blind.
const std::map<std::string, std::string, std::less<>>& story_actions_not_implemented();

//  Whether this game's executor does `step`: its kind, and its variant
//  among those the kind takes (the header comment's list).
bool story_step_supported(const Pokemon::StoryStep& step);


//  Does PLZA's kinds of story step on the console.
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override;
    //  Logs the step, fails one this game does not do, and turns a failed
    //  console step into a failed Outcome.
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

protected:
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome answer(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome battle(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome shop(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome save(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome game(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

private:
    //  Walks to the chosen starter (starter_moves()); not implemented for
    //  one whose walk was not recorded.
    void approach_starter();

    ConsoleHandle& console(){
        return m_env.env.console;
    }
    ProControllerContext& context(){
        return m_env.context;
    }

    StoryEnv& m_env;
    std::string m_checkpoint;
};


//  `rows` (kept alive by the caller) as one segment run by `executor`. Each
//  row saves at its end (SegmentHooks::save), so a failed row reloads the
//  save the row before it made; the first row's save is the user's.
Pokemon::Segment make_story_segment(const std::vector<Pokemon::StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
