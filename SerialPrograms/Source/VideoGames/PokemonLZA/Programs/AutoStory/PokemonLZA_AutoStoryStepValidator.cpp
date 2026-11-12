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

using TimedLegs = std::vector<Pokemon::StoryTimedLeg>;

//  What is wrong with the legs `rows` of step `step`: each turning at most
//  half a turn either way and walking no negative time, and at least one
//  that walks.
void add_legs_errors(std::string_view step, std::string_view key, const TimedLegs& rows, std::vector<std::string>& errors){
    if (rows.empty()){
        errors.push_back(std::format("{}: {} has no legs", step, key));
        return;
    }
    bool goes = false;
    for (size_t i = 0; i < rows.size(); i++){
        const Pokemon::StoryTimedLeg& row = rows[i];
        if (row.turn < -180 || row.turn > 180){
            errors.push_back(std::format("{}: {} leg {} turns {} deg, expected -180 to 180", step, key, i, row.turn));
        }
        if (row.ms < 0){
            errors.push_back(std::format("{}: {} leg {} walks {} ms, expected 0 or more", step, key, i, row.ms));
        }
        goes = goes || row.ms > 0;
    }
    if (!goes){
        errors.push_back(std::format("{}: {} walks nowhere", step, key));
    }
}

} // namespace


//  A walk's "end" arg as the event it ends on: a fade.
std::optional<PokemonAutomation::ScreenEvent> walk_end_event(std::string_view end){
    if (end == "fade_white"){
        return PokemonAutomation::ScreenEvent::FADE_WHITE;
    }
    if (end == "fade_black"){
        return PokemonAutomation::ScreenEvent::FADE_BLACK;
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
    const TimedLegs& rows = std::get<TimedLegs>(legs->second);
    add_legs_errors(step.name, "legs", rows, errors);
    const auto end = step.args.find("end");
    if (end != step.args.end()){
        const std::string& name = std::get<std::string>(end->second);
        if (!walk_end_event(name)){
            errors.push_back(std::format("{}: end {} is not \"fade_white\" or \"fade_black\"", step.name, name));
        }
        if (rows.empty() || rows.back().ms <= 0){
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
