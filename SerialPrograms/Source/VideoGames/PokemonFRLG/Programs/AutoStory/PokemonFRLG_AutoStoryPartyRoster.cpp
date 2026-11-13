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

//  The recorded user's menu from the overworld, and `from_menu` from it:
//  the failure, or nothing if the record didn't give the move (the screens
//  are closed then). A party that doesn't open is no fault of the record.
std::optional<std::string> use_recorded_member(
    Logger& logger, PartyScreens& screens, const FieldMoveUser& user, std::string_view move,
    const FieldMoveFromMenu& from_menu
){
    size_t members = 0;
    if (!screens.open_party(members)){
        logger.log("Party: the menu didn't open.", COLOR_RED);
        return FIELD_MOVE_NO_MEMBER;
    }
    logger.log(std::format("Party: the {} uses {} from slot {} (recorded).", user.species, move, user.slot + 1));
    if (user.slot < members && screens.open_member_menu(user.slot)){
        std::string failure = from_menu();
        if (failure.empty()){
            return failure;
        }
    }
    logger.log(std::format("Party: slot {} didn't give {}; reading the party.", user.slot + 1, move), COLOR_RED);
    screens.close();
    return std::nullopt;
}

} // namespace

std::string use_field_move(
    Logger& logger, PartyScreens& screens, PartyCache& cache,
    std::string_view move, std::string_view species, const FieldMoveFromMenu& from_menu
){
    if (const std::optional<FieldMoveUser> user = cache.users().find(move); user && user->species == species){
        if (const std::optional<std::string> done = use_recorded_member(logger, screens, *user, move, from_menu)){
            return *done;
        }
        cache.users().forget(move);
        cache.invalidate_species();
    }
    const std::optional<size_t> slot = open_member_menu_of(logger, screens, cache, species, true);
    if (!slot){
        return FIELD_MOVE_NO_MEMBER;
    }
    cache.users().record(std::string(move), *slot, std::string(species));
    return from_menu();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
