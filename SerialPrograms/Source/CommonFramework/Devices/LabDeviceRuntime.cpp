/*  Lab Device Runtime
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <stdexcept>
#include <vector>
#include <QDir>
#include <QFile>
#include <QSerialPortInfo>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "LabDeviceRuntime.h"

namespace PokemonAutomation{
namespace{

#if defined(__APPLE__)
// Shared by all guarded apps for this user, independent of their profile/bundle.
std::string shared_lease_directory(){
    return (QDir::homePath() + "/Library/Application Support/PokemonAutomation/LabDeviceLeases").toStdString();
}

std::string current_serial_identity(std::string_view port){
    const QSerialPortInfo selected(QString::fromUtf8(port.data(), (qsizetype)port.size()));
    std::vector<LabSerialDevice> devices;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()){
        if (info.hasVendorIdentifier() && info.hasProductIdentifier()){
            devices.emplace_back(LabSerialDevice{
                info.systemLocation().toStdString(), info.vendorIdentifier(),
                info.productIdentifier(), info.serialNumber().toStdString()
            });
        }
    }
    return resolve_lab_serial_identity(devices, selected.systemLocation().toStdString());
}

const LabDevicePolicy& runtime_policy(){
    static const LabDevicePolicy policy = []{
        const auto path = Filesystem::application_scratch_path() / "lab-devices.json";
        QFile file(QString::fromStdString(path.string()));
        if (!file.open(QIODevice::ReadOnly)){
            throw std::runtime_error("Cannot read lab-devices.json; hardware access is disabled.");
        }
        return parse_lab_device_policy(file.readAll().toStdString(), Filesystem::STARTUP_PROFILE());
    }();
    return policy;
}
#endif

} // namespace

std::unique_ptr<LabDeviceLease> reserve_lab_profile(){
#if defined(__APPLE__)
    const std::string directory = shared_lease_directory();
    if (!QDir().mkpath(QString::fromStdString(directory))){
        throw std::runtime_error("Cannot create lab profile lease directory.");
    }
    return std::make_unique<LabDeviceLease>(lab_device_lock_path(
        directory, {LabDeviceKind::PROFILE, Filesystem::STARTUP_PROFILE()}
    ));
#else
    return nullptr;
#endif
}

std::unique_ptr<LabDeviceLease> acquire_lab_serial(Logger& logger, std::string_view port){
#if defined(__APPLE__)
    try{
        // Do not trust the UI's cached port list after unplug/replug.
        return acquire_lab_device(runtime_policy(), shared_lease_directory(), {
            LabDeviceKind::SERIAL, current_serial_identity(port)
        });
    }catch (const std::runtime_error& error){
        throw UserSetupError(logger, error.what());
    }
#else
    (void)logger;
    (void)port;
    return nullptr;
#endif
}

std::unique_ptr<LabDeviceLease> acquire_lab_capture(Logger& logger, LabDeviceKind kind, std::string_view id){
#if defined(__APPLE__)
    try{
        return acquire_lab_device(runtime_policy(), shared_lease_directory(), {kind, std::string(id)});
    }catch (const std::runtime_error& error){
        throw UserSetupError(logger, error.what());
    }
#else
    (void)logger;
    (void)kind;
    (void)id;
    return nullptr;
#endif
}

} // namespace PokemonAutomation
