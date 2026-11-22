/*  BDSP Blink Extraction Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <stdint.h>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "PokemonBDSP_BlinkExtraction.h"
#include "PokemonBDSP_BlinkExtraction_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

constexpr double STREAM_LEAD_SECONDS = 1.0;
constexpr double HARDWARE_TICK_SECONDS = 1.0197;
constexpr double PERIOD_TOLERANCE_SECONDS = 0.002;
constexpr double MAX_TICK_FIT_RMS = 0.05;
const Pokemon::Xorshift128State TEST_SEED(0x12345678, 0x9abcdef0, 0x13579bdf, 0x2468ace0);

std::vector<BlinkEvent> truth_events_for_slot(size_t ticks, uint8_t npcs, uint8_t slot){
    const std::vector<BlinkTick> schedule = generate_blink_ticks(Pokemon::Xorshift128(TEST_SEED), ticks, npcs, slot);
    std::vector<BlinkEvent> events;
    for (size_t tick = 0; tick < schedule.size(); tick++){
        if (schedule[tick].blinked){
            events.emplace_back(BlinkEvent{STREAM_LEAD_SECONDS + (double)tick * BDSP_NPC_TICK_SECONDS, schedule[tick].type});
        }
    }
    return events;
}

Pokemon::Xorshift128State expected_first_blink_state(size_t ticks, uint8_t npcs){
    Pokemon::Xorshift128 scanner(TEST_SEED);
    size_t first_tick = ticks;
    for (size_t tick = 0; tick < ticks && first_tick == ticks; tick++){
        for (uint8_t slot = 0; slot < npcs; slot++){
            if (npc_blinks(scanner.next())){
                first_tick = tick;
                break;
            }
        }
    }
    Pokemon::Xorshift128 expected(TEST_SEED);
    expected.advance(first_tick * npcs);
    return expected.state();
}

std::string verify_tick_fit(const TickFit& fit, double expected_period, double max_rms_ticks = MAX_TICK_FIT_RMS){
    if (std::abs(fit.period_seconds - expected_period) > PERIOD_TOLERANCE_SECONDS){
        return std::format("Expected period {:.4f}s (+/-{:.4f}s), got {:.4f}s.", expected_period, PERIOD_TOLERANCE_SECONDS, fit.period_seconds);
    }
    if (fit.rms_ticks > max_rms_ticks){
        return std::format("Expected tick fit rms_ticks <= {:.4f}, got {:.4f}.", max_rms_ticks, fit.rms_ticks);
    }
    return {};
}

UnitTestResult verify_solve_result(const BlinkSolveResult& solved, bool expect_solved, const Pokemon::Xorshift128State& expected_state){
    if (solved.success != expect_solved){
        return std::format("Expected solve success={}, got {} (reason: {}).", expect_solved, solved.success, solved.failure_reason);
    }
    if (!expect_solved){
        if (solved.equations_used < 128){
            return std::format("Expected >= 128 equations (GF(2) contradiction), got {}.", solved.equations_used);
        }
        return solved.failure_reason.empty() ? UnitTestResult("Expected non-empty failure_reason.") : UnitTestResult(true);
    }
    if (solved.state != expected_state){
        return std::format("Expected state {}, got {}.", expected_state.to_string(), solved.state.to_string());
    }
    return true;
}

std::string solve_discrete_streams(
    const std::vector<std::vector<BlinkEvent>>& streams, const std::vector<uint8_t>& stream_slots,
    double max_rms_ticks, BlinkSolveResult& solved_out
){
    const TickFit fit = fit_tick_period(streams[0]);
    std::string err = verify_tick_fit(fit, BDSP_NPC_TICK_SECONDS, max_rms_ticks);
    if (!err.empty()){
        return err;
    }
    std::vector<BlinkSample> samples;
    std::string build_error;
    if (!build_samples(streams, stream_slots, fit.period_seconds, samples, build_error)){
        return std::format("build_samples() failed: {}", build_error);
    }
    solved_out = solve_state_from_samples(samples);
    return {};
}

//=============================================================================
//  Section 1: Pure Discrete Blinks -> RNG State (NO signal or waveform IO)
//  Feeds BlinkEvent lists from Xorshift128State straight into fit_tick_period()
//  -> build_samples() -> solve_state_from_samples().
//=============================================================================

bool flip_first_double_to_single(std::vector<BlinkEvent>& events){
    for (BlinkEvent& event : events){
        if (event.type == BlinkType::Double){
            event.type = BlinkType::Single;
            return true;
        }
    }
    return false;
}

struct DiscreteBlinkCase{
    std::string name;
    struct Input{ size_t ticks; std::vector<uint8_t> stream_slots; bool flip_first_double; };
    Input input;
    struct Expectation{ bool expect_solved; };
    Expectation expectation;
};

class Test_DiscreteBlinkSolver : public UnitTest{
public:
    explicit Test_DiscreteBlinkSolver(DiscreteBlinkCase scenario)
        : UnitTest(std::format("PokemonBDSP::BlinkSolver - {}", scenario.name)), m_case(std::move(scenario)){}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& in = m_case.input;
        const uint8_t npcs = (uint8_t)in.stream_slots.size();
        std::vector<std::vector<BlinkEvent>> streams(npcs);
        for (size_t s = 0; s < npcs; s++){
            streams[s] = truth_events_for_slot(in.ticks, npcs, in.stream_slots[s]);
        }
        if (in.flip_first_double && !flip_first_double_to_single(streams[0])){
            return "Test setup failed: stream 0 had no Double blink to flip.";
        }
        BlinkSolveResult solved;
        std::string err = solve_discrete_streams(streams, in.stream_slots, MAX_TICK_FIT_RMS, solved);
        if (!err.empty()){
            return err;
        }
        return verify_solve_result(solved, m_case.expectation.expect_solved, expected_first_blink_state(in.ticks, npcs));
    }

private:
    const DiscreteBlinkCase m_case;
};

void add_discrete_solver_tests(UnitTestDatabase& database){
    const DiscreteBlinkCase DISCRETE_CASES[] = {
        {
            .name = "2-NPC exact state recovery",
            .input = {.ticks = 240, .stream_slots = {0, 1}, .flip_first_double = false},
            .expectation = {.expect_solved = true},
        },{
            .name = "1-NPC exact state recovery",
            .input = {.ticks = 420, .stream_slots = {0}, .flip_first_double = false},
            .expectation = {.expect_solved = true},
        },{
            .name = "2-NPC rotated stream slots {1, 0} exact state recovery",
            .input = {.ticks = 240, .stream_slots = {1, 0}, .flip_first_double = false},
            .expectation = {.expect_solved = true},
        },{
            .name = "single Double->Single misclassification causes GF(2) contradiction",
            .input = {.ticks = 240, .stream_slots = {0, 1}, .flip_first_double = true},
            .expectation = {.expect_solved = false},
        },
    };
    for (const DiscreteBlinkCase& scenario : DISCRETE_CASES){
        database.add<Test_DiscreteBlinkSolver>(scenario);
    }
}

//=============================================================================
//  Section 2: Synthetic Eye-Match Signal -> Discrete Blinks
//  synthesize_match_stream() stands in for EyeBlinkWatcher, converting known
//  BlinkEvents into 30fps {timestamp, match_score} samples to test
//  auto_blink_threshold(), extract_blinks(), and group_blinks().
//=============================================================================

constexpr double FPS = 30.0;
constexpr double RESTING_MATCH = 0.95;
constexpr double BLINK_PEAK_DEPTH = 0.50;
constexpr double DIP_FLAT_HALF_WIDTH = 0.025;
constexpr double DIP_TOTAL_HALF_WIDTH = 0.045;
constexpr double DOUBLE_BLINK_GAP_SECONDS = 0.250;
constexpr double STREAM_TAIL_SECONDS = 1.0;
constexpr double GROUPING_THRESHOLD = 0.15;

struct FrameStall{ double start_seconds = 0.0; double end_seconds = 0.0; };

std::vector<BlinkEvent> short_grouping_schedule(double period_seconds){
    const std::pair<uint64_t, BlinkType> items[] = {
        {0, BlinkType::Single}, {2, BlinkType::Double}, {5, BlinkType::Single},
        {6, BlinkType::Double}, {11, BlinkType::Double}, {14, BlinkType::Single}, {19, BlinkType::Double},
    };
    std::vector<BlinkEvent> events;
    for (const auto& [tick, type] : items){
        events.emplace_back(BlinkEvent{STREAM_LEAD_SECONDS + (double)tick * period_seconds, type});
    }
    return events;
}

std::vector<double> expand_blink_dips(const std::vector<BlinkEvent>& events){
    std::vector<double> dips;
    dips.reserve(events.size() * 2);
    for (const BlinkEvent& event : events){
        dips.emplace_back(event.seconds);
        if (event.type == BlinkType::Double){
            dips.emplace_back(event.seconds + DOUBLE_BLINK_GAP_SECONDS);
        }
    }
    return dips;
}

//  Stands in for EyeBlinkWatcher: converts discrete BlinkEvents into a 30fps
//  {timestamp, match_score} waveform, optionally dropping frames in `stall`.
std::vector<BlinkMatchSample> synthesize_match_stream(
    const std::vector<BlinkEvent>& truth_events, double duration_seconds, FrameStall stall = {}
){
    const std::vector<double> dips = expand_blink_dips(truth_events);
    const WallClock origin{};
    const size_t total_frames = (size_t)std::ceil(duration_seconds * FPS);
    std::vector<BlinkMatchSample> samples;
    samples.reserve(total_frames + 1);
    size_t dip = 0;
    for (size_t frame = 0; frame <= total_frames; frame++){
        const double t = (double)frame / FPS;
        if (stall.end_seconds > stall.start_seconds && t >= stall.start_seconds && t <= stall.end_seconds){
            continue;
        }
        while (dip < dips.size() && dips[dip] + DIP_TOTAL_HALF_WIDTH < t){
            dip++;
        }
        const double dist = dip < dips.size() ? std::abs(t - dips[dip]) : DIP_TOTAL_HALF_WIDTH;
        const double depth = dist <= DIP_FLAT_HALF_WIDTH ? BLINK_PEAK_DEPTH
            : (dist < DIP_TOTAL_HALF_WIDTH ? BLINK_PEAK_DEPTH * (DIP_TOTAL_HALF_WIDTH - dist) / (DIP_TOTAL_HALF_WIDTH - DIP_FLAT_HALF_WIDTH) : 0.0);
        const WallClock ts = origin + std::chrono::duration_cast<WallClock::duration>(std::chrono::duration<double>(t));
        samples.emplace_back(BlinkMatchSample{ts, RESTING_MATCH * (1.0 - depth)});
    }
    return samples;
}

std::string_view blink_type_label(std::optional<BlinkType> type){
    return type.has_value() ? blink_type_name(*type) : "Uncertain";
}

struct ExtractOptions{
    bool use_auto_threshold = true;
    bool apply_stall_guard = true;
};

std::string extract_grouped_blinks(
    const std::vector<BlinkMatchSample>& raw, ExtractOptions options, std::vector<BlinkEvent>& observed
){
    //  Fixed threshold is used for ~20s schedules below auto_blink_threshold()'s 180s minimum.
    const double threshold = options.use_auto_threshold ? auto_blink_threshold(raw) : GROUPING_THRESHOLD;
    if (!(threshold > 0.0)){
        return std::format("auto_blink_threshold() returned {:.4f}.", threshold);
    }
    observed = group_blinks(extract_blinks(raw, threshold, WallClock{}));
    if (options.apply_stall_guard){
        mark_uncertain_blink_types(raw, WallClock{}, BlinkStallGuardConfig{}, observed);
    }
    return {};
}

struct BlinkEventCounts{
    size_t misclassified_doubles = 0;
    size_t uncertain_types = 0;
};

std::string verify_blink_events(const std::vector<BlinkEvent>& truth, const std::vector<BlinkEvent>& observed, BlinkEventCounts expected = {}){
    if (observed.size() != truth.size()){
        return std::format("Expected {} grouped events, got {}.", truth.size(), observed.size());
    }
    BlinkEventCounts got;
    for (size_t i = 0; i < truth.size(); i++){
        if (!observed[i].type.has_value()){
            got.uncertain_types++;
        }else if (truth[i].type == BlinkType::Double && observed[i].type == BlinkType::Single){
            got.misclassified_doubles++;
        }else if (truth[i].type != observed[i].type){
            return std::format("Event {} misclassified: expected {}, got {}.", i, blink_type_label(truth[i].type), blink_type_label(observed[i].type));
        }
    }
    if (got.misclassified_doubles != expected.misclassified_doubles || got.uncertain_types != expected.uncertain_types){
        return std::format("Expected misclassified={}/uncertain={}, got {}/{}.", expected.misclassified_doubles, expected.uncertain_types, got.misclassified_doubles, got.uncertain_types);
    }
    return {};
}

struct BlinkSignalCase{
    std::string name;
    struct Input{ std::vector<BlinkEvent> truth; bool use_auto_threshold; };
    Input input;
    struct Expectation{ double expected_period_seconds; };
    Expectation expectation;
};

class Test_BlinkSignal : public UnitTest{
public:
    explicit Test_BlinkSignal(BlinkSignalCase scenario)
        : UnitTest(std::format("PokemonBDSP::BlinkSignal - {}", scenario.name)), m_case(std::move(scenario)){}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& in = m_case.input;
        const double duration = in.truth.back().seconds + DOUBLE_BLINK_GAP_SECONDS + STREAM_TAIL_SECONDS;
        std::vector<BlinkEvent> observed;
        std::string err = extract_grouped_blinks(
            synthesize_match_stream(in.truth, duration),
            ExtractOptions{.use_auto_threshold = in.use_auto_threshold, .apply_stall_guard = true},
            observed
        );
        if (!err.empty()){
            return err;
        }
        err = verify_blink_events(in.truth, observed);
        if (!err.empty()){
            return err;
        }
        err = verify_tick_fit(fit_tick_period(observed), m_case.expectation.expected_period_seconds);
        return err.empty() ? UnitTestResult(true) : UnitTestResult(std::move(err));
    }

private:
    const BlinkSignalCase m_case;
};

void add_signal_extraction_tests(UnitTestDatabase& database){
    const BlinkSignalCase SIGNAL_CASES[] = {
        {
            .name = "single and double dips at 250ms with nominal 1.017s period",
            .input = {.truth = short_grouping_schedule(BDSP_NPC_TICK_SECONDS), .use_auto_threshold = false},
            .expectation = {.expected_period_seconds = BDSP_NPC_TICK_SECONDS},
        },{
            .name = "single and double dips at 250ms with hardware 1.0197s period",
            .input = {.truth = short_grouping_schedule(HARDWARE_TICK_SECONDS), .use_auto_threshold = false},
            .expectation = {.expected_period_seconds = HARDWARE_TICK_SECONDS},
        },{
            .name = "smooth 30fps stream recovers every BlinkEvent via auto_blink_threshold",
            .input = {.truth = truth_events_for_slot(/*ticks=*/240, /*npcs=*/2, /*slot=*/0), .use_auto_threshold = true},
            .expectation = {.expected_period_seconds = BDSP_NPC_TICK_SECONDS},
        },
    };
    for (const BlinkSignalCase& scenario : SIGNAL_CASES){
        database.add<Test_BlinkSignal>(scenario);
    }
}

