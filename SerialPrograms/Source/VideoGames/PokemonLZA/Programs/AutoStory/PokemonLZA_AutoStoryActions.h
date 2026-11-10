/*  PokemonLZA AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console actions the story executor (PokemonLZA_AutoStoryExecutor.h)
 *  is made of. Each is a short function over the LZA navigation, battle and
 *  detector code that already exists (BasicNavigation, FastTravelNavigation,
 *  TrainerBattle, the dialog and selection-arrow detectors). An action the
 *  game has no code for yet throws StoryActionNotImplemented, naming what is
 *  missing, instead of pressing buttons blind.
 *
 *  None of these has run on a console as a story step yet.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActions_H
#define PokemonAutomation_PokemonLZA_AutoStoryActions_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A story action this game has no code for yet. The story executor lets
//  it end the run instead of reloading and trying again, since every
//  attempt would fail the same way.
class StoryActionNotImplemented : public OperationFailedException{
public:
    explicit StoryActionNotImplemented(std::string message);

    virtual const char* name() const override{
        return "StoryActionNotImplemented";
    }
};

//  Throws StoryActionNotImplemented for `action`, with `todo` saying what
//  code it is waiting for.
[[noreturn]] void throw_not_implemented(ConsoleHandle& console, std::string_view action, std::string_view todo);


//  One leg of a walk: a turn, then a walk straight on.
struct StoryWalkLeg{
    //  Degrees clockwise from where the player faces (negative: left), as
    //  the story was recorded: L recenters the camera behind the player,
    //  the stick is tapped that way for 150 ms, and L again. The tap also
    //  takes a step. 0: no turn.
    double turn = 0;
    //  How long to hold the stick forward after the turn, walking (not
    //  dashing). 0: the turn only.
    Milliseconds walk = Milliseconds(0);
};

//  A walk in the overworld. Lumiose has no tiles: a move is an optional
//  compass heading to face first, then legs of turns and timed walks, as
//  recorded on the console.
struct StoryMove{
    //  Clockwise from north, as the minimap's arrow reads it
    //  (get_facing_direction()). None: no compass turn first.
    std::optional<double> heading = std::nullopt;
    std::vector<StoryWalkLeg> legs = {};
};


//  From a dialogue or a scene, presses `button` until the overworld is back
//  and holds for a second. Throws OperationFailedException if it is not back
//  within `timeout`.
void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout);

//  From the overworld, presses A at what the player faces. Returns once a
//  dialogue box shows; throws OperationFailedException if none does.
void talk_to_facing(ConsoleHandle& console, ProControllerContext& context);

//  Where a dialogue's choices are drawn, and choose_dialog_option() looks
//  for the selection arrow: the restaurant receptionist's (RestaurantFarmer).
//  Other dialogues' choices are assumed to sit there.
const ImageFloatBox& dialog_choice_box();

//  With a dialogue's choices on screen, picks choice `index` (0 is the top):
//  waits for the selection arrow, presses down until it has moved `index`
//  rows, then A. Throws OperationFailedException if the arrow never shows or
//  does not move.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index);

//  The heading the player faces, clockwise from north in [0, 360): the
//  minimap's arrow (get_facing_direction()).
double read_heading(ConsoleHandle& console, ProControllerContext& context);

//  Turns the player toward `heading`, within `tolerance` degrees: recenters
//  the camera behind the player, then taps the stick toward it and reads the
//  minimap again, a few times. Returns the heading read last; throws
//  OperationFailedException if it stays outside the tolerance.
double face_heading(ConsoleHandle& console, ProControllerContext& context, double heading, double tolerance);

//  From the overworld, walks as `move` says: faces its heading first, if it
//  has one, then each leg's turn and walk. Dash is a toggle in this game;
//  the walk assumes it is off. Nothing is read on the way: a leg that is cut
//  short or blocked goes unnoticed, and a step that needs a prompt after the
//  walk fails there instead.
void run_story_move(ConsoleHandle& console, ProControllerContext& context, const StoryMove& move);

//  From the overworld, opens the map and fast travels to `location`
//  (open_map_and_fly_to()). Returns in the overworld there; throws
//  OperationFailedException if the game would not fly (pursued, or the
//  place is not unlocked).
void fly_to(ConsoleHandle& console, ProControllerContext& context, Language language, Location location);

//  From a trainer's or a wild Pokemon's battle starting, fights it with
//  TrainerBattleState (`battle_ai`'s moves), pressing B through the text
//  around it, until the overworld is back. Throws OperationFailedException
//  if neither shows for two minutes.
void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
);

//  In a battle that is already on screen: attacks with TrainerBattleState
//  until a dialog box shows (the speech after a win or a loss). For story
//  battles, which show no Run prompt for fight_battle() to start on.
//  Throws OperationFailedException after `timeout`.
void fight_battle_in_progress(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
);

//  At a Pokemon Center: heal the party. Not implemented: throws
//  StoryActionNotImplemented.
void heal_at_pokemon_center(ConsoleHandle& console, ProControllerContext& context);

//  At a shop counter: buy `items`. Not implemented: throws
//  StoryActionNotImplemented.
void buy_items(ConsoleHandle& console, ProControllerContext& context, const std::vector<std::string>& items);

//  From the overworld, saves the game (save_game_to_menu()) and returns to
//  the overworld. Throws OperationFailedException if the save fails.
void save_story_game(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
