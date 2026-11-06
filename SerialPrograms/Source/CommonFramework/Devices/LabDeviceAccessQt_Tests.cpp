/*  Lab Device Access Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PR #226 startup incident: a Console B test build opened Console A's controller.
 *  These tests never enumerate or open physical devices, or load real profiles.
 */
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include "LabDeviceAccessQt.h"
#ifndef PA_LAB_DEVICE_STANDALONE_TESTS
#include "LabDeviceAccessQt_Tests.h"
#endif

namespace PokemonAutomation{
namespace{

struct PolicyInput{
    std::string_view name;
    std::string_view json;
    LabDeviceIdentity requested;
};

struct PolicyExpectation{
    int opens = 0;
};

struct PolicyCase{
    PolicyInput input;
    PolicyExpectation expectation;
};

struct SerialInput{
    std::string_view name;
    std::vector<LabSerialDevice> devices;
    std::string_view port;
};

struct SerialExpectation{
    std::optional<std::string> identity = std::nullopt;
};

struct SerialCase{
    SerialInput input;
    SerialExpectation expectation;
};

const SerialCase SERIAL_CASES[] = {
    {.input = {.name = "USB serial identity", .devices = {{"/dev/cu.A", 0x10c4, 0xea60, "A"}}, .port = "/dev/cu.A"}, .expectation = {.identity = "10c4:ea60:A"}},
    {.input = {.name = "tty and cu share a key", .devices = {{"/dev/tty.A", 0x10c4, 0xea60, "A"}, {"/dev/cu.A", 0x10c4, 0xea60, "A"}}, .port = "/dev/cu.A"}, .expectation = {.identity = "10c4:ea60:A"}},
    {.input = {.name = "two adapters with duplicate serial", .devices = {{"/dev/cu.A", 0x10c4, 0xea60, "same"}, {"/dev/cu.B", 0x10c4, 0xea60, "same"}}, .port = "/dev/cu.A"}, .expectation = {}},
    {.input = {.name = "missing USB serial", .devices = {{"/dev/cu.A", 0x10c4, 0xea60, ""}}, .port = "/dev/cu.A"}, .expectation = {}},
    {.input = {.name = "unplugged adapter", .devices = {}, .port = "/dev/cu.A"}, .expectation = {}},
    {.input = {.name = "path reused by another adapter", .devices = {{"/dev/cu.A", 0x10c4, 0xea60, "B"}}, .port = "/dev/cu.A"}, .expectation = {.identity = "10c4:ea60:B"}},
};

std::string check_serial(const SerialCase& test){
    std::optional<std::string> identity;
    try{
        identity = resolve_lab_serial_identity(test.input.devices, test.input.port);
    }catch (const std::runtime_error&){}
    if (identity != test.expectation.identity){
        return "Expected identity " + test.expectation.identity.value_or("<denied>") + "; read " + identity.value_or("<denied>");
    }
    return {};
}

const PolicyCase POLICY_CASES[] = {
    {.input = {.name = "matching serial", .json = R"({"schema":1,"profile":"lab-B","serial":["10c4:ea60:B"],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, "10c4:ea60:B"}}, .expectation = {.opens = 1}},
    {.input = {.name = "wrong console serial", .json = R"({"schema":1,"profile":"lab-B","serial":["10c4:ea60:B"],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, "10c4:ea60:A"}}, .expectation = {}},
    {.input = {.name = "camera allowed", .json = R"({"schema":1,"profile":"lab-B","serial":[],"video":["766964656f42"],"audio":[]})", .requested = {LabDeviceKind::VIDEO, "766964656f42"}}, .expectation = {.opens = 1}},
    {.input = {.name = "audio allowed", .json = R"({"schema":1,"profile":"lab-B","serial":[],"video":[],"audio":["617564696f42"]})", .requested = {LabDeviceKind::AUDIO, "617564696f42"}}, .expectation = {.opens = 1}},
    {.input = {.name = "camera not audio", .json = R"({"schema":1,"profile":"lab-B","serial":[],"video":["same-id"],"audio":[]})", .requested = {LabDeviceKind::AUDIO, "same-id"}}, .expectation = {}},
    {.input = {.name = "mismatched profile", .json = R"({"schema":1,"profile":"lab-A","serial":["port"],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "missing policy", .json = "", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "malformed policy", .json = "{", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "unsupported schema", .json = R"({"schema":2,"profile":"lab-B","serial":["port"],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "duplicate identity", .json = R"({"schema":1,"profile":"lab-B","serial":["port","port"],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "missing audio list", .json = R"({"schema":1,"profile":"lab-B","serial":["port"],"video":[]})", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
    {.input = {.name = "empty identity", .json = R"({"schema":1,"profile":"lab-B","serial":[""],"video":[],"audio":[]})", .requested = {LabDeviceKind::SERIAL, ""}}, .expectation = {}},
    {.input = {.name = "unknown policy field", .json = R"({"schema":1,"profile":"lab-B","serial":["port"],"video":[],"audio":[],"allow_all":true})", .requested = {LabDeviceKind::SERIAL, "port"}}, .expectation = {}},
};

std::string check_policy(const PolicyCase& test){
    QTemporaryDir directory;
    if (!directory.isValid()){
        return "Cannot create temporary lease directory.";
    }
    int opens = 0;
    try{
        const auto policy = parse_lab_device_policy(test.input.json, "lab-B");
        auto lease = acquire_lab_device(policy, directory.path().toStdString(), test.input.requested);
        opens++;  // Scripted native open: no real port, camera or microphone exists.
    }catch (const std::runtime_error&){}
    if (opens != test.expectation.opens){
        return "Expected opens " + std::to_string(test.expectation.opens) + "; read " + std::to_string(opens);
    }
    return {};
}

enum class LeaseScenario{ SAME_PROCESS, DIFFERENT_DEVICE, DIFFERENT_KIND, RELEASE, FAILED_OPEN, LONG_RUNNING, LIVE_CHILD, DEAD_CHILD, PROFILE, EARLY_CHILD };

struct LeaseInput{
    std::string_view name;
    LeaseScenario scenario;
};

struct LeaseExpectation{
    bool second_opens;
};

struct LeaseCase{
    LeaseInput input;
    LeaseExpectation expectation;
};

const LeaseCase LEASE_CASES[] = {
    {.input = {.name = "same device two profiles", .scenario = LeaseScenario::SAME_PROCESS}, .expectation = {.second_opens = false}},
    {.input = {.name = "different consoles", .scenario = LeaseScenario::DIFFERENT_DEVICE}, .expectation = {.second_opens = true}},
    {.input = {.name = "video and audio inputs", .scenario = LeaseScenario::DIFFERENT_KIND}, .expectation = {.second_opens = true}},
    {.input = {.name = "disconnect then reconnect", .scenario = LeaseScenario::RELEASE}, .expectation = {.second_opens = true}},
    {.input = {.name = "failed native open releases lease", .scenario = LeaseScenario::FAILED_OPEN}, .expectation = {.second_opens = true}},
#if !defined(_WIN32)
    // Windows' live lock prevents editing this timestamp fixture; exercise it on POSIX.
    {.input = {.name = "age never steals live lease", .scenario = LeaseScenario::LONG_RUNNING}, .expectation = {.second_opens = false}},
#endif
    {.input = {.name = "live second process", .scenario = LeaseScenario::LIVE_CHILD}, .expectation = {.second_opens = false}},
    {.input = {.name = "profile reserved before Qt startup", .scenario = LeaseScenario::EARLY_CHILD}, .expectation = {.second_opens = false}},
    {.input = {.name = "killed owner recovery", .scenario = LeaseScenario::DEAD_CHILD}, .expectation = {.second_opens = true}},
    {.input = {.name = "duplicate profile", .scenario = LeaseScenario::PROFILE}, .expectation = {.second_opens = false}},
};

std::string test_helper_path(){
    QString name = "LabDeviceAccessTests";
#if defined(_WIN32)
    name += ".exe";
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name).toStdString();
}

void start_lease_child(QProcess& child, const std::string& path, LeaseScenario scenario){
    const QString flag = scenario == LeaseScenario::EARLY_CHILD ? "--lease-before-qt" : "--lease-child";
    child.start(QString::fromStdString(test_helper_path()), {flag, QString::fromStdString(path)});
    if (!child.waitForStarted(5000) || !child.waitForReadyRead(5000) || child.readAllStandardOutput().trimmed() != "LOCKED"){
        throw std::runtime_error("Lease test child did not confirm ownership.");
    }
}

std::unique_ptr<LabDeviceLease> prepare_owner(LeaseScenario scenario, const std::string& path, QProcess& child){
    if (scenario == LeaseScenario::LIVE_CHILD || scenario == LeaseScenario::DEAD_CHILD || scenario == LeaseScenario::EARLY_CHILD){
        start_lease_child(child, path, scenario);
        if (scenario == LeaseScenario::DEAD_CHILD){
            child.kill();
            if (!child.waitForFinished(5000)){
                throw std::runtime_error("Child did not exit after kill.");
            }
        }
        return nullptr;
    }
    auto owner = std::make_unique<LabDeviceLease>(path);
    if (scenario == LeaseScenario::FAILED_OPEN){
        try{
            auto opening = std::move(owner);
            throw std::runtime_error("Scripted native open failed.");
        }catch (const std::runtime_error&){}
    }
    if (scenario == LeaseScenario::RELEASE){
        owner.reset();
    }
    if (scenario == LeaseScenario::LONG_RUNNING){
        QFile lock(QString::fromStdString(path));
        if (!lock.open(QIODevice::ReadOnly) || !lock.setFileTime(QDateTime::currentDateTimeUtc().addDays(-2), QFileDevice::FileModificationTime)){
            throw std::runtime_error("Could not age the lock fixture.");
        }
    }
    return owner;
}

std::string check_lease(const LeaseCase& test){
    QTemporaryDir directory;
    if (!directory.isValid()){
        return "Cannot create temporary lease directory.";
    }
    LabDevicePolicy policy{"lab-B", {"port-A", "port-B"}, {"port-A"}, {}};
    LabDeviceIdentity first{LabDeviceKind::SERIAL, "port-A"};
    if (test.input.scenario == LeaseScenario::PROFILE){
        first.kind = LabDeviceKind::PROFILE;
    }
    const std::string root = directory.path().toStdString();
    const std::string path = lab_device_lock_path(root, first);
    QProcess child;
    try{
        auto owner = prepare_owner(test.input.scenario, path, child);
        LabDeviceIdentity second = first;
        if (test.input.scenario == LeaseScenario::DIFFERENT_DEVICE){
            second.id = "port-B";
        }
        if (test.input.scenario == LeaseScenario::DIFFERENT_KIND){
            second.kind = LabDeviceKind::VIDEO;
        }
        bool opened = false;
        try{
            auto lease = second.kind == LabDeviceKind::PROFILE
                ? std::make_unique<LabDeviceLease>(lab_device_lock_path(root, second))
                : acquire_lab_device(policy, root, second);
            opened = true;
        }catch (const std::runtime_error&){}
        child.kill();
        child.waitForFinished(5000);
        if (opened != test.expectation.second_opens){
            return "Expected second opens " + std::to_string(test.expectation.second_opens) + "; read " + std::to_string(opened);
        }
    }catch (const std::runtime_error& error){
        child.kill();
        child.waitForFinished(5000);
        return error.what();
    }
    return {};
}

#ifndef PA_LAB_DEVICE_STANDALONE_TESTS
class SerialTest : public UnitTest{
public:
    explicit SerialTest(const SerialCase& test) : UnitTest("LabDevices/Serial/" + std::string(test.input.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_serial(m_test);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    const SerialCase& m_test;
};

class PolicyTest : public UnitTest{
public:
    explicit PolicyTest(const PolicyCase& test) : UnitTest("LabDevices/Policy/" + std::string(test.input.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_policy(m_test);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    const PolicyCase& m_test;
};

class LeaseTest : public UnitTest{
public:
    explicit LeaseTest(const LeaseCase& test) : UnitTest("LabDevices/Lease/" + std::string(test.input.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_lease(m_test);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    const LeaseCase& m_test;
};
#endif

} // namespace

#ifndef PA_LAB_DEVICE_STANDALONE_TESTS
void add_tests_LabDeviceAccess(UnitTestDatabase& database){
    for (const SerialCase& test : SERIAL_CASES){
        database.add<SerialTest>(test);
    }
    for (const PolicyCase& test : POLICY_CASES){
        database.add<PolicyTest>(test);
    }
    for (const LeaseCase& test : LEASE_CASES){
        database.add<LeaseTest>(test);
    }
}
#endif

} // namespace PokemonAutomation

#ifdef PA_LAB_DEVICE_STANDALONE_TESTS
// Small Qt-only runner/child process. It never links the GUI or device backends.
int main(int argc, char* argv[]){
    std::unique_ptr<PokemonAutomation::LabDeviceLease> early_lease;
    if (argc == 3 && std::string_view(argv[1]) == "--lease-before-qt"){
        early_lease = std::make_unique<PokemonAutomation::LabDeviceLease>(argv[2]);
    }
    QCoreApplication application(argc, argv);
    if (early_lease){
        std::cout << "LOCKED" << std::endl;
        return application.exec();
    }else if (argc == 3 && std::string_view(argv[1]) == "--lease-child"){
        PokemonAutomation::LabDeviceLease lease(argv[2]);
        std::cout << "LOCKED" << std::endl;
        return application.exec();
    }
    if (argc != 1){
        return 2;
    }
    int failed = 0;
    for (const auto& test : PokemonAutomation::SERIAL_CASES){
        const std::string error = PokemonAutomation::check_serial(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << std::endl;
        failed += !error.empty();
    }
    for (const auto& test : PokemonAutomation::POLICY_CASES){
        const std::string error = PokemonAutomation::check_policy(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << std::endl;
        failed += !error.empty();
    }
    for (const auto& test : PokemonAutomation::LEASE_CASES){
        const std::string error = PokemonAutomation::check_lease(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << std::endl;
        failed += !error.empty();
    }
    return failed;
}
#endif
