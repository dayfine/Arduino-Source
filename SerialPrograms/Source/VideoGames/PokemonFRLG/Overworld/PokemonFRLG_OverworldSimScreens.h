/*  Overworld Sim Screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the simulated game shows and which watcher sees it, its wild
 *  encounters and its timings.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSimScreens_H
#define PokemonAutomation_PokemonFRLG_OverworldSimScreens_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the screen shows, as the walker's detectors see it.
enum class SimScreen{
    OVERWORLD,
    MESSAGE,        //  a white message box: a sign, a trainer
    START_MENU,
    BATTLE_FLASH,   //  a wild battle's opening flashes
    BATTLE_WIPE,    //  black, as a wild battle's wipe ends, before its text box
    BATTLE_OPENING, //  a battle's text box, empty, before the HP boxes
    BATTLE,         //  a battle's text box with the HP boxes
    FADE,           //  a warp's fade out and in
};
//  The human-readable name of a simulated screen.
std::string_view sim_screen_name(SimScreen screen);

//  What the walker watches the screen with.
enum class SimWatcher{
    FLASH,          //  BattleFlashWatcher: a battle's flashes, and a warp's fade
    BATTLE_SCREEN,  //  BattleScreenWatcher: the battle menu or an HP box
    //  BattleDialogDetector, which the walker used before BattleScreenWatcher:
    //  the battle text box alone, which black below a small room passes for.
    BATTLE_TEXT_BOX,
    MESSAGE,        //  WhiteDialogWatcher
    FADE,           //  BlackScreenWatcher: a warp's fade
};

//  A wild battle on the player's `grass_step`th step onto tall grass (from
//  1), or onto any tile of a map in SimScript::wild_floor_maps. Seen late:
//  its flashes are missed, and only the battle screen shows.
struct SimEncounter{
    int grass_step = 1;
    bool seen_late = false;
};

//  How long after a step's press each thing shows. The walker's own
//  detection delays (OverworldInterrupt::detection_delay()) are its guesses
//  at these.
struct SimTiming{
    Milliseconds tile = MovementMechanics::TILE_TIME;
    //  A tile on the Bicycle.
    Milliseconds bike_tile = MovementMechanics::BIKE_TILE_TIME;
    Milliseconds flash{300};
    Milliseconds battle_screen{3700};
    //  A wild battle's text box, before the HP boxes (battle_screen). Not
    //  set: not modelled, the battle screen shows at once.
    std::optional<Milliseconds> battle_opening;
    //  Black, from the end of a wild battle's wipe until its text box
    //  (battle_opening) or HP boxes. Not set: not modelled.
    std::optional<Milliseconds> battle_wipe;
    Milliseconds message{400};
    //  The hideout lift's ride, from the floor picked to the player moving
    //  again: 24 shakes of 3 frames for two floors (sElevatorAnimationDuration,
    //  pokefirered field_specials.c), and its chime. Not measured on the console.
    Milliseconds lift_ride{1500};
    Milliseconds fade{1000};            //  from the step's end to the new map
    //  From the fade's end, the new map fades in this long, and the game
    //  takes no input yet (fault-class sweep; BDSP's Center, lab-B
    //  2026-11-10). Not measured on FRLG: 0, none.
    Milliseconds fade_in{0};
    Milliseconds trainer_notice{1000};  //  "!" over the trainer, before they walk up
    Milliseconds trainer_battle{2000};  //  after the trainer's message closes
    //  From a battle's end on, the game ignores taps this long.
    Milliseconds battle_exit_lockout = MovementMechanics::BATTLE_EXIT_LOCKOUT;
};

//  A screen the sim showed, and when, from its start.
struct SimShown{
    Milliseconds at{0};
    SimScreen screen = SimScreen::OVERWORLD;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
