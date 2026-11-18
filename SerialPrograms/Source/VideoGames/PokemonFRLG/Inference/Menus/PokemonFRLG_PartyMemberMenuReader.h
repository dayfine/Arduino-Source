/*  Party Member Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the menu A opens on a member in the party menu: SUMMARY, the
 *  member's field moves in blue (FLY, CUT, ...), then SWITCH, ITEM and
 *  CANCEL (pokefirered party_menu.c, SetPartyMonFieldSelectionActions()).
 *  The window grows upwards from the bottom right corner, one row of
 *  16 pixels per action, so its rows are found from CANCEL up.
 *
 *  It knows the field moves' names in English only so far: in another
 *  language a blue row is never read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PartyMemberMenuReader_H
#define PokemonAutomation_PokemonFRLG_PartyMemberMenuReader_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A member's menu as read.
struct PartyMemberMenu{
    //  The field moves' slugs ("fly", "rock-smash"), from the top; empty
    //  for a blue row whose name wasn't read.
    std::vector<std::string> field_moves = {};

    //  The row of `move`, counted from SUMMARY's row 0; nothing if the
    //  menu doesn't list it.
    std::optional<size_t> row_of(std::string_view move) const;
    //  Whether every blue row's name was read.
    bool all_read() const;
};

//  The member's menu on `screen`; nothing if the screen doesn't show one
//  (no CANCEL at the bottom right, or rows that don't make SUMMARY, field
//  moves, then two or more plain rows).
std::optional<PartyMemberMenu> read_party_member_menu(Logger& logger, Language language, const ImageViewRGB32& screen);


//  Registers this reader's tests.
void add_tests_PartyMemberMenuReader(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
