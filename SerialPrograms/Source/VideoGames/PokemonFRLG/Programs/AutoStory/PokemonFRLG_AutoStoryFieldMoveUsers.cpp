/*  AutoStory Field Move Users
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
    return "";
}

ParsedFieldMoveUsers parse_field_move_users(std::string_view){
    return {};
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