//=============================================================================
//  Section 3: End-to-End Bug #2 Frame-Stall Guard (Signal -> Solver)
//  Bridges Section 2 -> Section 1: a 95ms capture stall over either dip of a
//  Double blink misclassifies it as Single without the guard, while the guard
//  clears bit 0 so the solver recovers the exact Xorshift128State.
//=============================================================================

constexpr double STALL_DURATION_SECONDS = 0.095;
//  Dropping Dip 1 shifts one event by +0.250s (~0.246 ticks), raising 30-event RMS to ~0.065.
constexpr double DIP1_SHIFT_MAX_TICK_FIT_RMS = 0.08;

enum class DoubleDipTarget{
    FirstDip,
    SecondDip,
};

FrameStall stall_on_first_double_dip(const std::vector<BlinkEvent>& events, DoubleDipTarget target, double duration){
    for (const BlinkEvent& event : events){
        if (event.type == BlinkType::Double){
            const double offset = target == DoubleDipTarget::SecondDip ? DOUBLE_BLINK_GAP_SECONDS : 0.0;
            const double center = event.seconds + offset;
            return FrameStall{center - duration * 0.5, center + duration * 0.5};
        }
    }
    return {};
}

struct FrameStallCase{
    std::string name;
    struct Input{ size_t ticks; std::vector<uint8_t> stream_slots; DoubleDipTarget dip_target; double stall_duration_seconds; bool apply_stall_guard; };
    Input input;
    struct Expectation{ BlinkEventCounts stream0_counts; double max_rms_ticks; bool expect_solved; };
    Expectation expectation;
};

