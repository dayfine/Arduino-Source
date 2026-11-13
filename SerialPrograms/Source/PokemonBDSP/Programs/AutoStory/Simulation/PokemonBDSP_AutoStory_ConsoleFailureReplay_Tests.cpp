/*  BDSP AutoStory - Console failure replay tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Every fault file (Pokemon_LegReplay.h) of a console failure runs here on
 *  the story simulation: trainers, NPCs and walls come from the map, only
 *  the file's faults are injected, and the checkpoint it names must pass.
 *  A file is red on the program that failed on the console and green on the
 *  program that fixed it. No case has code of its own.
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/Navigation/Pokemon_LegReplay.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_ConsoleFailureData.h"
#include "PokemonBDSP_AutoStory_ConsoleFailureReplay_Tests.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using Pokemon::ConsoleFailure;
using Pokemon::ReplayFault;
using Pokemon::ReplayFaultKind;
using Pokemon::StoryStep;

const StoryStep* find_step(const std::vector<StoryCheckpoint>& rows, const std::string& row, const std::string& name){
    for (const StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id != row){
            continue;
        }
        for (const StoryStep& step : checkpoint.steps){
            if (step.name == name){
                return &step;
            }
        }
    }
    return nullptr;
}

SimFade sim_fade_of(Pokemon::ReplayFade fade){
    switch (fade){
    case Pokemon::ReplayFade::UNSEEN:
        return SimFade::UNSEEN;
    case Pokemon::ReplayFade::BLACK:
        return SimFade::BLACK;
    case Pokemon::ReplayFade::DIM:
        return SimFade::DIM;
    case Pokemon::ReplayFade::WHITE:
        return SimFade::WHITE;
    }
    return SimFade::UNSEEN;
}

//  The walk that goes on past its end, as the fault says.
SimSlide slide_of(const ConsoleFailure& failure, const std::vector<StoryCheckpoint>& rows, const ReplayFault& fault){
    SimSlide slide{
        .step = fault.step, .direction = fault.direction, .tiles = fault.tiles,
        .fade = sim_fade_of(fault.fade), .banner_up = fault.banner_up,
    };
    if (fault.far_pose_of.empty()){
        return slide;
    }
    const StoryStep* landing = find_step(rows, failure.checkpoint, fault.far_pose_of);
    if (landing == nullptr || !landing->pose_after){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION,
            std::format("{}: step {} has no pose after it", failure.name, fault.far_pose_of));
    }
    slide.far = Pokemon::pose_of(*landing->pose_after);
    return slide;
}

//  The faults as the simulation's knobs.
void inject(const ConsoleFailure& failure, const std::vector<StoryCheckpoint>& rows, SimMisses& misses, SimWorldKnobs& knobs){
    if (failure.start){
        knobs.start_at = Pokemon::pose_of(Pokemon::GridPose{
            failure.start->map, {failure.start->tile}, *failure.start_facing
        });
    }
    for (const ReplayFault& fault : failure.faults){
        switch (fault.kind){
        case ReplayFaultKind::FADE_MISSED:
            misses.fade = fault.index;
            break;
        case ReplayFaultKind::BANNER_MISSED:
            misses.banner = fault.index;
            break;
        case ReplayFaultKind::MOVED_FURTHER:
            knobs.slides.push_back(slide_of(failure, rows, fault));
            break;
        case ReplayFaultKind::HOLD_THROUGH_WARP:
            knobs.hold_slides.push_back(slide_of(failure, rows, fault));
            break;
        case ReplayFaultKind::SCENE_ENDS_FURTHER:
            knobs.scene_slides.push_back(slide_of(failure, rows, fault));
            break;
        case ReplayFaultKind::PERSON_STANDS:
            knobs.standing[fault.name] = fault.tile;
            break;
        }
    }
}

struct ReplayInput{
    std::string name;
    std::string text;
};
struct ReplayExpectation{
};
struct ReplayCase{
    ReplayInput input;
    ReplayExpectation expectation;
};

//  The note the sim leaves when a fault really happened; empty for kinds with no note.
std::string fault_marker(ReplayFaultKind kind){
    switch (kind){
    case ReplayFaultKind::MOVED_FURTHER: return "[Sim] slid";
    case ReplayFaultKind::HOLD_THROUGH_WARP: return "[Sim] held through a warp";
    case ReplayFaultKind::SCENE_ENDS_FURTHER: return "[Sim] scene ended";
    default: return "";
    }
}

//  The cases are discovered from the embedded fault JSONs, so a new console failure needs no
//  C++ edit; the code-health test-case table rule does not fit a data-driven test.
class Test_ConsoleFailureReplay : public UnitTest{
public:
    explicit Test_ConsoleFailureReplay(ReplayCase test_case)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Console failure replay: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            const ConsoleFailure failure = Pokemon::parse_console_failure(m_case.input.name, m_case.input.text);
            const std::vector<StoryCheckpoint> rows = story_checkpoints();
            if (find_step(rows, failure.checkpoint, failure.failed_step) == nullptr){
                return std::format("{}: the step {} it failed at is not in {}", m_case.input.name, failure.failed_step, failure.checkpoint);
            }
            SimMisses misses{.fade = {}, .banner = {}};
            SimWorldKnobs knobs;
            inject(failure, rows, misses, knobs);
            SimExecutor executor(logger, std::move(misses), std::move(knobs));
            const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = failure.checkpoint, .to = failure.checkpoint}, rows);
            //  A fault that never happened on the sim replays nothing.
            for (const ReplayFault& fault : failure.faults){
                const std::string marker = fault_marker(fault.kind);
                if (marker.empty()){
                    continue;
                }
                const auto& notes = executor.notes();
                if (std::none_of(notes.begin(), notes.end(), [&](const std::string& note){ return note.find(marker) != std::string::npos; })){
                    return std::format("{}: the {} fault at {} never happened on the sim", m_case.input.name,
                        Pokemon::replay_fault_kind_name(fault.kind), fault.step);
                }
            }
            if (report.completed){
                return true;
            }
            return std::format("stopped in {}: {} (the console failed at {}: {})", executor.result().failed_step,
                executor.result().error, failure.failed_step, failure.failed_error);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }catch (std::exception& e){
            return std::string(e.what());
        }
    }

private:
    ReplayCase m_case;
};

} // namespace

void add_tests_AutoStory_ConsoleFailureReplay(UnitTestDatabase& database){
    std::vector<ReplayCase> cases;
    for (const auto& [name, text] : console_failure_json()){
        cases.push_back({.input = {name, text}, .expectation = {}});
    }
    for (const ReplayCase& item : cases){
        database.add<Test_ConsoleFailureReplay>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
