/*  AutoStory Save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The save dialog as the story runs it, and the place the save card
 *  names.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySave_H
#define PokemonAutomation_PokemonFRLG_AutoStorySave_H

#include <cstddef>
#include <functional>
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
#include "PokemonFRLG_AutoStoryIntro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The save dialog after SAVE, as pokefirered's start_menu.c runs it:
//  "Would you like to save the game?" YES/NO (YES first). Then, over a save
//  of this game, "There is already a saved file. Is it okay to overwrite
//  it?" YES/NO (YES first); over another game's save (a new game), "WARNING!
//  There is a different game file that is already saved." on four pages,
//  ending "...overwrite the other save file?" YES/NO with NO first. Then
//  "SAVING... DON'T TURN OFF THE POWER.", "PLAYER saved the game." and,
//  without a press, the overworld. NO at any YES/NO brings the START menu
//  back; a failed save shows "Save error." and waits for A held.
enum class SaveScreen{
    YES_NO,
    //  A message with the arrow for the next page.
    PAGED_TEXT,
    //  A message without an arrow: SAVING, or saved.
    TEXT,
    START_MENU,
    //  No message and no menu: the overworld, or a frame between two boxes.
    NOTHING,
};
//  Action to perform in the save menu loop.
enum class SaveAction{
    WAIT,
    ANSWER_YES,
    NEXT_PAGE,
    //  Confirm the overworld (START opens the menu): the game is saved.
    CHECK_OVERWORLD,
    //  The START menu is back: a YES/NO was answered NO.
    FAIL_CANCELLED,
    //  A paged message after the last YES: "Save error."
    FAIL_ERROR,
};
//  What save_game() does on each screen it reads after the save card's
//  first YES/NO shows. Every YES/NO gets YES; B is never pressed.
class SaveDialog{
public:
    SaveAction next(SaveScreen screen);

private:
    bool m_paged_since_yes = false;
    size_t m_nothing_seen = 0;
};

//  Why the save card's place (read_save_card_location(), empty if it can't
//  be read) shows the game mustn't be saved there, or empty if it may be.
using SavePlaceCheck = std::function<std::string(const std::string& place)>;

//  From the overworld: SAVE, YES, and YES again to overwrite an older save
//  (see SaveDialog). The menu's cursor is read, since the game remembers it
//  and the menu grows. Before the first YES the card's place is read in
//  `language` and given to `check`: an error answers NO, goes back to the
//  overworld and throws OperationFailedException, unsaved. Returns to the
//  overworld; throws OperationFailedException if the save wasn't made.
//  Overlaps save_game_to_overworld() in PokemonFRLG_Navigation.h.
//  Ours reads the cursor for any START menu length (the menu grows during the
//  story) and checks the overworld is back; upstream assumes the full menu.
//  Could be unified with upstream if needed.
void save_game(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu,
    Language language, const SavePlaceCheck& check
);

//  From the overworld: the place the START menu's SAVE card names (see
//  read_save_card_location()), then NO, back to the overworld. Empty if it
//  can't be read. The menu's length is read, not assumed, so a save made
//  with another START menu (another checkpoint's) is read too.
std::string read_save_place(ConsoleHandle& console, ProControllerContext& context, Language language);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