class Test_FrameStallBug2 : public UnitTest{
public:
    explicit Test_FrameStallBug2(FrameStallCase scenario)
        : UnitTest(std::format("PokemonBDSP::BlinkPipeline - {}", scenario.name)), m_case(std::move(scenario)){}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& in = m_case.input;
        const uint8_t npcs = (uint8_t)in.stream_slots.size();
        const double duration = STREAM_LEAD_SECONDS + (double)in.ticks * BDSP_NPC_TICK_SECONDS + STREAM_TAIL_SECONDS;
        std::vector<std::vector<BlinkEvent>> observed(npcs);
        for (size_t s = 0; s < npcs; s++){
            const std::vector<BlinkEvent> truth = truth_events_for_slot(in.ticks, npcs, in.stream_slots[s]);
            const FrameStall stall = s == 0 ? stall_on_first_double_dip(truth, in.dip_target, in.stall_duration_seconds) : FrameStall{};
            const ExtractOptions options{.use_auto_threshold = true, .apply_stall_guard = in.apply_stall_guard};
            std::string err = extract_grouped_blinks(synthesize_match_stream(truth, duration, stall), options, observed[s]);
            if (!err.empty()){
                return std::format("Stream {}: {}", s, err);
            }
            err = verify_blink_events(truth, observed[s], s == 0 ? m_case.expectation.stream0_counts : BlinkEventCounts{});
            if (!err.empty()){
                return err;
            }
        }
        BlinkSolveResult solved;
        std::string err = solve_discrete_streams(observed, in.stream_slots, m_case.expectation.max_rms_ticks, solved);
        return err.empty()
            ? verify_solve_result(solved, m_case.expectation.expect_solved, expected_first_blink_state(in.ticks, npcs))
            : UnitTestResult(std::move(err));
    }

