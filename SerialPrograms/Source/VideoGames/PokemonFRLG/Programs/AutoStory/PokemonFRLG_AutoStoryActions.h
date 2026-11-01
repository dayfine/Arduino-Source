/*  AutoStory Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console steps the FRLG story is made of, as a by-hand run from a new
 *  game to Brock found them (console B, 2026-11-01): reading text to its end,
 *  YES/NO prompts, saving from the START menu, the nurse, the Viridian Mart,
 *  and the new game's intro. Story text is advanced with B, which never
 *  starts a talk; the overworld is back once START opens the menu, which it
 *  doesn't while a scene runs.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryActions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryActions_H

#include <optional>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How much the story checks the screen before and after it acts.
//  THOROUGH: reads a Mart row's item before buying it, the bag's Items
//  pocket after shopping (the counts the story then keeps), and an item's
//  name in the bag before using it. FAST trusts the counts it keeps and the
//  timings.
enum class StoryChecks{
    THOROUGH,
    FAST,
};

//  The START menu as the story grows it. Before the starter: BAG, the
//  player, SAVE, OPTION, EXIT; the starter adds POKEMON on top, the Pokedex
//  POKEDEX above that.
enum class StartMenu{
    BEFORE_POKEMON,
    BEFORE_POKEDEX,
    FULL,
};
int start_menu_rows(StartMenu menu);
int start_menu_save_row(StartMenu menu);
int start_menu_option_row(StartMenu menu);


//  Whether the overworld is up: START opens the menu only there, with no
//  scene running. If it opened, it is closed again.
bool overworld_shows(ConsoleHandle& console, ProControllerContext& context);


//  Which row of the standard YES/NO box the arrow is on (true: YES), if
//  one shows.
std::optional<bool> yes_no_cursor(const ImageViewRGB32& screen);


enum class TextEnd{
    OVERWORLD,
    BATTLE,
};

//  Presses B until the overworld is back or, if `stop_at_battle`, a battle
//  shows. Throws OperationFailedException after `timeout`.
TextEnd advance_text(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout, bool stop_at_battle);

//  advance_text() to the overworld.
void mash_until_overworld(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout);

//  Advances text with `advance` until a YES/NO shows, then answers it. B
//  answers NO. Throws OperationFailedException after `timeout`.
void answer_yes_no(ConsoleHandle& console, ProControllerContext& context, bool yes, Button advance, Milliseconds timeout);

//  From the overworld: one step `direction`, onto a trigger or into a door.
//  `facing` is the way the player faces, if known; a player facing another
//  way is turned first.
void step_once(ProControllerContext& context, Pokemon::Direction direction, std::optional<Pokemon::Direction> facing);

//  From the overworld, turns to face `direction` without stepping: the
//  player must have a wall that way.
void face_wall(ProControllerContext& context, Pokemon::Direction direction);

//  From the overworld: SAVE, YES, and YES again to overwrite an older save.
//  The menu's cursor is read, since the game remembers it and the menu
//  grows. Returns to the overworld.
void save_game(ConsoleHandle& console, ProControllerContext& context, StartMenu menu);

//  From the overworld: OPTION, text speed FAST, battle style SET (no
//  "Will you switch?" between a trainer's Pokemon). Returns to the
//  overworld.
void set_options(ConsoleHandle& console, ProControllerContext& context, StartMenu menu);

//  Facing the nurse: A, YES, the heal, and the text to its end.
void heal_at_center(ConsoleHandle& console, ProControllerContext& context);

//  One line of a Mart's BUY list: the row (from 0) and how many.
struct MartOrder{
    int row = 0;
    int quantity = 1;
    //  The item's slug for the check (see read_item_list_name()).
    const char* item = "";
};

//  Down (positive) or up (negative) presses to move a Mart list's cursor
//  from `cursor` to `row`. After a purchase the cursor stays on its row.
int mart_cursor_moves(int cursor, int row);

//  Facing a Mart clerk: BUY each order, then out to the overworld.
//  THOROUGH: each row's item is read before buying; one that reads as
//  another item fails with a screenshot, one that can't be read is only
//  logged.
//  `language` is the game's: in one the reader has no names for, rows are
//  bought by position only.
void buy_items(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<MartOrder>& orders, StoryChecks checks, Language language
);

//  From the title screen of a game with no save, to the bedroom: the
//  controls pages, Oak's speech, BOY, and the default names for the player
//  and the rival (an empty name confirmed). Returns on the overworld.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context);



}
}
}
#endif
