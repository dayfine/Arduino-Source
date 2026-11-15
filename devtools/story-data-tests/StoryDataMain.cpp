/*  Story Data Tests Main
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Entry point of the standalone StoryDataTests: with no argument it runs
 *  the built-in story step JSON cases, with a path it checks
 *  PokemonSimulation's shared wire-case file.
 *
 */

#include <iostream>
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson_Tests.h"
#include "StoryWireCases.h"

int main(int argc, char** argv){
    using namespace PokemonAutomation::Pokemon;
    if (argc == 1){
        return run_story_step_json_cases() != 0;
    }
    if (argc == 2){
        return check_story_wire_cases(argv[1]) != 0;
    }
    std::cerr << "Usage: StoryDataTests [wire-cases.json]" << std::endl;
    return 2;
}
