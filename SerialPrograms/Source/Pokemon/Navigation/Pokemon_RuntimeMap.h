/*  Versioned Runtime Map Decoder
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_RuntimeMap_H
#define PokemonAutomation_RuntimeMap_H

#include <string_view>
#include "Common/Cpp/Json/JsonValue.h"

namespace PokemonAutomation{
namespace Pokemon{

// Decode v1/v2 runtime wire data to legacy JSON without modifying the input.
// Preserve extension metadata; reject mixed encodings, malformed pairs and unknown formats.
// Group order is NPC, item, sign, interactable; order within each group is retained.
JsonValue normalize_runtime_map(const JsonValue& json, std::string_view source_name);

} // namespace Pokemon
} // namespace PokemonAutomation
#endif