private:
    const FrameStallCase m_case;
};

void add_bug2_characterization_tests(UnitTestDatabase& database){
    const FrameStallCase BUG2_CASES[] = {
        {.name = "95ms stall on Double dip 2 without guard fails GF(2)",
         .input = {.ticks = 240, .stream_slots = {0, 1}, .dip_target = DoubleDipTarget::SecondDip, .stall_duration_seconds = STALL_DURATION_SECONDS, .apply_stall_guard = false},
         .expectation = {.stream0_counts = {.misclassified_doubles = 1, .uncertain_types = 0}, .max_rms_ticks = MAX_TICK_FIT_RMS, .expect_solved = false}},
        {.name = "95ms stall on Double dip 2 with guard recovers exact state (2-NPC)",
         .input = {.ticks = 240, .stream_slots = {0, 1}, .dip_target = DoubleDipTarget::SecondDip, .stall_duration_seconds = STALL_DURATION_SECONDS, .apply_stall_guard = true},
         .expectation = {.stream0_counts = {.misclassified_doubles = 0, .uncertain_types = 1}, .max_rms_ticks = MAX_TICK_FIT_RMS, .expect_solved = true}},
        {.name = "95ms stall on Double dip 1 without guard fails GF(2)",
         .input = {.ticks = 240, .stream_slots = {0, 1}, .dip_target = DoubleDipTarget::FirstDip, .stall_duration_seconds = STALL_DURATION_SECONDS, .apply_stall_guard = false},
         .expectation = {.stream0_counts = {.misclassified_doubles = 1, .uncertain_types = 0}, .max_rms_ticks = DIP1_SHIFT_MAX_TICK_FIT_RMS, .expect_solved = false}},
        {.name = "95ms stall on Double dip 1 with guard recovers exact state (2-NPC)",
         .input = {.ticks = 240, .stream_slots = {0, 1}, .dip_target = DoubleDipTarget::FirstDip, .stall_duration_seconds = STALL_DURATION_SECONDS, .apply_stall_guard = true},
         .expectation = {.stream0_counts = {.misclassified_doubles = 0, .uncertain_types = 1}, .max_rms_ticks = DIP1_SHIFT_MAX_TICK_FIT_RMS, .expect_solved = true}},
        {.name = "95ms stall on Double dip 2 with guard recovers exact state (1-NPC)",
         .input = {.ticks = 420, .stream_slots = {0}, .dip_target = DoubleDipTarget::SecondDip, .stall_duration_seconds = STALL_DURATION_SECONDS, .apply_stall_guard = true},
         .expectation = {.stream0_counts = {.misclassified_doubles = 0, .uncertain_types = 1}, .max_rms_ticks = MAX_TICK_FIT_RMS, .expect_solved = true}},
    };
    for (const FrameStallCase& scenario : BUG2_CASES){
        database.add<Test_FrameStallBug2>(scenario);
    }
}

} // namespace

void add_tests_BlinkExtraction(UnitTestDatabase& database){
    add_discrete_solver_tests(database);
    add_signal_extraction_tests(database);
    add_bug2_characterization_tests(database);
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
