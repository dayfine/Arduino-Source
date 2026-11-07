/*  RNG Plan Inputs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_RngPlanInputs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

std::string trim(const std::string& text){
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos){
        return "";
    }
    const size_t last = text.find_last_not_of(' ');
    return text.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& text, char separator){
    std::vector<std::string> parts;
    size_t start = 0;
    while (true){
        const size_t next = text.find(separator, start);
        parts.push_back(trim(text.substr(start, next == std::string::npos ? std::string::npos : next - start)));
        if (next == std::string::npos){
            return parts;
        }
        start = next + 1;
    }
}

} // namespace


std::optional<uint16_t> parse_trainer_id(const std::string& text){
    const std::string digits = trim(text);
    if (digits.empty() || digits.size() > 5 || !std::all_of(digits.begin(), digits.end(), [](char ch){ return ch >= '0' && ch <= '9'; })){
        return std::nullopt;
    }
    const int value = std::stoi(digits);
    if (value > 0xffff){
        return std::nullopt;
    }
    return uint16_t(value);
}

std::optional<AdvIVs> parse_ivs(const std::string& text){
    const std::vector<std::string> parts = split(text, '/');
    if (parts.size() != 6){
        return std::nullopt;
    }
    uint8_t values[6];
    for (size_t c = 0; c < 6; c++){
        const std::string& part = parts[c];
        if (part.empty() || part.size() > 2 || !std::all_of(part.begin(), part.end(), [](char ch){ return ch >= '0' && ch <= '9'; })){
            return std::nullopt;
        }
        const int value = std::stoi(part);
        if (value > 31){
            return std::nullopt;
        }
        values[c] = uint8_t(value);
    }
    AdvIVs ivs;
    ivs.hp = values[0];
    ivs.attack = values[1];
    ivs.defense = values[2];
    ivs.spatk = values[3];
    ivs.spdef = values[4];
    ivs.speed = values[5];
    return ivs;
}

std::string ivs_to_string(const AdvIVs& ivs){
    return std::format("{}/{}/{}/{}/{}/{}", ivs.hp, ivs.attack, ivs.defense, ivs.spatk, ivs.spdef, ivs.speed);
}

std::optional<std::vector<AdvNature>> parse_natures(const std::string& text){
    std::vector<AdvNature> natures;
    if (trim(text).empty()){
        return natures;
    }
    for (const std::string& name : split(text, ',')){
        const AdvNature nature = string_to_nature(name);
        if (nature == AdvNature::Any){
            return std::nullopt;
        }
        natures.push_back(nature);
    }
    return natures;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
