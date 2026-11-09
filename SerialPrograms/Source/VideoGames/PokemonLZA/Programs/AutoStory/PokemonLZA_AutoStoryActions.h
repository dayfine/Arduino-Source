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

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
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


//  A straight run in the overworld. Lumiose has no tiles: a move is a
//  heading and how long to run, as measured on the console.
struct StoryMove{
    //  Clockwise from north, as the minimap's arrow reads it
    //  (get_facing_direction()). None: straight ahead of the camera.
    std::optional<double> heading;
    Milliseconds duration = Milliseconds(0);
};


//  From a dialogue or a scene, presses `button` until the overworld is back
//  and holds for a second. Throws OperationFailedException if it is not back
//  within `timeout`.
void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout);

//  From the overworld, presses A at what the player faces. Returns once a
//  dialogue box shows; throws OperationFailedException if none does.
void talk_to_facing(ConsoleHandle& console, ProControllerContext& context);

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

//  From the overworld, runs as `move` says: faces its heading first, if it
//  has one, then runs straight ahead for its duration. Throws
//  OperationFailedException if a day/night change cut the run short, since
//  where it stopped is then unknown.
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
