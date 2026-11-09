/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Pose;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

constexpr const char* LUMIOSE = "LumioseCity";

//  Somewhere in `map`: the game reads no position yet.
Pose in(const char* map){
    Pose pose;
    pose.map = map;
    return pose;
}

StoryStep make(StoryStepKind kind, std::string name, std::string why, std::string variant = ""){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = std::move(variant);
    return step;
}

//  A GAME step that runs the move of story_moves() named `name`.
StoryStep move(std::string name, std::string why){
    std::string variant = name;
    return make(StoryStepKind::GAME, std::move(name), std::move(why), std::move(variant));
}

StoryCheckpoint row(std::string id, std::string title, Pose start, std::vector<StoryStep> steps){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = std::move(title);
    checkpoint.start = std::move(start);
    checkpoint.steps = std::move(steps);
    return checkpoint;
}

std::vector<StoryCheckpoint> make_story_checkpoints(){
    std::vector<StoryCheckpoint> rows;
    rows.push_back(row(
        "01.01", "Mission 1: chase Pancham", in(LUMIOSE),
        {
            move("run_after_pancham", "Pancham ran off with the travel bag"),
            make(StoryStepKind::MASH, "pancham_cornered", "the scene where Pancham is cornered in the alley"),
        }
    ));
    rows.push_back(row(
        "01.02", "Mission 1: choose a starter, get the bag back", in(LUMIOSE),
        {
            make(StoryStepKind::GAME, "choose_starter", "the starter the options name", "choose_starter"),
            make(StoryStepKind::MASH, "bag_returned", "the scenes until the travel bag is back"),
        }
    ));
    return rows;
}

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> rows = make_story_checkpoints();
    return rows;
}

const std::map<std::string, StoryMove, std::less<>>& story_moves(){
    //  The scaffold's run: straight ahead for 3 s. Not measured on a console.
    static const std::map<std::string, StoryMove, std::less<>> moves{
        {"run_after_pancham", StoryMove{.heading = std::nullopt, .duration = 3000ms}},
    };
    return moves;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
