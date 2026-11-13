/*  AutoStory Field Move Users
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who in the party uses which field move (Fly, Surf, Strength, ...), as
 *  a TEACH of it, or a scan that found the user, learned it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFieldMoveUsers_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFieldMoveUsers_H

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Who in the party uses a field move: the slot and species it was taught
//  to. Kept with the run like the saved supplies (a save keeps a copy, a
//  load restores it), so a Fly after the TEACH opens the party once and
//  reads no summary, in a later checkpoint too.
struct FieldMoveUser{
    size_t slot = 0;
    //  Empty if the TEACH named a slot and the summary didn't read.
    std::string species = {};
};

//  The user of each field move, by the move's slug ("fly", "surf").
class FieldMoveUsers{
public:
    void record(std::string move, size_t slot, std::string species);
    std::optional<FieldMoveUser> find(std::string_view move) const;
    void forget(std::string_view move);
    //  The party order changed (a deposit or withdrawal): nobody is known.
    void clear();

private:
    std::map<std::string, FieldMoveUser, std::less<>> m_users;
};


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
