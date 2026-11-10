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
#include "PokemonFRLG_AutoStorySave.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  RIGHT presses that turn an option showing `current` into `target`.
//  RIGHT steps through `values` and wraps from the last to the first
//  (pokefirered option_menu.c OptionMenu_ProcessInput). Nothing if either
//  isn't one of `values`.
std::optional<size_t> option_right_presses(
    const std::vector<std::string>& values, const std::string& current, const std::string& target
);

//  From the overworld: OPTION, text speed FAST, battle scene OFF (no move
//  animations), battle style SET (no "Will you switch?" between a
//  trainer's Pokemon). Returns to the overworld. A new game keeps the
//  options of the save it replaces, so each value is read and stepped to
//  its target, then read again; one that can't be read, as in a language
//  the reader has no names for, or that isn't the target after the
//  presses, fails with a screenshot and leaves the menu open.
void set_options(ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language);

//  Facing the nurse: A, YES, the heal, and the text to its end.
void heal_at_center(ConsoleHandle& console, ProControllerContext& context);

//  One line of a Mart's BUY list: the row (from 0) and how many.
struct MartOrder{
    int row = 0;
    int quantity = 1;
    //  The item's slug for the check (see read_item_list_name()).
    std::string_view item = "";
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
    const std::vector<MartOrder>& orders, Pokemon::StoryCheckLevel checks, Language language
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
