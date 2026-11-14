/*  Story Action Not Implemented
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Pokemon_StoryActionNotImplemented.h"

namespace PokemonAutomation{
namespace Pokemon{


StoryActionNotImplemented::StoryActionNotImplemented(std::string message)
    : OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, std::move(message))
{}



} // namespace Pokemon
} // namespace PokemonAutomation
