/*  AutoStory Checkpoint Id
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_CheckpointId.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  The value of two decimal digits, or nullopt if either is not a digit.
std::optional<uint8_t> parse_two_digits(std::string_view text){
    if (text.size() != 2){
        return std::nullopt;
    }
    uint8_t value = 0;
    for (char c : text){
        if (c < '0' || c > '9'){
            return std::nullopt;
        }
        value = (uint8_t)(value * 10 + (c - '0'));
    }
    return value;
}

}  // namespace



std::optional<CheckpointId> CheckpointId::parse(std::string_view text){
    if ((text.size() != 5 && text.size() != 6) || text[2] != '.'){
        return std::nullopt;
    }
    std::optional<uint8_t> segment = parse_two_digits(text.substr(0, 2));
    std::optional<uint8_t> checkpoint = parse_two_digits(text.substr(3, 2));
    if (!segment || !checkpoint){
        return std::nullopt;
    }
    char suffix = '\0';
    if (text.size() == 6){
        suffix = text[5];
        if (suffix < 'a' || suffix > 'z'){
            return std::nullopt;
        }
    }
    return CheckpointId{*segment, *checkpoint, suffix};
}

std::string CheckpointId::to_string() const{
    std::string ret = std::format("{:02}.{:02}", segment, checkpoint);
    if (suffix != '\0'){
        ret += suffix;
    }
    return ret;
}

std::string CheckpointId::segment_string() const{
    return std::format("{:02}", segment);
}


bool starts_segment(std::optional<CheckpointId> previous, CheckpointId next){
    return !previous || previous->segment != next.segment;
}



}  // namespace Pokemon
}  // namespace PokemonAutomation
