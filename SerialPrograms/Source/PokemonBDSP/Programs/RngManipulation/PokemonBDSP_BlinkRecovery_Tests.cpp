/*  BDSP Blink Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "PokemonBDSP_BlinkRecovery_Tests.h"
#include "PokemonBDSP_BlinkWindowSolver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

constexpr double STREAM_LEAD_SECONDS = 1.0;

enum class Perturbation{
    None,
    DropEarliestAfterProvisional,
    CorruptDuringConfirmation,
};

struct SimulationStats{
    BlinkWindowStatus terminal = BlinkWindowStatus::Waiting;
    Pokemon::Xorshift128State confirmed_state;
    uint64_t expected_earliest_tick = 0;
    size_t provisional_changed = 0;
    size_t provisional_dropped = 0;
    size_t failures_before_drop = 0;
    bool trimmed_while_confirming = false;
    bool trimmed_after_drop = false;
};

struct BlinkRecoveryCase{
    std::string name;
    struct Input{
        Pokemon::Xorshift128State seed;
        uint8_t npcs;
        size_t max_ticks;
        BlinkWindowSolverConfig config;
        Perturbation perturbation;
    };
    Input input;
    struct Expectation{
        BlinkWindowStatus terminal;
        size_t provisional_changed;
        size_t provisional_dropped;
        size_t failures_before_drop;
        bool trimmed_after_drop;
    };
    Expectation expectation;
};

struct SimulationState{
    std::vector<std::vector<BlinkEvent>> streams;
    std::vector<std::vector<uint64_t>> stream_ticks;
    size_t consecutive_failures = 0;
    bool confirming = false;
    bool perturbation_applied = false;
};

uint64_t earliest_tick_in(const std::vector<std::vector<uint64_t>>& stream_ticks){
    uint64_t earliest = UINT64_MAX;
    for (const std::vector<uint64_t>& ticks : stream_ticks){
        if (!ticks.empty()){
            earliest = std::min(earliest, ticks.front());
        }
    }
    return earliest;
}

void drop_earliest_blink(SimulationState& sim){
    size_t earliest = sim.streams.size();
    for (size_t s = 0; s < sim.streams.size(); s++){
        if (sim.streams[s].empty()){
            continue;
        }
        if (earliest == sim.streams.size() || sim.streams[s].front().seconds < sim.streams[earliest].front().seconds){
            earliest = s;
        }
    }
    if (earliest < sim.streams.size()){
        sim.streams[earliest].erase(sim.streams[earliest].begin());
        sim.stream_ticks[earliest].erase(sim.stream_ticks[earliest].begin());
    }
}

bool trim_streams(size_t keep_per_stream, SimulationState& sim){
    bool trimmed = false;
    for (size_t s = 0; s < sim.streams.size(); s++){
        if (sim.streams[s].size() <= keep_per_stream){
            continue;
        }
        const ptrdiff_t drop = (ptrdiff_t)(sim.streams[s].size() - keep_per_stream);
        sim.streams[s].erase(sim.streams[s].begin(), sim.streams[s].begin() + drop);
        sim.stream_ticks[s].erase(sim.stream_ticks[s].begin(), sim.stream_ticks[s].begin() + drop);
        trimmed = true;
    }
    return trimmed;
}

void append_tick_blinks(
    const std::vector<std::vector<BlinkTick>>& schedules, size_t tick,
    Perturbation perturbation, SimulationState& sim
){
    const double seconds = STREAM_LEAD_SECONDS + (double)tick * BDSP_NPC_TICK_SECONDS;
    for (size_t s = 0; s < schedules.size(); s++){
        if (!schedules[s][tick].blinked){
            continue;
        }
        BlinkType type = schedules[s][tick].type;
        if (perturbation == Perturbation::CorruptDuringConfirmation && sim.confirming && !sim.perturbation_applied){
            type = type == BlinkType::Single ? BlinkType::Double : BlinkType::Single;
            sim.perturbation_applied = true;
        }
        sim.streams[s].emplace_back(BlinkEvent{seconds, type});
        sim.stream_ticks[s].emplace_back(tick);
    }
}

void record_provisional_status(BlinkWindowStatus status, Perturbation perturbation, SimulationState& sim, SimulationStats& stats){
    if (status == BlinkWindowStatus::ProvisionalFound){
        sim.confirming = true;
        if (perturbation == Perturbation::DropEarliestAfterProvisional && !sim.perturbation_applied){
            drop_earliest_blink(sim);
            sim.perturbation_applied = true;
        }
    }else if (status == BlinkWindowStatus::ProvisionalChanged){
        stats.provisional_changed++;
    }else if (status == BlinkWindowStatus::ProvisionalDropped){
        stats.provisional_dropped++;
        stats.failures_before_drop = sim.consecutive_failures + 1;
        sim.confirming = false;
    }
    sim.consecutive_failures = 0;
}

bool apply_step_status(const BlinkWindowStep& step, Perturbation perturbation, SimulationState& sim, SimulationStats& stats){
    if (step.status == BlinkWindowStatus::Waiting){
        return false;
    }
    if (step.status == BlinkWindowStatus::SolveFailed){
        sim.consecutive_failures++;
        return false;
    }
    if (step.status == BlinkWindowStatus::Confirmed || step.status == BlinkWindowStatus::GaveUp){
        stats.terminal = step.status;
        stats.confirmed_state = step.state;
        stats.expected_earliest_tick = earliest_tick_in(sim.stream_ticks);
        return true;
    }
    record_provisional_status(step.status, perturbation, sim, stats);
    return false;
}

std::vector<std::vector<BlinkTick>> make_schedules(const BlinkRecoveryCase::Input& in){
    std::vector<std::vector<BlinkTick>> schedules(in.npcs);
    for (uint8_t slot = 0; slot < in.npcs; slot++){
        schedules[slot] = generate_blink_ticks(Pokemon::Xorshift128(in.seed), in.max_ticks, in.npcs, slot);
    }
    return schedules;
}

SimulationStats run_window_simulation(const BlinkRecoveryCase::Input& in){
    const std::vector<std::vector<BlinkTick>> schedules = make_schedules(in);
    BlinkWindowSolver solver(in.config);
    SimulationState sim{
        .streams = std::vector<std::vector<BlinkEvent>>(in.npcs),
        .stream_ticks = std::vector<std::vector<uint64_t>>(in.npcs),
        .consecutive_failures = 0, .confirming = false, .perturbation_applied = false,
    };
    SimulationStats stats;
    for (size_t tick = 0; tick < in.max_ticks; tick++){
        append_tick_blinks(schedules, tick, in.perturbation, sim);
        if (solver.should_trim_window()){
            stats.trimmed_while_confirming |= sim.confirming;
            if (trim_streams(in.config.window_events / in.npcs, sim) && stats.provisional_dropped > 0){
                stats.trimmed_after_drop = true;
            }
        }
        if (apply_step_status(solver.step(sim.streams, WallClock{}), in.perturbation, sim, stats)){
            break;
        }
    }
    return stats;
}

UnitTestResult verify_confirmed_state(const BlinkRecoveryCase::Input& in, const SimulationStats& stats){
    Pokemon::Xorshift128 expected(in.seed);
    expected.advance(stats.expected_earliest_tick * in.npcs);
    if (stats.confirmed_state != expected.state()){
        return std::format("Expected state {}, got {}.", expected.state().to_string(), stats.confirmed_state.to_string());
    }
    return true;
}

UnitTestResult verify_simulation(const BlinkRecoveryCase::Input& in, const BlinkRecoveryCase::Expectation& exp, const SimulationStats& stats){
    if (stats.trimmed_while_confirming){
        return "Window trimming stayed enabled while confirming a provisional candidate.";
    }
    if (stats.terminal != exp.terminal){
        return std::format("Expected terminal status {}, got {}.", (int)exp.terminal, (int)stats.terminal);
    }
    if (stats.provisional_changed != exp.provisional_changed || stats.provisional_dropped != exp.provisional_dropped){
        return std::format(
            "Expected changed={}/dropped={}, got {}/{}.",
            exp.provisional_changed, exp.provisional_dropped, stats.provisional_changed, stats.provisional_dropped
        );
    }
    if (stats.failures_before_drop != exp.failures_before_drop || stats.trimmed_after_drop != exp.trimmed_after_drop){
        return std::format(
            "Expected failures_before_drop={}/trimmed_after_drop={}, got {}/{}.",
            exp.failures_before_drop, exp.trimmed_after_drop, stats.failures_before_drop, stats.trimmed_after_drop
        );
    }
    return exp.terminal == BlinkWindowStatus::Confirmed ? verify_confirmed_state(in, stats) : UnitTestResult(true);
}

class Test_BlinkWindowSolver : public UnitTest{
public:
    explicit Test_BlinkWindowSolver(BlinkRecoveryCase scenario)
        : UnitTest(std::format("PokemonBDSP::BlinkRecovery - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return verify_simulation(m_scenario.input, m_scenario.expectation, run_window_simulation(m_scenario.input));
    }

private:
    const BlinkRecoveryCase m_scenario;
};

BlinkWindowSolverConfig make_solver_config(size_t max_confirmation_failures){
    return BlinkWindowSolverConfig{
        .min_rolls_to_try = 44,
        .confirmation_events = 6,
        .window_events = 52,
        .max_failed_windows = 3,
        .max_confirmation_failures = max_confirmation_failures,
    };
}

const Pokemon::Xorshift128State TEST_SEED(0x12345678, 0x9abcdef0, 0x13579bdf, 0x2468ace0);

void add_confirmation_tests(UnitTestDatabase& database){
    const BlinkRecoveryCase CASES[] = {
        {
            .name = "clean 2-NPC stream solves provisional state and confirms",
            .input = {.seed = TEST_SEED, .npcs = 2, .max_ticks = 400, .config = make_solver_config(3), .perturbation = Perturbation::None},
            .expectation = {.terminal = BlinkWindowStatus::Confirmed, .provisional_changed = 0, .provisional_dropped = 0, .failures_before_drop = 0, .trimmed_after_drop = false},
        },
        {
            .name = "earliest blink shift during confirmation replaces candidate and confirms",
            .input = {.seed = TEST_SEED, .npcs = 2, .max_ticks = 400, .config = make_solver_config(3), .perturbation = Perturbation::DropEarliestAfterProvisional},
            .expectation = {.terminal = BlinkWindowStatus::Confirmed, .provisional_changed = 1, .provisional_dropped = 0, .failures_before_drop = 0, .trimmed_after_drop = false},
        },
    };
    for (const BlinkRecoveryCase& scenario : CASES){
        database.add<Test_BlinkWindowSolver>(scenario);
    }
}

void add_stuck_confirmation_tests(UnitTestDatabase& database){
    const BlinkRecoveryCase CASES[] = {
        {
            .name = "corrupted confirmation blink stays stuck until give-up when drop is disabled",
            .input = {.seed = TEST_SEED, .npcs = 2, .max_ticks = 1200, .config = make_solver_config(0), .perturbation = Perturbation::CorruptDuringConfirmation},
            .expectation = {.terminal = BlinkWindowStatus::GaveUp, .provisional_changed = 0, .provisional_dropped = 0, .failures_before_drop = 0, .trimmed_after_drop = false},
        },
        {
            .name = "corrupted confirmation blink drops candidate on 3rd failure and recovers",
            .input = {.seed = TEST_SEED, .npcs = 2, .max_ticks = 1200, .config = make_solver_config(3), .perturbation = Perturbation::CorruptDuringConfirmation},
            .expectation = {.terminal = BlinkWindowStatus::Confirmed, .provisional_changed = 0, .provisional_dropped = 1, .failures_before_drop = 3, .trimmed_after_drop = true},
        },
    };
    for (const BlinkRecoveryCase& scenario : CASES){
        database.add<Test_BlinkWindowSolver>(scenario);
    }
}

} // namespace

void add_tests_BlinkRecovery(UnitTestDatabase& database){
    add_confirmation_tests(database);
    add_stuck_confirmation_tests(database);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
