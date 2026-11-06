/*  AutoStory Teach Screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The TEACH step's game steps on the console, screen by screen as console
 *  B showed them (2026-11-03, FireRed, TM03 to a Wartortle with four
 *  moves):
 *
 *  - START, BAG: the bag opens on the pocket it was left on (ITEMS);
 *    RIGHT goes to KEY ITEMS, LEFT back (POKE BALLS is right of KEY ITEMS).
 *  - KEY ITEMS: TEACHY TV, TM CASE, HELIX FOSSIL, CANCEL. A on TM CASE:
 *    OPEN/REGISTER/CANCEL, OPEN on top; A: the TM CASE's list ("No03
 *    WATER PULSE x1", "No39 ROCK TOMB x1", CLOSE).
 *  - A on a TM: USE/GIVE/EXIT, USE on top; A: the party menu, "Teach which
 *    POKEMON?", each member ABLE or NOT ABLE, the cursor on the lead.
 *  - A on a member: "<MON> wants to learn the move <MOVE>."; with room it
 *    learns. With four: "However, <MON> already knows four moves.",
 *    "Should a move be deleted and replaced with <MOVE>?" YES/NO (YES on
 *    top), YES: "Which move should be forgotten?", then KNOWN MOVES with
 *    the four moves and the new one fifth, the red cursor on the first. A
 *    on a move forgets it: "1, 2 and... ... Poof!", "<MON> forgot <OLD>.",
 *    "And... <MON> learned <MOVE>!", which the reader doesn't place (read
 *    "other"; B pages through them), then the TM CASE's list. B instead: "Stop trying to teach <MOVE>?"
 *    YES/NO, YES: "<MON> did not learn the move <MOVE>." and the TM CASE's
 *    list again, the TM kept.
 *  - From the TM CASE's list, B closes it to the bag, and B the bag.
 *
 *  Each step looks at the screen before it presses, and stops at a screen
 *  it doesn't expect instead of pressing on. The screens come from a
 *  TeachScreens, so the steps run in tests against a fake bag, TM CASE and
 *  party menu that answer as the console did.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTeachScreens_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTeachScreens_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_TeachScreenReader.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A screen on the way, read.
struct TeachView{
    TeachScreen screen = TeachScreen::OTHER;
    //  The cursor's row, from the top.
    std::optional<size_t> cursor = std::nullopt;
    //  In the bag: the pocket's name ("items", "key-items", "poke-balls").
    std::string pocket = {};
    //  In KEY ITEMS, its items ("tm-case", ...); in the TM CASE, its TMs'
    //  moves ("water-pulse", ...): from the top to the first not read.
    std::vector<std::string> rows = {};
    //  Over the party menu: its message.
    TeachText text = TeachText::UNKNOWN;
};

//  A controller action taken while navigating teaching menus.
enum class TeachPress{
    A,
    B,
    UP,
    DOWN,
    LEFT,
    RIGHT,
};
//  The human-readable name of a teaching navigation action.
const char* teach_press_name(TeachPress press);

//  The console's screens and buttons, as the TEACH step uses them.
class TeachScreens{
public:
    virtual ~TeachScreens() = default;

    //  From the overworld: START, BAG. False if the bag didn't open.
    virtual bool open_bag() = 0;
    //  The screen, once it has stopped changing.
    virtual TeachView look() = 0;
    virtual void press(TeachPress press) = 0;
    //  B until the overworld is back. False if it didn't come back.
    virtual bool to_overworld() = 0;
    //  Saves the screen as a debug image, named `label`.
    virtual void dump_screen(std::string_view label) = 0;
};


//  The TEACH step's game steps through `screens`.
class ScreenTeachActions : public TeachActions{
public:
    ScreenTeachActions(Logger& logger, TeachScreens& screens);

    virtual ItemOpen open_item(std::string_view item, std::string_view move) override;
    virtual TeachPrompt choose_member(size_t slot) override;
    virtual ForgetEnd forget(size_t slot) override;
    virtual void stop_teaching() override;
    virtual bool leave() override;
    virtual void dump_screen(std::string_view label) override;

private:
    //  Looks until `screen` shows, `looks` times at most.
    std::optional<TeachView> wait_for(TeachScreen screen, size_t looks = 12);
    //  On `screen`: the cursor to `row`. False if it didn't get there.
    bool move_cursor_to(TeachScreen screen, size_t row);
    //  In the bag: to KEY ITEMS.
    bool to_key_items();
    //  On the list `screen`: A on the row `name`, then A on the item menu's
    //  top row (OPEN or USE). NOT_IN_CASE: the rows read haven't got it.
    ItemOpen open_row(TeachScreen screen, std::string_view name);
    //  After A on a member: what it showed.
    TeachPrompt member_answer();
    //  On a YES/NO while forgetting: the answer that goes on to the moves.
    void answer_to_forget(const TeachView& view);
    void press(TeachPress press);

    Logger& m_logger;
    TeachScreens& m_screens;
};


//  The console's screens: read with PokemonFRLG_TeachScreenReader.h in a
//  game in `language`.
class ConsoleTeachScreens : public TeachScreens{
public:
    ConsoleTeachScreens(ConsoleHandle& console, ProControllerContext& context, Language language);

    virtual bool open_bag() override;
    virtual TeachView look() override;
    virtual void press(TeachPress press) override;
    virtual bool to_overworld() override;
    virtual void dump_screen(std::string_view label) override;

private:
    //  The screen in `image`, read.
    TeachView read_view(const ImageViewRGB32& image, TeachScreen screen, std::optional<size_t> cursor);
    void log_view(const TeachView& view);

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    //  The last view logged.
    std::string m_logged;
};


//  Register teach screen navigation unit tests.
void add_tests_AutoStoryTeachScreens(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
