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
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPartyRoster_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPartyRoster_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The species of the first party slots as the last scan read them.
class PartyCache{
public:
    //  Forget the party: it changed (a catch, a gift, a deposit or
    //  withdrawal) or a new checkpoint starts.
    void invalidate();
    //  Keep `species` as slots 0, 1, ...
    void store(std::vector<std::string> species);
    //  The first slot read as `species`; nothing if none, or no scan yet.
    std::optional<size_t> find(std::string_view species) const;

private:
    std::vector<std::string> m_species;
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
    //  From the party menu on the lead: slot `slot`'s menu opened.
    virtual bool open_member_menu(size_t slot) = 0;
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


//  Registers this file's tests (PokemonFRLG_AutoStoryPartyRoster_Tests.cpp).
void add_tests_AutoStoryPartyRoster(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
