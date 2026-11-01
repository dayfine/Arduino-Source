/*  Command File Parsing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading the arguments of console commands: button and D-pad names, and
 *  the numbers of a line. Splitting a line into words is in
 *  CommonFramework/Windows/LabRequest.h, shared with the lab channel.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileParsing_H
#define PokemonAutomation_NintendoSwitch_CommandFileParsing_H

#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Windows/LabRequest.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  Word `index` of a command line. Throws ParseException if it is missing.
const std::string& required_arg(const std::vector<std::string>& words, size_t index);

//  Word `index` as a number, or `fallback` if it is missing. Throws
//  ParseException if it is not a number.
double optional_number(const std::vector<std::string>& words, size_t index, double fallback);

//  Word `index` as a number of milliseconds, or `fallback` if it is missing.
//  Throws ParseException if it is not a number.
Milliseconds optional_duration(const std::vector<std::string>& words, size_t index, Milliseconds fallback);


//  A button name, or several joined with '+' (e.g. "L+R"). Throws
//  ParseException for an unknown name.
Button parse_buttons(const std::string& text);

//  "UP", "DOWN", "LEFT" or "RIGHT". Throws ParseException otherwise.
DpadPosition parse_dpad(const std::string& text);

//  The arguments of "taps DIR N [all] [hold_ms] [release_ms] [still_rmsd]":
//  N D-pad taps, each measured against the frame before it.
struct TapsArguments{
    DpadPosition direction = DPAD_NONE;
    size_t count = 0;
    bool all = false;           //  Keep tapping after two still taps in a row.
    Milliseconds hold = Milliseconds(0);
    Milliseconds release = Milliseconds(0);
    double still = 0;           //  A tap changing the screen less than this (RMSD at 192x108) didn't move.
};
TapsArguments parse_taps(const std::vector<std::string>& words);



}
}
#endif
