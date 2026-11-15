/*  Story Dev Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checkpoint files that a run reads in place of the built-in ones of the
 *  same name (load_story_data() in Pokemon_StoryFiles.h). The program
 *  passes none; the tests pass their own.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryDevFiles_H
#define PokemonAutomation_Pokemon_StoryDevFiles_H

#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint file read from disk.
struct StoryDevFile{
    std::string name;   //  "plza-02.01.plan.json"
    std::string path;
    std::string text;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
