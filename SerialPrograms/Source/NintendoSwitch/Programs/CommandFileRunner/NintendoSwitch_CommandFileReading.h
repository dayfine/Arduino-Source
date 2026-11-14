/*  Command File Runner - Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A reading is a value a game reads from a frame rather than yes or no,
 *  such as the heading on a minimap. "detect NAME" prints it, and every
 *  command's line in the steps file (NintendoSwitch_CommandFileRecorder.h)
 *  records the games' readings before and after it.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileReading_H
#define PokemonAutomation_NintendoSwitch_CommandFileReading_H

#include <functional>
#include <string>
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  A value read from a frame: a number, a string or a bool, or null when
//  the frame shows none.
using CommandFileReading = std::function<JsonValue(ConsoleHandle& console, const ImageViewRGB32& screen)>;

//  Adds a reading, by name. Games call it from their panel list's
//  constructor, as they register their commands and detectors.
void register_command_file_reading(std::string name, CommandFileReading reading);



}
}
#endif
