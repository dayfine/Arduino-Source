/*  AutoStory Start Menu Opening
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Opening the START menu for the story's menus (SAVE, OPTION), only once
 *  the overworld is at rest.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStartMenuOpening_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStartMenuOpening_H

#include <cstddef>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a look before START shows, in the order it is judged: the START
//  menu, a battle, a box (message or choice), the overworld at rest
//  (quiet_overworld()), or anything else still moving (a warp's fade-in,
//  the map's name popping up, the player walking out of a door).
enum class StartOpenScreen{
    START_MENU,
    BATTLE,
    BOX,
    QUIET_OVERWORLD,
    SETTLING,
};
//  What open_start_menu_settled() does after a look (StartMenuOpening).
enum class StartOpenAction{
    WAIT,
    //  B: a message or a choice left open would take START's place.
    CLOSE_BOX,
    PRESS_START,
    OPENED,
    //  START does nothing in a battle; the save can't be made here.
    FAIL_BATTLE,
    //  START was pressed on a quiet overworld MAX_PRESSES times, no menu.
    FAIL_NO_MENU,
};
//  What open_start_menu_settled() does on each look until the START menu
//  shows: START only on a quiet overworld, never blind. Pressed in a
//  warp's fade-in, START is lost (19.04d, console B, 2026-11-16 04:24),
//  and the B presses after a blind START that missed can't leave a
//  battle (04:14: six misses, the save failed).
class StartMenuOpening{
public:
    static constexpr size_t MAX_PRESSES = 3;

    StartOpenAction next(StartOpenScreen screen);

private:
    size_t m_presses = 0;
};

//  From the overworld, once it is at rest: the START menu open. Throws
//  OperationFailedException (with the frame) on a battle, on no menu
//  after StartMenuOpening::MAX_PRESSES, or when the screen never settles.
void open_start_menu_settled(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
