/*  BBQ Snapshot Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <vector>
#include <utility>
#include <QFile>
#include <QTemporaryDir>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonSV_BBQSnapshot.h"
#include "PokemonSV_BBQSnapshot_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

// Capture evidence through the real logging interface without controller input.
class SnapshotTestLogger : public Logger{
public:
    virtual void log(const std::string& message, Color) override{
        messages.push_back(message);
    }
    std::vector<std::string> messages;
};

// State of the screenshot destination before the attempted write.
enum class SnapshotDestination{
    MissingDirectory,
    BlockedByFile,
};

// A quest failure and the destination state to exercise.
struct SnapshotInput{
    std::string name;
    SnapshotDestination destination;
    BBQuests quest;
    std::string reason;
};

// The path contract and outcome prefix expected in the one emitted log line.
struct SnapshotExpectation{
    bool saved;
    std::string suffix;
    std::string log_prefix;
};

// Each row keeps its input and expectation together.
struct SnapshotCase{
    SnapshotInput input;
    SnapshotExpectation expectation;
};

// Check real local image output in an automatically removed temporary directory.
class Test_BBQSnapshot : public UnitTest{
public:
    explicit Test_BBQSnapshot(SnapshotCase test_case)
        : UnitTest("PokemonSV::BBQSnapshot - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        QTemporaryDir temp;
        if (!temp.isValid()){
            return "Could not create the snapshot test directory.";
        }
        const std::string base = temp.path().toStdString() + "/BBQ";
        if (m_case.input.destination == SnapshotDestination::BlockedByFile){
            QFile block(QString::fromStdString(base));
            if (!block.open(QIODevice::WriteOnly)){
                return "Could not create the blocking-file fixture.";
            }
            block.write("not a folder");
        }
        const std::string directory = base + "/";
        ImageRGB32 frame(4, 4);
        frame.fill(0xff00ff00);
        SnapshotTestLogger logger;
        const std::string path = save_bbq_snapshot(logger, frame, directory, m_case.input.quest, m_case.input.reason);
        if (path.empty() == m_case.expectation.saved){
            return "Expected saved=" + std::to_string(m_case.expectation.saved) + "; read path [" + path + "].";
        }
        if (logger.messages.size() != 1){
            return "Expected exactly one snapshot log; read " + std::to_string(logger.messages.size()) + ".";
        }
        const std::string prefix = m_case.expectation.log_prefix + directory;
        const std::string& message = logger.messages[0];
        if (!message.starts_with(prefix) || !message.ends_with(m_case.expectation.suffix)){
            return "Expected snapshot log prefix [" + prefix + "] and suffix [" + m_case.expectation.suffix + "]; read [" + message + "].";
        }
        if (m_case.expectation.saved && (!message.ends_with(path) || !Filesystem::is_regular_file(Filesystem::Path(path)))){
            return "Expected the logged file to exist at the returned path [" + path + "].";
        }
        return true;
    }

private:
    const SnapshotCase m_case;
};

// Both original screenshot cases: a new folder and a destination blocked by a file.
const SnapshotCase SNAPSHOT_CASES[] = {
    {
        .input = {.name = "saves into a new folder", .destination = SnapshotDestination::MissingDirectory,
            .quest = BBQuests::make_tm, .reason = "verification-failed"},
        .expectation = {.saved = true, .suffix = "-make-tm-verification-failed.png",
            .log_prefix = "BBQ event=snapshot saved=1 path="},
    },
    {
        .input = {.name = "reports a failed save", .destination = SnapshotDestination::BlockedByFile,
            .quest = BBQuests::make_tm, .reason = "verification-failed"},
        .expectation = {.saved = false, .suffix = "-make-tm-verification-failed.png",
            .log_prefix = "BBQ event=snapshot saved=0 path="},
    },
};

} // namespace

void add_tests_BBQSnapshot(UnitTestDatabase& database){
    for (const SnapshotCase& test_case : SNAPSHOT_CASES){
        database.add<Test_BBQSnapshot>(test_case);
    }
}

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
