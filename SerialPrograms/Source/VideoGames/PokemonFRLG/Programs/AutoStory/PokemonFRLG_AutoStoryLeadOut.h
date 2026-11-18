/*  AutoStory Lead Out
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which of the player's Pokemon is out in a trainer's battle: the name its
 *  HP box shows, the party list's slot that reads the lead, and the one rule
 *  that tells a lead dragged out by Roar or Whirlwind from a faint.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeadOut_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeadOut_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where the lead is at a trainer battle's menu.
enum class LeadOnField{
    OUT,
    FAINTED,
    //  Another member is out that the battle loop didn't send: the lead is
    //  alive in the party.
    FORCED_OUT,
};

//  What the battle loop knows at a trainer battle's menu.
struct LeadOnFieldInput{
    //  The lead's HP box went (the faint watch).
    bool box_gone = false;
    //  The loop sent a member from the party list.
    bool member_sent = false;
    //  Our Pokemon out as read (read_own_battler()), "" if not read.
    std::string_view own_out = {};
    //  The lead's species.
    std::string_view lead = {};
};

//  The one rule: the member out read as the lead's species, the lead is
//  out; another member read out while none was sent from the party list,
//  it was dragged out (console B, 2026-11-16, Bruno's Onix's Roar, read as
//  a faint: the lead's box gone); else the box gone means it fainted.
LeadOnField lead_on_field(const LeadOnFieldInput& input);

//  The player's Pokemon out on `screen`, a battle menu: the name its HP
//  box shows, among battle_species(); empty if not read. After a Roar or
//  Whirlwind it is another member than the lead (console B, 2026-11-16,
//  Bruno's Onix dragged Lapras in).
std::string read_own_battler(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The slot of the party list on `screen` (from 0, the big top-left one)
//  whose name reads `species`; none if no slot does.
std::optional<size_t> party_slot_of(Logger& logger, Language language, const ImageViewRGB32& screen, std::string_view species);


//  Registers the lead-out unit tests.
void add_tests_AutoStoryLeadOut(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
