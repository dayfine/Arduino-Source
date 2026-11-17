/*  AutoStory Supplies
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::string_view status_name(StatusCondition status){
    switch (status){
    case StatusCondition::NONE:
        return "no status";
    case StatusCondition::POISON:
        return "PSN";
    case StatusCondition::PARALYSIS:
        return "PAR";
    case StatusCondition::SLEEP:
        return "SLP";
    case StatusCondition::OTHER:
        return "a status";
    }
    return "?";
}

std::string_view cure_of(StatusCondition status){
    switch (status){
    case StatusCondition::SLEEP:
        return "awakening";
    case StatusCondition::POISON:
        return "antidote";
    case StatusCondition::PARALYSIS:
        return "parlyz-heal";
    case StatusCondition::NONE:
    case StatusCondition::OTHER:
        return "";
    }
    return "";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
