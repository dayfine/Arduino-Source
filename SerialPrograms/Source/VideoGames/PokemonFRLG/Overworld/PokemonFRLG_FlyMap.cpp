/*  Fly Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_FlyMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const std::vector<FlyTown>& kanto_fly_towns(){
    static const std::vector<FlyTown> TOWNS{
        {"PalletTown",      {4, 11},    {"PalletTown", {6, 8}}},
        {"ViridianCity",    {4, 8},     {"ViridianCity", {26, 27}}},
        {"PewterCity",      {4, 4},     {"PewterCity", {17, 26}}},
        {"Route4",          {8, 3},     {"Route4", {12, 6}}},
        {"CeruleanCity",    {14, 3},    {"CeruleanCity", {22, 20}}},
        {"VermilionCity",   {14, 9},    {"VermilionCity", {15, 7}}},
        {"Route10",         {18, 3},    {"Route10", {13, 21}}},
        {"LavenderTown",    {18, 6},    {"LavenderTown", {6, 6}}},
        {"CeladonCity",     {11, 6},    {"CeladonCity", {48, 12}}},
        {"SaffronCity",     {14, 6},    {"SaffronCity", {24, 39}}},
        {"FuchsiaCity",     {12, 12},   {"FuchsiaCity", {25, 32}}},
        {"CinnabarIsland",  {4, 14},    {"CinnabarIsland", {14, 12}}},
        {"IndigoPlateau",   {2, 3},     {"IndigoPlateau_Exterior", {11, 7}}},
    };
    return TOWNS;
}

std::optional<size_t> fly_town_index(std::string_view name){
    const std::vector<FlyTown>& towns = kanto_fly_towns();
    for (size_t c = 0; c < towns.size(); c++){
        if (towns[c].name == name){
            return c;
        }
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
