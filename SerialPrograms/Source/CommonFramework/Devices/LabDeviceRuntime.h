/*  Lab Device Runtime
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_LabDeviceRuntime_H
#define PokemonAutomation_LabDeviceRuntime_H

#include "Common/Cpp/Logging/AbstractLogger.h"
#include "LabDeviceAccessQt.h"

namespace PokemonAutomation{

// Reserve a macOS GUI profile before loading settings; throw std::runtime_error on failure.
// The returned lease must outlive the GUI. Other platforms return no reservation.
std::unique_ptr<LabDeviceLease> reserve_lab_profile();

// Resolve a fresh USB identity and reserve it before opening the requested port.
// Missing policy/identity or contention throws UserSetupError. Non-macOS is unchanged.
std::unique_ptr<LabDeviceLease> acquire_lab_serial(Logger& logger, std::string_view port);

// Reserve a camera/audio input by the lowercase hex encoding of its Qt device ID.
// Caller holds the lease until after closing the input. Non-macOS is unchanged.
std::unique_ptr<LabDeviceLease> acquire_lab_capture(Logger& logger, LabDeviceKind kind, std::string_view id);

} // namespace PokemonAutomation
#endif
