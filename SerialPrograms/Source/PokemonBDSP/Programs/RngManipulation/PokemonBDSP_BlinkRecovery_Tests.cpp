/*  BDSP Blink Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_Xorshift128.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_BlinkExtraction.h"
#include "PokemonBDSP_BlinkRecovery.h"
#include "PokemonBDSP_BlinkRecovery_Tests.h"
#include "PokemonBDSP_StateSolver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using Pokemon::Xorshift128;
using Pokemon::Xorshift128State;

constexpr double TEST_PERIOD_SECONDS = 1.017;
constexpr double TEST_FPS = 30.0;
constexpr double RESTING_MATCH = 0.90;
constexpr double DIP_MATCH = 0.40;
constexpr double DOUBLE_GAP_SECONDS = 0.25;

struct SyntheticStreams{
    Xorshift128State seed;
    WallClock origin;
    std::vector<std::vector<BlinkMatchSample>> matches;
    std::vector<std::vector<BlinkEvent>> truth;
};

struct StallWindow{
    double start_seconds = -1.0;
    double end_seconds = -1.0;
};

void sample_npc_dips(
    const std::vector<double>& dips,
    size_t frames,
    WallClock origin,
    const StallWindow& stall,
    std::vector<BlinkMatchSample>& out_matches
){
    for (size_t f = 0; f < frames; f++){
        const double t = (double)f / TEST_FPS;
        if (t > stall.start_seconds && t < stall.end_seconds){
            continue;
        }
        double match = RESTING_MATCH;
        for (double dip : dips){
            if (std::abs(t - dip) <= 0.04){
                match = DIP_MATCH;
                break;
            }
        }
        const WallClock ts = origin + std::chrono::duration_cast<WallDuration>(
            std::chrono::duration<double>(t)
        );
        out_matches.push_back({ts, match});
    }
}

SyntheticStreams make_synthetic_two_npc_streams(
    const Xorshift128State& seed,
    size_t ticks,
    const StallWindow& npc0_stall
){
    SyntheticStreams out;
    out.seed = seed;
    out.origin = WallClock(std::chrono::seconds(1000));
    out.matches.resize(2);
    out.truth.resize(2);

    Xorshift128 rng(seed);
    std::vector<std::vector<double>> dips(2);
    for (size_t tick = 0; tick < ticks; tick++){
        for (size_t npc = 0; npc < 2; npc++){
            const uint32_t nibble = rng.next() & 0xF;
            if (nibble == 0 || nibble == 1){
                const double t = (double)tick * TEST_PERIOD_SECONDS;
                out.truth[npc].push_back({
                    .seconds = t,
                    .type = nibble == 1 ? BlinkType::Double : BlinkType::Single,
                    .type_confident = true,
                });
                dips[npc].push_back(t);
                if (nibble == 1){
                    dips[npc].push_back(t + DOUBLE_GAP_SECONDS);
                }
            }
        }
    }

    const size_t frames = (size_t)(((double)ticks * TEST_PERIOD_SECONDS + 1.0) * TEST_FPS);
    sample_npc_dips(dips[0], frames, out.origin, npc0_stall, out.matches[0]);
    sample_npc_dips(dips[1], frames, out.origin, StallWindow{}, out.matches[1]);
    return out;
}

double find_first_double_blink(const std::vector<BlinkEvent>& events, double min_seconds){
    for (const BlinkEvent& ev : events){
        if (ev.type == BlinkType::Double && ev.seconds > min_seconds){
            return ev.seconds;
        }
    }
    return -1.0;
}

std::vector<std::vector<BlinkEvent>> extract_two_streams(const SyntheticStreams& streams){
    std::vector<std::vector<BlinkEvent>> out(2);
    for (size_t c = 0; c < 2; c++){
        out[c] = group_blinks(extract_blinks(streams.matches[c], 0.20, streams.origin));
    }
    return out;
}

BlinkSolveResult solve_two_streams(
    const std::vector<std::vector<BlinkEvent>>& streams,
    std::string& build_failure
){
    std::vector<BlinkSample> samples;
    if (!build_samples(streams, {0, 1}, TEST_PERIOD_SECONDS, samples, build_failure)){
        return BlinkSolveResult{};
    }
    return solve_state_from_samples(samples, nullptr);
}

UnitTestResult verify_solved_state(
    const Xorshift128State& seed,
    const std::vector<std::vector<BlinkEvent>>& streams,
    const BlinkSolveResult& solved
){
    if (!solved.success){
        return "expected guarded streams to solve exact state, failed: " + solved.failure_reason;
    }
    Xorshift128 expected(seed);
    const double first_blink = std::min(streams[0][0].seconds, streams[1][0].seconds);
    const uint64_t first_tick = (uint64_t)std::llround(first_blink / TEST_PERIOD_SECONDS);
    expected.advance(first_tick * 2);
    if (solved.state != expected.state()){
        return std::format(
            "solved wrong state: got {}, expected {}",
            solved.state.to_string(),
            expected.state().to_string()
        );
    }
    return true;
}

UnitTestResult test_frame_stall_downgrades_single_and_solves(Logger&){
    const Xorshift128State seed{0x12345678u, 0x9abcdef0u, 0x0fedcba9u, 0x87654321u};
    const SyntheticStreams clean = make_synthetic_two_npc_streams(seed, 220, StallWindow{});

    //  Locate a Double blink on NPC 0 and drop 95 ms of frames right across its second dip.
    const double target_double = find_first_double_blink(clean.truth[0], 5.0);
    if (target_double < 0){
        return "synthetic seed produced no Double blink on stream 0";
    }

    const StallWindow stall{
        .start_seconds = target_double + DOUBLE_GAP_SECONDS - 0.045,
        .end_seconds = target_double + DOUBLE_GAP_SECONDS + 0.050,
    };
    const SyntheticStreams stalled = make_synthetic_two_npc_streams(seed, 220, stall);
    std::vector<std::vector<BlinkEvent>> streams = extract_two_streams(stalled);

    std::string build_failure;
    const BlinkSolveResult raw_solve = solve_two_streams(streams, build_failure);
    if (!build_failure.empty()){
        return "build_samples failed on raw stalled streams: " + build_failure;
    }
    if (raw_solve.success){
        return "expected unguarded frame stall across Double blink's 2nd dip to fail solving";
    }

    BlinkTypeGuardConfig guard;
    guard.origin = stalled.origin;
    for (size_t c = 0; c < 2; c++){
        mark_uncertain_blink_types(stalled.matches[c], guard, streams[c]);
    }
    const bool downgraded = std::any_of(
        streams[0].begin(), streams[0].end(),
        [target_double](const BlinkEvent& ev){
            return std::abs(ev.seconds - target_double) < 0.15 && !ev.type_confident;
        }
    );
    if (!downgraded){
        return "expected stalled Single blink to be marked type_confident=false";
    }

    const BlinkSolveResult guarded_solve = solve_two_streams(streams, build_failure);
    if (!build_failure.empty()){
        return "build_samples failed on guarded streams: " + build_failure;
    }
    return verify_solved_state(seed, streams, guarded_solve);
}

UnitTestResult test_smooth_stream_keeps_confident_types(Logger&){
    const Xorshift128State seed{0x12345678u, 0x9abcdef0u, 0x0fedcba9u, 0x87654321u};
    const SyntheticStreams clean = make_synthetic_two_npc_streams(seed, 60, StallWindow{});
    std::vector<BlinkEvent> events = group_blinks(extract_blinks(clean.matches[0], 0.20, clean.origin));
    BlinkTypeGuardConfig guard;
    guard.origin = clean.origin;
    mark_uncertain_blink_types(clean.matches[0], guard, events);
    for (const BlinkEvent& ev : events){
        if (!ev.type_confident){
            return "smooth 30 fps stream unexpectedly cleared type_confident";
        }
    }
    return true;
}

UnitTestResult test_confirmation_tracker_drops_stuck_candidate(Logger&){
    const Xorshift128State s1{1, 2, 3, 4};
    const Xorshift128State s2{5, 6, 7, 8};
    BlinkConfirmationTracker tracker(3);

    if (tracker.on_solve_failed() != BlinkConfirmationOutcome::PENDING || tracker.has_candidate()){
        return "failed solve without candidate should stay idle";
    }
    if (tracker.on_solve_succeeded(s1, 44) != BlinkConfirmationOutcome::PENDING || !tracker.has_candidate()){
        return "first solved state should become pending candidate";
    }
    if (tracker.on_solve_failed() != BlinkConfirmationOutcome::PENDING || !tracker.has_candidate()){
        return "1st failed confirmation should keep candidate";
    }
    if (tracker.on_solve_failed() != BlinkConfirmationOutcome::PENDING || !tracker.has_candidate()){
        return "2nd failed confirmation should keep candidate";
    }
    if (tracker.on_solve_failed() != BlinkConfirmationOutcome::DROPPED || tracker.has_candidate()){
        return "3rd failed confirmation should drop stuck candidate so window trimming resumes";
    }

    if (tracker.on_solve_succeeded(s1, 45) != BlinkConfirmationOutcome::PENDING){
        return "new candidate after drop should be pending";
    }
    if (tracker.on_solve_succeeded(s2, 51) != BlinkConfirmationOutcome::CHANGED || tracker.candidate() != s2){
        return "different solved state should report CHANGED and replace candidate";
    }
    if (tracker.on_solve_succeeded(s2, 57) != BlinkConfirmationOutcome::CONFIRMED){
        return "matching solved state should report CONFIRMED";
    }
    return true;
}

struct BlinkRecoveryTestCase{
    struct Input{
        UnitTestResult (*body)(Logger&);
    };
    struct Expectation{};
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_BlinkRecovery : public UnitTest{
public:
    explicit Test_BlinkRecovery(BlinkRecoveryTestCase test_case)
        : UnitTest("PokemonBDSP::Rng - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    BlinkRecoveryTestCase m_case;
};

const BlinkRecoveryTestCase BLINK_RECOVERY_CASES[] = {
    {.name = "frame stall over Double blink downgrades bit 0 and still solves exact state", .input = {test_frame_stall_downgrades_single_and_solves}, .expectation = {}},
    {.name = "smooth 30 fps stream keeps all blink types confident", .input = {test_smooth_stream_keeps_confident_types}, .expectation = {}},
    {.name = "provisional confirmation drops stuck candidate after consecutive failures", .input = {test_confirmation_tracker_drops_stuck_candidate}, .expectation = {}},
};

} // namespace

void add_tests_BlinkRecovery(UnitTestDatabase& database){
    for (const BlinkRecoveryTestCase& item : BLINK_RECOVERY_CASES){
        database.add<Test_BlinkRecovery>(item);
    }
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
