/*  Map Util
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lookups in a map through the iterator find() returned, so a call site
 *  says which lookup it does and never copies the value by accident:
 *      const int64_t* budget = value_if_at_it<int64_t>(args, args.find("budget"));
 *
 */

#ifndef PokemonAutomation_Pokemon_MapUtil_H
#define PokemonAutomation_Pokemon_MapUtil_H

#include <variant>

namespace PokemonAutomation{
namespace Pokemon{


//  The value at `it`, or null when `it` is `map`'s end.
template <typename Map>
const typename Map::mapped_type* value_at_it(const Map& map, typename Map::const_iterator it);

//  The `Type` alternative of the variant at `it`, or null when `it` is
//  `map`'s end or the variant holds another type.
template <typename Type, typename Map>
const Type* value_if_at_it(const Map& map, typename Map::const_iterator it);


template <typename Map>
const typename Map::mapped_type* value_at_it(const Map& map, typename Map::const_iterator it){
    return it == map.end() ? nullptr : &it->second;
}

template <typename Type, typename Map>
const Type* value_if_at_it(const Map& map, typename Map::const_iterator it){
    return it == map.end() ? nullptr : std::get_if<Type>(&it->second);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
