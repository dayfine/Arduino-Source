/*  AutoStory Start Menu
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The START menu as the story grows it, whether the story checks the
 *  screen, and whether the overworld is up.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStartMenu_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStartMenu_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether the shared check level (Types/Pokemon_StoryCheckLevel.h, set by
//  StoryRunOptions) has the story check the screen before and after it
//  acts: DEFAULT and above (TESTING, THOROUGH). Then FRLG reads a Mart
//  row's item before buying it, the bag's Items pocket after shopping (the
//  counts the story then keeps), and an item's name in the bag before
//  using it. FAST and SPEEDRUN trust the counts it keeps and the timings.
inline bool checks_screen(Pokemon::StoryCheckLevel level){
    return level <= Pokemon::StoryCheckLevel::DEFAULT;
}

//  The START menu as the story grows it. Before the starter: BAG, the
//  player, SAVE, OPTION, EXIT; the starter adds POKEMON on top, the Pokedex
//  POKEDEX above that.
enum class StartMenu{
    BEFORE_POKEMON,
    BEFORE_POKEDEX,
    FULL,
};
//  Total rows in the start menu.
int start_menu_rows(StartMenu menu);
//  Row index of the SAVE option in the start menu.
int start_menu_save_row(StartMenu menu);
//  Row index of the OPTION item in the start menu.
int start_menu_option_row(StartMenu menu);


//  Whether the overworld is up: START opens the menu only there, with no
//  scene running. If it opened, it is closed again.
bool overworld_shows(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
