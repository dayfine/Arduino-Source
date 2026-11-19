/*  Hardware Validation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_HardwareValidation_H
#define PokemonAutomation_HardwareValidation_H

#include "CommonFramework/Startup/SetupSettings.h"

namespace PokemonAutomation{

// Check user hardware.
// If the hardware is too old, send a QMessageBox (when `dialogs == SHOW`) and return false.
// If the hardware may not be powerful enough, send a QMessageBox (when `dialogs == SHOW`).
bool check_hardware(StartupDialogs dialogs = StartupDialogs::SHOW);


}
#endif
