/*  Story Action Not Implemented
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionNotImplemented_H
#define PokemonAutomation_Pokemon_StoryActionNotImplemented_H

#include <string>
#include "CommonFramework/Exceptions/OperationFailedException.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A story action the game has no code for yet, thrown with what it waits
//  for instead of pressing buttons blind. A game's backend lets it end the
//  run instead of reloading and trying again, since every attempt would
//  fail the same way.
class StoryActionNotImplemented : public OperationFailedException{
public:
    explicit StoryActionNotImplemented(std::string message);

    // code-health: allow char_pointer -- Exception::name() returns a const char pointer in the base class
    virtual const char* name() const override{
        return "StoryActionNotImplemented";
    }
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
