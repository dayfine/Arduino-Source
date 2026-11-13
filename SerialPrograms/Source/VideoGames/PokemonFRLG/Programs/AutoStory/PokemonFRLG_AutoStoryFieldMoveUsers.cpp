/*  AutoStory Field Move Users
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <format>
#include <system_error>
#include <utility>
#include "PokemonFRLG_AutoStoryFieldMoveUsers.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void FieldMoveUsers::record(std::string move, size_t slot, std::string species){
    m_users[std::move(move)] = FieldMoveUser{.slot = slot, .species = std::move(species)};
}

std::optional<FieldMoveUser> FieldMoveUsers::find(std::string_view move) const{
    const auto found = m_users.find(move);
    if (found == m_users.end()){
        return std::nullopt;
    }
    return found->second;
}

void FieldMoveUsers::forget(std::string_view move){
    const auto found = m_users.find(move);
    if (found != m_users.end()){
        m_users.erase(found);
    }
}

void FieldMoveUsers::clear(){
    m_users.clear();
}

std::string FieldMoveUsers::to_text() const{
    std::string text;
    for (const auto& [move, user] : m_users){
        if (!text.empty()){
            text += ",";
        }
        text += std::format("{}={}:{}", move, user.slot + 1, user.species);
    }
    return text;
}

SavedFieldMoveUsers::SavedFieldMoveUsers(FieldMoveUsers users, Keep keep)
    : m_users(std::move(users))
    , m_keep(std::move(keep))
{}

void SavedFieldMoveUsers::save(const FieldMoveUsers& users){
    m_users = users;
    if (m_keep){
        m_keep(users);
    }
}

const FieldMoveUsers& SavedFieldMoveUsers::users() const{
    return m_users;
}


namespace{

//  The game's party: slots 1 to 6 in the text.
constexpr size_t PARTY_SLOTS = 6;

std::string_view trimmed(std::string_view text){
    while (!text.empty() && text.front() == ' '){
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' '){
        text.remove_suffix(1);
    }
    return text;
}

//  A move or species slug: lower-case letters, digits and '-'.
bool is_slug(std::string_view text){
    for (char c : text){
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')){
            return false;
        }
    }
    return true;
}

//  "move=slot:species" as its move and user; nothing if it isn't readable.
std::optional<std::pair<std::string, FieldMoveUser>> parse_entry(std::string_view entry){
    const size_t equals = entry.find('=');
    const size_t colon = entry.find(':', equals == std::string_view::npos ? 0 : equals);
    if (equals == std::string_view::npos || colon == std::string_view::npos){
        return std::nullopt;
    }
    const std::string_view move = trimmed(entry.substr(0, equals));
    const std::string_view slot_text = trimmed(entry.substr(equals + 1, colon - equals - 1));
    const std::string_view species = trimmed(entry.substr(colon + 1));
    size_t slot = 0;
    const auto [end, error] = std::from_chars(slot_text.data(), slot_text.data() + slot_text.size(), slot);
    if (move.empty() || !is_slug(move) || !is_slug(species)
        || error != std::errc{} || end != slot_text.data() + slot_text.size() || slot < 1 || slot > PARTY_SLOTS
    ){
        return std::nullopt;
    }
    return std::pair{std::string(move), FieldMoveUser{.slot = slot - 1, .species = std::string(species)}};
}

} // namespace

ParsedFieldMoveUsers parse_field_move_users(std::string_view text){
    ParsedFieldMoveUsers parsed;
    while (!text.empty()){
        const size_t comma = text.find(',');
        const std::string_view entry = trimmed(text.substr(0, comma));
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
        if (entry.empty()){
            continue;
        }
        if (std::optional<std::pair<std::string, FieldMoveUser>> user = parse_entry(entry)){
            parsed.users.record(std::move(user->first), user->second.slot, std::move(user->second.species));
        }else{
            parsed.rejected.emplace_back(entry);
        }
    }
    return parsed;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
