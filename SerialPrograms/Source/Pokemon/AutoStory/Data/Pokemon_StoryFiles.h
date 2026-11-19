/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The rows of a game's story a run plays.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFiles_H
#define PokemonAutomation_Pokemon_StoryFiles_H

#include <cstddef>
#include <span>

namespace PokemonAutomation{
namespace Pokemon{


//  The rows a run from `first` to `last` of `rows` plays, in order. Empty
//  if `first` comes after `last` or `last` is past the end.
template <typename Checkpoint>
std::span<Checkpoint> story_run_rows(std::span<Checkpoint> rows, size_t first, size_t last);



template <typename Checkpoint>
std::span<Checkpoint> story_run_rows(std::span<Checkpoint> rows, size_t first, size_t last){
    if (first > last || last >= rows.size()){
        return {};
    }
    return rows.subspan(first, last - first + 1);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
