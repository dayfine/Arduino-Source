/*  Lab Device Access (Qt process-lock seam)
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_LabDeviceAccessQt_H
#define PokemonAutomation_LabDeviceAccessQt_H

#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <QLockFile>

namespace PokemonAutomation{

// Separate namespaces for serial, video, audio and whole-profile reservations.
enum class LabDeviceKind{ SERIAL, VIDEO, AUDIO, PROFILE };

// Stable hardware identity, never a display alias, ordinal or USB port pathname.
struct LabDeviceIdentity{
    LabDeviceKind kind;
    std::string id;
};

// Immutable allowlist loaded once for the selected lab profile.
struct LabDevicePolicy{
    std::string profile;
    std::set<std::string> serial;
    std::set<std::string> video;
    std::set<std::string> audio;
};

// One currently enumerated USB port; tty/cu aliases may describe the same adapter.
struct LabSerialDevice{
    std::string port;
    unsigned vendor;
    unsigned product;
    std::string serial;
};

// Resolve a selected current port, rejecting missing or ambiguous physical identity.
std::string resolve_lab_serial_identity(std::span<const LabSerialDevice> devices, std::string_view port);

// Parse the strict schema; malformed/mismatched input throws std::runtime_error.
LabDevicePolicy parse_lab_device_policy(std::string_view json, std::string_view profile);

// Normalize VID/PID and a nonempty USB serial; empty identifiers are refused.
std::string lab_serial_identity(unsigned vendor, unsigned product, std::string_view serial);

// Compute a shared lock path independent of profile, executable and display names.
std::string lab_device_lock_path(std::string_view directory, const LabDeviceIdentity& device);

// A noncopyable process lease. Destruction releases it; dead owners are recovered by Qt.
// Keep this alive until after the native device has closed. Acquisition never waits.
class LabDeviceLease{
public:
    explicit LabDeviceLease(std::string_view path);
    LabDeviceLease(const LabDeviceLease&) = delete;
    LabDeviceLease& operator=(const LabDeviceLease&) = delete;
private:
    QLockFile m_lock;
};

// Check the allowlist before acquiring a lease. A denial never touches hardware.
// Throws std::runtime_error on denied access, contention or filesystem failure.
std::unique_ptr<LabDeviceLease> acquire_lab_device(
    const LabDevicePolicy& policy, std::string_view directory, const LabDeviceIdentity& device
);

} // namespace PokemonAutomation
#endif
