/*  PokemonLZA AutoStory - Step validator
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <variant>
#include "PokemonLZA_AutoStoryStepValidator.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


using Pokemon::StoryStepKind;


namespace{

using IntegerRows = std::vector<std::vector<int64_t>>;

//  What is wrong with the legs `rows` of step `step`: each a [turn, ms]
//  pair turning at most half a turn either way and walking no negative
//  time, and at least one that walks.
void add_legs_errors(std::string_view step, std::string_view key, const IntegerRows& rows, std::vector<std::string>& errors){
    if (rows.empty()){
        errors.push_back(std::format("{}: {} has no legs", step, key));
        return;
    }
    bool goes = false;
    for (size_t i = 0; i < rows.size(); i++){
        const std::vector<int64_t>& row = rows[i];
        if (row.size() != 2){
            errors.push_back(std::format("{}: {} leg {} needs 2 numbers [turn, ms], has {}", step, key, i, row.size()));
            continue;
        }
        if (row[0] < -180 || row[0] > 180){
            errors.push_back(std::format("{}: {} leg {} turns {} deg, expected -180 to 180", step, key, i, row[0]));
        }
        if (row[1] < 0){
            errors.push_back(std::format("{}: {} leg {} walks {} ms, expected 0 or more", step, key, i, row[1]));
        }
        goes = goes || row[1] > 0;
    }
    if (!goes){
        errors.push_back(std::format("{}: {} walks nowhere", step, key));
    }
}

} // namespace


//  A walk's "end" arg as the event it ends on: a fade.
std::optional<Pokemon::ScreenEvent> walk_end_event(std::string_view end){
    if (end == "fade_white"){
        return Pokemon::ScreenEvent::FADE_WHITE;
    }
    if (end == "fade_black"){
        return Pokemon::ScreenEvent::FADE_BLACK;
    }
    return std::nullopt;
}

//  What is wrong with the args of a "walk" step;
//  nothing for any other step.
std::vector<std::string> plza_step_arg_errors(const Pokemon::StoryStep& step){
    std::vector<std::string> errors;
    if (step.kind != StoryStepKind::GAME){
        return errors;
    }
    if (step.action != "walk"){
        return errors;
    }
    const auto legs = step.args.find("legs");
    if (legs == step.args.end()){
        return errors;
    }
    const IntegerRows& rows = std::get<IntegerRows>(legs->second);
    add_legs_errors(step.name, "legs", rows, errors);
    const auto end = step.args.find("end");
    if (end != step.args.end()){
        const std::string& name = std::get<std::string>(end->second);
        if (!walk_end_event(name)){
            errors.push_back(std::format("{}: end {} is not \"fade_white\" or \"fade_black\"", step.name, name));
        }
        if (rows.empty() || rows.back().size() != 2 || rows.back()[1] <= 0){
            errors.push_back(std::format("{}: the last leg has no walk to hold until the end", step.name));
        }
    }
    const auto heading = step.args.find("heading");
    if (heading != step.args.end()){
        const int64_t degrees = std::get<int64_t>(heading->second);
        if (degrees < 0 || degrees >= 360){
            errors.push_back(std::format("{}: heading {} deg, expected 0 to 359", step.name, degrees));
        }
    }
    return errors;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
