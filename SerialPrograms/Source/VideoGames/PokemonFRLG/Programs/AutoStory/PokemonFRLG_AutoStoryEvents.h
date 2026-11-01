/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG story as data for the shared story engine
 *  (Pokemon/AutoStory/Pokemon_StoryEvents.h, Pokemon_StorySegments.h).
 *  A checkpoint is a list of the shared StorySteps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h), each one kind of event (a
 *  route, a talk, text read to its end, a YES/NO, a battle, the nurse, the
 *  Mart); StoryExecutor does each kind on the console, and
 *  make_story_segment() turns the table into engine checkpoints. Routes
 *  plan from the pose the run carries and return where they leave the
 *  player; scenes that move the player say where in the table.
 *
 *  The table itself is in PokemonFRLG_AutoStoryTable.h; it is plain data,
 *  so its routes are planned in unit tests without a console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEvents_H

#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::StoryLeg;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;

//  Where the player stands and faces: the one-tile poses of FRLG's table
//  and routes. An unknown facing (after a scene that turns the player) is
//  set before the next route by turning to a wall.
struct StoryPose{
    std::string map;
    TilePos tile;
    std::optional<Direction> facing;
};

//  A shared checkpoint with what FRLG adds to it.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
};


//  The maps the story plans on: every map to Brock, with the objects the
//  game hides after the Hall of Fame removed, as the lab's frlg_route does.
const std::vector<MapData>& story_maps();

Pokemon::Pose to_pose(const StoryPose& pose);
//  `pose` as a StoryPose: nothing unless it is on one tile.
std::optional<StoryPose> story_pose(const Pokemon::Pose& pose);

//  The place the save card names for a save on `map` (see
//  read_save_card_location()), or empty for a map whose place isn't known.
std::string save_card_place(const std::string& map);

//  `legs` from `start` as route steps for the walker, each checked against
//  the map: nothing if a step is blocked or leaves the map. A leg ends "to
//  wall" if the next step its way is blocked.
std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
);

//  A way the player at `tile` can turn to without stepping: a wall there,
//  with no one standing in it. West and east first: walking north or south
//  into a sign would read it.
std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile);

//  `items` as orders at the Viridian Mart before the Boulder Badge: the
//  rows of POKE BALL, POTION, ANTIDOTE, PARLYZ HEAL. Nothing if it does
//  not sell one of them.
std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items);


//  What the story's events act on. `supplies` are what the bag holds now,
//  `saved_supplies` what the last save holds.
struct StoryEnv{
    ConsoleHandle& console;
    ProControllerContext& context;
    Supplies supplies;
    Supplies saved_supplies;
    //  Whether the game has a save to reload.
    bool has_save = false;
    StoryChecks checks = StoryChecks::THOROUGH;
    //  The game's language, for reading item names. In one the readers have
    //  no names for, the checks fall back to positions and counts.
    Language language = Language::English;
};

//  Does FRLG's kinds of story step on the console. A console step that
//  fails is a failed Outcome, which the segment answers by reloading.
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);

    StoryEnv& env(){
        return m_env;
    }
    //  The START menu SET_OPTIONS leaves: the checkpoint's being built.
    void set_menu(StartMenu menu){
        m_menu = menu;
    }

    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override;

    virtual Pokemon::Outcome new_game(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome options(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome answer(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome battle(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const StoryStep& step) override;
    //  The Viridian Mart, before the Boulder Badge: Potions, Antidotes and Parlyz Heals.
    virtual Pokemon::Outcome shop(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome ready(Pokemon::RunState& state, const StoryStep& step) override;

private:
    StoryEnv& m_env;
    StartMenu m_menu = StartMenu::FULL;
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor);



}
}
}
#endif
