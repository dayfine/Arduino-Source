/*  AutoStory Party Roster
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void PartyCache::invalidate(){
    m_species.clear();
    m_users.clear();
}

FieldMoveUsers& PartyCache::users(){
    return m_users;
}

const FieldMoveUsers& PartyCache::users() const{
    return m_users;
}

void PartyCache::invalidate_species(){
    m_species.clear();
}

void PartyCache::store(std::vector<std::string> species){
    m_species = std::move(species);
}

std::optional<size_t> PartyCache::find(std::string_view species) const{
    for (size_t slot = 0; slot < m_species.size(); slot++){
        if (m_species[slot] == species){
            return slot;
        }
    }
    return std::nullopt;
}


namespace{

//  With the party open: the summaries paged through until one reads as
//  `species`, left on its summary. The species read are in `read`.
std::optional<size_t> scan_summaries(
    PartyScreens& screens, size_t members, std::string_view species, std::vector<std::string>& read
){
    if (!screens.enter_summary()){
        return std::nullopt;
    }
    for (size_t slot = 0; slot < members; slot++){
        if (slot > 0 && !screens.next_member()){
            return std::nullopt;
        }
        const std::string seen = screens.read_species();
        if (seen.empty()){
            return std::nullopt;
        }
        read.push_back(seen);
        if (seen == species){
            return slot;
        }
    }
    return std::nullopt;
}

} // namespace


std::optional<size_t> open_member_menu_of(
    Logger& logger, PartyScreens& screens, PartyCache& cache, std::string_view species, bool keep_menu
){
    const std::optional<size_t> known = cache.find(species);
    if (known && !keep_menu){
        logger.log(std::format("Party: the {} is slot {} (read earlier in this checkpoint).", species, *known + 1));
        return known;
    }
    size_t members = 0;
    if (!screens.open_party(members)){
        logger.log("Party: the menu didn't open.", COLOR_RED);
        return std::nullopt;
    }
    if (known){
        logger.log(std::format("Party: the {} is slot {} (read earlier in this checkpoint).", species, *known + 1));
        if (screens.open_member_menu(*known)){
            return known;
        }
        logger.log("Party: the member's menu didn't open.", COLOR_RED);
        screens.close();
        return std::nullopt;
    }
    std::vector<std::string> read;
    const std::optional<size_t> slot = scan_summaries(screens, members, species, read);
    if (!slot){
        logger.log(std::format("Party: no {} among the {} read.", species, read.size()), COLOR_RED);
        screens.close();
        return std::nullopt;
    }
    cache.store(std::move(read));
    logger.log(std::format("Party: the {} is slot {}.", species, *slot + 1));
    if (!screens.leave_summary()){
        logger.log("Party: the summary didn't close to the member's menu.", COLOR_RED);
        screens.close();
        return std::nullopt;
    }
    if (!keep_menu){
        screens.close();
    }
    return slot;
}


namespace{

//  What a member's menu says about a field move.
struct MemberMenuCheck{
    enum class Result{
        LISTED,     //  the menu is open and lists the move at `row`
        NOT_LISTED, //  it doesn't; the menu is closed again
        UNREAD,     //  the menu, or a field move's name on it, wasn't read;
                    //  the menu is closed again
        STUCK,      //  the menu didn't open, or didn't close again
    };
    Result result = Result::STUCK;
    size_t row = 0;
};

//  With the party open: slot `slot`'s menu opened and read for `move`.
MemberMenuCheck check_member_menu(Logger& logger, PartyScreens& screens, size_t slot, std::string_view move){
    using Result = MemberMenuCheck::Result;
    if (!screens.open_member_menu(slot)){
        logger.log(std::format("Party: slot {}'s menu didn't open.", slot + 1), COLOR_RED);
        return {};
    }
    const std::optional<PartyMemberMenu> menu = screens.read_member_menu();
    if (menu){
        if (const std::optional<size_t> row = menu->row_of(move)){
            return {.result = Result::LISTED, .row = *row};
        }
    }
    if (!screens.close_member_menu()){
        logger.log(std::format("Party: slot {}'s menu didn't close.", slot + 1), COLOR_RED);
        return {};
    }
    //  A name not read may be the move.
    return {.result = menu && menu->all_read() ? Result::NOT_LISTED : Result::UNREAD};
}

} // namespace

std::string use_field_move(
    Logger& logger, PartyScreens& screens, PartyCache& cache,
    std::string_view move, std::string_view species, const FieldMoveFromMenu& from_menu
){
    using Result = MemberMenuCheck::Result;
    size_t members = 0;
    if (!screens.open_party(members)){
        logger.log("Party: the menu didn't open.", COLOR_RED);
        return FIELD_MOVE_NO_MEMBER;
    }
    //  The recorded user first, then the others from the lead.
    std::vector<size_t> order;
    if (const std::optional<FieldMoveUser> user = cache.users().find(move); user && user->slot < members){
        logger.log(std::format("Party: {} recorded for slot {} ({}).", move, user->slot + 1, user->species));
        order.push_back(user->slot);
    }
    for (size_t slot = 0; slot < members; slot++){
        if (order.empty() || order[0] != slot){
            order.push_back(slot);
        }
    }
    //  A menu not read may list the move; the scan goes on to the others.
    bool unread = false;
    for (const size_t slot : order){
        const MemberMenuCheck check = check_member_menu(logger, screens, slot, move);
        if (check.result == Result::STUCK){
            screens.close();
            return FIELD_MOVE_MENU_UNREAD;
        }
        if (check.result == Result::UNREAD){
            unread = true;
            continue;
        }
        if (check.result == Result::LISTED){
            logger.log(std::format("Party: slot {} lists {} on row {}.", slot + 1, move, check.row + 1));
            cache.users().record(std::string(move), slot, std::string(species));
            return from_menu(check.row);
        }
    }
    screens.close();
    if (unread){
        logger.log(std::format("Party: no menu read lists {}; one wasn't read.", move), COLOR_RED);
        return FIELD_MOVE_MENU_UNREAD;
    }
    logger.log(std::format("Party: no member's menu lists {}.", move), COLOR_RED);
    cache.users().forget(move);
    return FIELD_MOVE_NO_MEMBER;
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
