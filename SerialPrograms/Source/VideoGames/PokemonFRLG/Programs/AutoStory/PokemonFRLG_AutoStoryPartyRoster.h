/*  AutoStory Party Roster
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which party slot holds a species, found with the party opened once.
 *  The scan opens the party, enters slot 1's summary and pages down with
 *  the D-pad, reading each member's species until one matches. In the
 *  summary's first page (pokefirered pokemon_summary_screen.c,
 *  SeekToNextMonInSingleParty()), DOWN goes to the next party member, eggs
 *  included, and does nothing on the last one: it never wraps, so the
 *  party's size bounds the scan. Leaving the summary returns to the party
 *  menu with the cursor on the last viewed member and that member's menu
 *  open again (CB2_ReturnToPartyMenuFromSummaryScreen(): slotId =
 *  GetLastViewedMonIndex(), Task_TryCreateSelectionWindow), so a field
 *  move is picked from there without closing anything.
 *
 *  The species read are kept in a PartyCache for the checkpoint: the next
 *  lookup of a species in it opens the party once and goes straight to the
 *  slot, with no summary read.
 *
 *  A field move's user is found without a summary: A on a member opens its
 *  menu, which lists its field moves in blue under SUMMARY
 *  (PartyMemberMenuReader). The scan opens the party once, walks the cursor
 *  down it, and stops at the first member whose menu lists the move; that
 *  menu is where the move is picked.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPartyRoster_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPartyRoster_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMemberMenuReader.h"
#include "PokemonFRLG_AutoStoryFieldMoveUsers.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The species of the first party slots as the last scan read them.
class PartyCache{
public:
    //  Forget the party: a deposit or withdrawal changed its order, or a
    //  save is loaded. Drops the field move users too.
    void invalidate();
    //  Forget only the species read.
    void invalidate_species();
    //  Keep `species` as slots 0, 1, ...
    void store(std::vector<std::string> species);
    //  The first slot read as `species`; nothing if none, or no scan yet.
    std::optional<size_t> find(std::string_view species) const;

    //  Who uses which field move. A load restores them from the save's
    //  copy (StoryEnv::saved_field_moves), not from nothing.
    FieldMoveUsers& users();
    const FieldMoveUsers& users() const;

private:
    std::vector<std::string> m_species;
    FieldMoveUsers m_users;
};


//  The screens a scan goes through, from the overworld.
class PartyScreens{
public:
    virtual ~PartyScreens() = default;
    //  Open the party menu with the cursor on the lead. Gives the number
    //  of members; false if it didn't open.
    virtual bool open_party(size_t& members) = 0;
    //  Slot 1's summary, first page; false if it didn't show.
    virtual bool enter_summary() = 0;
    //  The species slug on the summary now shown; empty if not read.
    virtual std::string read_species() = 0;
    //  D-pad down to the next member's summary; false if it didn't change.
    virtual bool next_member() = 0;
    //  Back out of the summary to the party menu: the member's menu is
    //  open. False if it didn't show.
    virtual bool leave_summary() = 0;
    //  From the party menu, the cursor where open_party(), the summary or
    //  the last member's menu left it: slot `slot`'s menu opened.
    virtual bool open_member_menu(size_t slot) = 0;
    //  The member's menu now open, as read; nothing if it isn't read.
    virtual std::optional<PartyMemberMenu> read_member_menu() = 0;
    //  B on the member's menu: back to the party menu, the cursor on the
    //  member. False if the menu didn't close.
    virtual bool close_member_menu() = 0;
    //  Back to the overworld from wherever the party menu is.
    virtual void close() = 0;
};


//  The party opened once and left on the menu of the first member read as
//  `species`, whose slot is given; from the cache, with no summary read, if
//  it knows the species. With `keep_menu` false the slot is only found: from
//  the cache, nothing is opened; otherwise the party is closed again before
//  it returns. If no member reads as `species`, or a screen didn't show, the
//  party is closed and the result is nothing; a failed scan leaves the cache
//  as it was.
std::optional<size_t> open_member_menu_of(
    Logger& logger, PartyScreens& screens, PartyCache& cache, std::string_view species, bool keep_menu
);


//  What use_field_move() returns when no member's menu lists the move.
inline const std::string FIELD_MOVE_NO_MEMBER = "no_member";
//  What it returns when no menu read lists the move but a member's menu
//  isn't read, or lists a field move whose name isn't read (the move may
//  be there), or a menu didn't open or close.
inline const std::string FIELD_MOVE_MENU_UNREAD = "menu_unread";
//  Runs the move from the member's menu, its row there (SUMMARY is 0)
//  given; its failure, or empty.
using FieldMoveFromMenu = std::function<std::string(size_t row)>;
//  The field move `move` (a slug, "fly") used by the first member whose
//  menu lists it: the party is opened once, the members' menus are opened
//  down the party until one lists `move` (a menu not read is closed again
//  and passed), and `from_menu` (the console's FLY row and map) runs from
//  that menu. No summary is read. Returns its
//  failure, empty on success, FIELD_MOVE_NO_MEMBER if no menu lists the
//  move, or FIELD_MOVE_MENU_UNREAD. With a record in the cache's users for
//  `move`, that slot's menu is opened first and the scan runs only if it
//  doesn't list the move (the record is dropped then). What is found is
//  recorded, with `species` (the step's name for the user) as its species.
std::string use_field_move(
    Logger& logger, PartyScreens& screens, PartyCache& cache,
    std::string_view move, std::string_view species, const FieldMoveFromMenu& from_menu
);


//  Registers this file's tests (PokemonFRLG_AutoStoryPartyRoster_Tests.cpp).
void add_tests_AutoStoryPartyRoster(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
