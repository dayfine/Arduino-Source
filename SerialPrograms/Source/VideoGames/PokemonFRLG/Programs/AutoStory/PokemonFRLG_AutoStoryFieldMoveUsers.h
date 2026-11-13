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
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    friend bool operator==(const FieldMoveUser&, const FieldMoveUser&) = default;
};

//  The user of each field move, by the move's slug ("fly", "surf").
class FieldMoveUsers{
public:
    void record(std::string move, size_t slot, std::string species);
    std::optional<FieldMoveUser> find(std::string_view move) const;
    void forget(std::string_view move);
    //  The party order changed (a deposit or withdrawal): nobody is known.
    void clear();

    //  The users as text, "fly=3:pidgey,cut=2:", slots from 1, in move
    //  order: AutoStory keeps it in its settings, as it keeps the SID, so a
    //  restart of the program knows them too.
    std::string to_text() const;

    friend bool operator==(const FieldMoveUsers&, const FieldMoveUsers&) = default;

private:
    std::map<std::string, FieldMoveUser, std::less<>> m_users;
};

//  The users as the last save holds them, and where they are kept beyond
//  the program (AutoStory's settings), called at each save.
class SavedFieldMoveUsers{
public:
    using Keep = std::function<void(const FieldMoveUsers&)>;
    SavedFieldMoveUsers() = default;
    SavedFieldMoveUsers(FieldMoveUsers users, Keep keep);

    //  A save holds `users`: kept here, and by `keep` if there is one.
    void save(const FieldMoveUsers& users);
    const FieldMoveUsers& users() const;

private:
    FieldMoveUsers m_users;
    Keep m_keep;
};

//  The users read back from to_text()'s text, and the entries that weren't
//  readable (a slot outside 1-6, a move or species that isn't a slug, no
//  "="): those are dropped, so their moves are found by a party scan.
struct ParsedFieldMoveUsers{
    FieldMoveUsers users = {};
    std::vector<std::string> rejected = {};
};
//  `text` read as ParsedFieldMoveUsers says; never throws.
ParsedFieldMoveUsers parse_field_move_users(std::string_view text);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
