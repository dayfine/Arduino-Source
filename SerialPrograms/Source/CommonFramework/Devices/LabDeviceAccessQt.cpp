/*  Lab Device Access (Qt process-lock seam)
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "LabDeviceAccessQt.h"

namespace PokemonAutomation{
namespace{

QString text(std::string_view value){
    return QString::fromUtf8(value.data(), (qsizetype)value.size());
}

std::set<std::string> read_ids(const QJsonObject& object, const char* key){
    if (!object.value(key).isArray()){
        throw std::runtime_error(std::string("Expected device array: ") + key);
    }
    std::set<std::string> ids;
    for (const QJsonValue& value : object.value(key).toArray()){
        const std::string id = value.toString().toStdString();
        if (!value.isString() || id.empty() || id.find('\0') != std::string::npos || !ids.emplace(id).second){
            throw std::runtime_error(std::string("Empty, invalid or duplicate device ID in ") + key);
        }
    }
    return ids;
}

bool permitted(const LabDevicePolicy& policy, const LabDeviceIdentity& device){
    switch (device.kind){
    case LabDeviceKind::SERIAL: return policy.serial.contains(device.id);
    case LabDeviceKind::VIDEO: return policy.video.contains(device.id);
    case LabDeviceKind::AUDIO: return policy.audio.contains(device.id);
    case LabDeviceKind::PROFILE: return false;
    }
    return false;
}

//  Capture IDs are allowlisted as hex; show the readable ID beside it so a
//  refusal names the device the settings selected.
std::string readable_capture_id(const std::string& hex){
    const bool is_hex = hex.size() % 2 == 0 && std::ranges::all_of(hex, [](char c){
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
    if (!is_hex){
        return hex;
    }
    const QByteArray bytes = QByteArray::fromHex(QByteArray(hex.data(), (qsizetype)hex.size()));
    return bytes.toStdString() + " (allowlist ID " + hex + ")";
}

std::string describe(const LabDeviceIdentity& device){
    switch (device.kind){
    case LabDeviceKind::SERIAL: return "Serial device " + device.id;
    case LabDeviceKind::VIDEO: return "Video input " + readable_capture_id(device.id);
    case LabDeviceKind::AUDIO: return "Audio input " + readable_capture_id(device.id);
    case LabDeviceKind::PROFILE: return "Profile " + device.id;
    }
    return device.id;
}

std::string normalize_serial_path(std::string_view path){
    if (path.starts_with("/dev/tty.")){
        return "/dev/cu." + std::string(path.substr(9));
    }
    return std::string(path);
}

} // namespace

LabDevicePolicy parse_lab_device_policy(std::string_view json, std::string_view profile){
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(json.data(), (qsizetype)json.size()), &error);
    const QJsonObject object = document.object();
    const std::set<QString> keys{"schema", "profile", "serial", "video", "audio"};
    for (auto it = object.begin(); it != object.end(); ++it){
        if (!keys.contains(it.key())){
            throw std::runtime_error("Unknown field in lab device policy.");
        }
    }
    if (error.error != QJsonParseError::NoError || !document.isObject() || object.size() != 5 ||
        !object.value("schema").isDouble() || object.value("schema").toDouble() != 1 ||
        profile.empty() || object.value("profile").toString() != text(profile)){
        throw std::runtime_error("Missing, malformed or mismatched lab device policy.");
    }
    return {std::string(profile), read_ids(object, "serial"), read_ids(object, "video"), read_ids(object, "audio")};
}

std::string lab_serial_identity(unsigned vendor, unsigned product, std::string_view serial){
    if (vendor > 0xffff || product > 0xffff || serial.empty() || serial.find('\0') != std::string_view::npos){
        throw std::runtime_error("A unique USB serial, vendor and product identity are required.");
    }
    return std::format("{:04x}:{:04x}:{}", vendor, product, serial);
}

std::string resolve_lab_serial_identity(std::span<const LabSerialDevice> devices, std::string_view port){
    const std::string path = normalize_serial_path(port);
    const auto selected = std::ranges::find_if(devices, [&](const LabSerialDevice& device){
        return normalize_serial_path(device.port) == path;
    });
    if (selected == devices.end()){
        throw std::runtime_error("Controller has no current USB identity; hardware access is disabled.");
    }
    const std::string identity = lab_serial_identity(selected->vendor, selected->product, selected->serial);
    for (const LabSerialDevice& other : devices){
        if (normalize_serial_path(other.port) != path && !other.serial.empty() &&
            lab_serial_identity(other.vendor, other.product, other.serial) == identity){
            throw std::runtime_error("Two adapters report the same USB identity; refusing ambiguous ownership.");
        }
    }
    return identity;
}

std::string lab_device_lock_path(std::string_view directory, const LabDeviceIdentity& device){
    if (directory.empty() || device.id.empty()){
        throw std::runtime_error("Device lease directory and identity must be nonempty.");
    }
    const QByteArray key = QByteArray::number((int)device.kind) + ':' + QByteArray(device.id.data(), (qsizetype)device.id.size());
    const QByteArray digest = QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
    return QDir(text(directory)).filePath(QString::fromLatin1(digest) + ".lock").toStdString();
}

LabDeviceLease::LabDeviceLease(std::string_view path)
    : m_lock(text(path))
{
    // Age alone must never evict an active multi-hour lab run.
    m_lock.setStaleLockTime(std::chrono::milliseconds::zero());
    if (!m_lock.tryLock(std::chrono::milliseconds::zero())){
        throw std::runtime_error("Device/profile is already leased, or its lock cannot be created. Nothing was opened.");
    }
}

std::unique_ptr<LabDeviceLease> acquire_lab_device(
    const LabDevicePolicy& policy, std::string_view directory, const LabDeviceIdentity& device
){
    if (!permitted(policy, device)){
        throw std::runtime_error(describe(device) + " is not allowlisted for profile " + policy.profile + ". Nothing was opened.");
    }
    if (!QDir().mkpath(text(directory))){
        throw std::runtime_error("Cannot create shared device lease directory. Nothing was opened.");
    }
    return std::make_unique<LabDeviceLease>(lab_device_lock_path(directory, device));
}

} // namespace PokemonAutomation
