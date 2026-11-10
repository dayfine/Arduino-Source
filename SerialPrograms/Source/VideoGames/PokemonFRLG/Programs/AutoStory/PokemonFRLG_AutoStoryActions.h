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


//  Which row of the standard YES/NO box the arrow is on (true: YES), if
//  one shows.
std::optional<bool> yes_no_cursor(const ImageViewRGB32& screen);

//  The arrow's row on Oak's list of rival names (NEW NAME, then four preset
//  names), if the list shows. In pokefirered's oak_speech.c the list is a
//  12x10-tile window at tile (2, 2), rows 16 px apart, the cursor starting
//  on NEW NAME. The intro's YES/NO boxes open in the same corner but are
//  6x4 tiles: the list is told from them by its white past their frame.
std::optional<int> rival_name_list_cursor(const ImageViewRGB32& screen);

//  What shows in the new game's intro, as far as the names go.
enum class IntroNameScreen{
    OTHER,
    KEYBOARD,
    RIVAL_LIST,
};
//  What the new game's intro does about the names. The player's name is
//  asked on a keyboard, always first; the rival's on the name list, where
//  the first preset name is picked, as SID Helper does. A keyboard after
//  the player's is the rival's, opened because the list was missed.
enum class IntroNameStep{
    NONE,
    PLAYER_KEYBOARD,
    RIVAL_PRESET,
    RIVAL_KEYBOARD,
};
//  A preset picked returns to the list only after NO on "Er, was it ...?",
//  so after this many picks A opens the keyboard instead.
constexpr size_t RIVAL_PRESET_TRIES = 3;
//  Determines the next input step in character/rival naming sequence.
IntroNameStep intro_name_step(IntroNameScreen screen, size_t keyboards_seen, size_t presets_picked);


//  Expected screen state when dialogue text finishes.
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

//  From the overworld: SAVE, YES, and YES again to overwrite an older save
//  (see SaveDialog). The menu's cursor is read, since the game remembers it
//  and the menu grows. Returns to the overworld; throws
//  OperationFailedException if the save wasn't made.
void save_game(ConsoleHandle& console, ProControllerContext& context, StartMenu menu);

//  From the overworld: the place the START menu's SAVE card names (see
//  read_save_card_location()), then NO, back to the overworld. Empty if it
//  can't be read. The menu's length is read, not assumed, so a save made
//  with another START menu (another checkpoint's) is read too.
std::string read_save_place(ConsoleHandle& console, ProControllerContext& context, Language language);

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
    const std::vector<MartOrder>& orders, Pokemon::StoryCheckLevel checks, Language language
);

//  From the title screen of a game with no save, to the bedroom: the
//  controls pages, Oak's speech, BOY, the default name for the player (an
//  empty name confirmed) and the rival's first preset name. Returns on the
//  overworld.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
