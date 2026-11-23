/*  BDSP Starter RNG Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonBDSP_BlinkRecovery.h"
#include "PokemonBDSP_BlinkSampleLog.h"
#include "PokemonBDSP_RngAim.h"
#include "PokemonBDSP_RngCalibration.h"
#include "PokemonBDSP_StarterNavigation.h"
#include "PokemonBDSP_StarterRngAttempts.h"
#include "PokemonBDSP_StarterRng_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


namespace{

struct StarterTableRow{
    BdspStarter starter;
    std::string slug;
    uint8_t cursor_steps;
    BaseStats base_stats;
    double move_seconds;
};

const std::vector<StarterTableRow> STARTER_TABLE_ROWS = {
    {
        .starter = BdspStarter::Turtwig,
        .slug = "turtwig",
        .cursor_steps = 0,
        .base_stats = {55, 68, 64, 45, 55, 31},
        .move_seconds = 6.8,
    },
    {
        .starter = BdspStarter::Chimchar,
        .slug = "chimchar",
        .cursor_steps = 1,
        .base_stats = {44, 58, 44, 58, 44, 61},
        .move_seconds = 7.5,
    },
    {
        .starter = BdspStarter::Piplup,
        .slug = "piplup",
        .cursor_steps = 2,
        .base_stats = {53, 51, 53, 61, 56, 40},
        .move_seconds = 8.2,
    },
};


UnitTestResult test_starter_tables(Logger&){
    for (const StarterTableRow& row : STARTER_TABLE_ROWS){
        const std::string got_slug(starter_slug(row.starter));
        if (got_slug != row.slug){
            return std::format("starter_slug mismatch: got {}, expected {}", got_slug, row.slug);
        }
        const uint8_t got_steps = starter_cursor_steps(row.starter);
        if (got_steps != row.cursor_steps){
            return std::format(
                "starter_cursor_steps({}) = {}, expected {}",
                row.slug, got_steps, row.cursor_steps
            );
        }
        const BaseStats& stats = starter_base_stats(row.starter);
        if (stats.hp != row.base_stats.hp
            || stats.attack != row.base_stats.attack
            || stats.defense != row.base_stats.defense
            || stats.spatk != row.base_stats.spatk
            || stats.spdef != row.base_stats.spdef
            || stats.speed != row.base_stats.speed
        ){
            return std::format(
                "starter_base_stats({}) = {}/{}/{}/{}/{}/{}, expected {}/{}/{}/{}/{}/{}",
                row.slug,
                stats.hp, stats.attack, stats.defense, stats.spatk, stats.spdef, stats.speed,
                row.base_stats.hp, row.base_stats.attack, row.base_stats.defense,
                row.base_stats.spatk, row.base_stats.spdef, row.base_stats.speed
            );
        }
        const double move_s = seconds_to_move_to_starter(row.starter);
        if (std::abs(move_s - row.move_seconds) > 1e-9){
            return std::format(
                "seconds_to_move_to_starter({}) = {}, expected {}",
                row.slug, move_s, row.move_seconds
            );
        }
    }
    return true;
}


UnitTestResult test_skip_buffer_and_landing_math(Logger&){
    if (bdsp_skip_buffer(0.0) != 750){
        return std::format("bdsp_skip_buffer(0) = {}, expected 750", bdsp_skip_buffer(0.0));
    }
    if (bdsp_skip_buffer(100000.0) != 770){
        return std::format("bdsp_skip_buffer(100000) = {}, expected 770", bdsp_skip_buffer(100000.0));
    }

    const double sigma0 = bdsp_skip_landing_sigma(0.0);
    if (std::abs(sigma0 - 67.0) > 1e-9){
        return std::format("bdsp_skip_landing_sigma(0) = {}, expected 67.0", sigma0);
    }

    const uint64_t lake0 = bdsp_lake_advances_needed(0.0);
    if (lake0 != 750 + 2 * 67){
        return std::format("bdsp_lake_advances_needed(0) = {}, expected {}", lake0, 750 + 2 * 67);
    }

    //  At 1,000,000 advances skipped: buffer = 750 + 200 = 950,
    //  sigma = sqrt(100^2 + 67^2) = sqrt(14489) ~ 120.37, 2*sigma = 240, total = 1190.
    const uint64_t lake1m = bdsp_lake_advances_needed(1000000.0);
    if (lake1m != 1190){
        return std::format("bdsp_lake_advances_needed(1e6) = {}, expected 1190", lake1m);
    }
    return true;
}


UnitTestResult test_aim_and_calibration(Logger& logger){
    if (aimed_advance(1000, 200, 0) != 1200){
        return "aimed_advance(1000, 200, 0) != 1200";
    }
    if (aimed_advance(1000, 200, 3) != 1203){
        return "aimed_advance(1000, 200, 3) != 1203";
    }
    if (press_for_advance(1203, 200, 3) != 1000){
        return "press_for_advance(1203, 200, 3) != 1000";
    }

    RngAim aim(3, 1.0);
    if (aim.bias() != 0){
        return "initial bias != 0";
    }
    if (aim.record_offset(logger, 2, true) || aim.record_offset(logger, 2, true)){
        return "should not adjust before 3 samples";
    }
    if (!aim.record_offset(logger, 2, true)){
        return "expected adjustment on 3rd sample of +2";
    }
    if (aim.bias() != 2){
        return std::format("bias after three +2 offsets = {}, expected 2", aim.bias());
    }
    return true;
}


//  A shiny whose post-battle scene did not finish still ends the loop (the
//  shiny must not be reset away), and says so.
UnitTestResult test_shiny_unsettled(Logger&){
    BdspStarterAttemptResult shiny{
        .outcome = BdspAttemptOutcome::Hit, .shiny = true, .target_advance = 9, .settled = false
    };
    StarterRngRunResult result;
    if (!resolve_starter_rng_step(shiny, 1, 3, result) || result.outcome != StarterRngOutcome::SHINY){
        return "expected a shiny to end the loop";
    }
    if (result.settled){
        return "expected the unsettled shiny to say that the game is not settled";
    }
    shiny.settled = true;
    StarterRngRunResult settled;
    if (!resolve_starter_rng_step(shiny, 1, 3, settled) || !settled.settled){
        return "expected a settled shiny to be settled";
    }
    return true;
}

struct TransitionRow{
    std::string label;
    BdspStarterAttemptResult attempt;
    uint64_t resets_used;
    uint16_t max_resets;
    bool stop;
    StarterRngOutcome outcome;
    uint64_t expected_advance;
    std::string expected_reason;
};

std::vector<TransitionRow> make_transition_rows(){
    BdspPokemonResult target{};
    target.ec = 0x12345678;
    target.pid = 0x87654321;
    target.nature = 3;
    return {
        {"miss below max_resets continues", {.outcome = BdspAttemptOutcome::Missed, .shiny = false, .target = target, .target_advance = 1500}, 1, 3, false, StarterRngOutcome::ABANDONED, 1500, ""},
        {"non-shiny hit stops with HIT", {.outcome = BdspAttemptOutcome::Hit, .shiny = false, .target = target, .target_advance = 1505}, 2, 3, true, StarterRngOutcome::HIT, 1505, ""},
        {"shiny hit stops with SHINY", {.outcome = BdspAttemptOutcome::Hit, .shiny = true, .target = target, .target_advance = 2048}, 1, 0, true, StarterRngOutcome::SHINY, 2048, ""},
        {"miss at max_resets stops with OUT_OF_RESETS", {.outcome = BdspAttemptOutcome::Missed, .shiny = false, .target = target, .target_advance = 1600}, 3, 3, true, StarterRngOutcome::OUT_OF_RESETS, 1600, ""},
        {"abandoned at max_resets stops with ABANDONED", {.outcome = BdspAttemptOutcome::Abandoned, .shiny = false, .failure_reason = "no matching target within the allowed wait time"}, 1, 1, true, StarterRngOutcome::ABANDONED, 0, "no matching target within the allowed wait time"},
    };
}

UnitTestResult test_attempt_loop_transitions(Logger&){
    for (const TransitionRow& row : make_transition_rows()){
        StarterRngRunResult result;
        if (resolve_starter_rng_step(row.attempt, row.resets_used, row.max_resets, result) != row.stop){
            return std::format("{}: unexpected stop flag", row.label);
        }
        if (result.resets_used != row.resets_used || result.target_advance != row.expected_advance){
            return std::format("{}: unexpected resets/advance", row.label);
        }
        if (row.stop && result.outcome != row.outcome){
            return std::format("{}: unexpected outcome", row.label);
        }
        if (!row.expected_reason.empty() && result.last_failure_reason != row.expected_reason){
            return std::format("{}: failure_reason not preserved", row.label);
        }
    }
    return true;
}


//  The default filter row: a table that has it stops for a shiny of any
//  nature and gender, and no other starter; an empty table never stops, which
//  is why a caller must seed it.
UnitTestResult test_default_shiny_row(Logger&){
    BdspRngFilterTable empty("Filters", Pokemon::StatsHuntMiscFeatureFlags{false, true, true, true});
    BdspPokemonResult shiny{};
    shiny.shiny = BdspShiny::Star;
    shiny.nature = 7;
    BdspPokemonResult plain{};
    plain.nature = 7;
    if (starter_wanted(empty.make_snapshot(), shiny)){
        return "an empty filter table must not match a shiny";
    }

    BdspRngFilterTable table("Filters", Pokemon::StatsHuntMiscFeatureFlags{false, true, true, true});
    set_default_shiny_row(table);
    if (table.current_rows() != 1){
        return std::format("expected one default row, got {}", table.current_rows());
    }
    const BdspRngFilterSnapshot snapshot = table.make_snapshot();
    if (!starter_wanted(snapshot, shiny)){
        return "the default row must match a shiny of any nature";
    }
    shiny.nature = 20;
    if (!starter_wanted(snapshot, shiny)){
        return "the default row must match a shiny of another nature";
    }
    if (starter_wanted(snapshot, plain)){
        return "the default row must not match a non-shiny";
    }
    return true;
}


size_t count_lines_starting(const std::string& text, std::string_view prefix){
    size_t count = 0;
    size_t start = 0;
    while (start < text.size()){
        size_t end = text.find('\n', start);
        end = end == std::string::npos ? text.size() : end;
        if (std::string_view(text).substr(start, end - start).starts_with(prefix)){
            count++;
        }
        start = end + 1;
    }
    return count;
}

//  The recovery hands the log its whole sample window every poll, and the
//  window overlaps the last one; replay needs every sample exactly once.
UnitTestResult test_blink_log_writes_each_sample_once(Logger&){
    Filesystem::create_directories(Filesystem::Path(DEBUG_PATH()));
    const Filesystem::Path path = Filesystem::unique_temp_path(
        Filesystem::Path(std::format("{}blink-sample-log-test.txt", DEBUG_PATH()))
    );
    const WallClock t0 = current_time();
    auto at = [t0](int ms){
        return t0 + std::chrono::milliseconds(ms);
    };
    {
        BlinkSampleLog log(path.string());
        if (!log.is_open()){
            return std::format("could not open {}", path.string());
        }
        log.add_samples({{{at(0), 0.9}, {at(16), 0.9}, {at(33), 0.2}}, {{at(5), 0.8}}});
        log.window(0, at(16));
        log.add_samples({{{at(16), 0.9}, {at(33), 0.2}, {at(50), 0.9}}, {{at(5), 0.8}, {at(21), 0.8}}});
        log.window(0, at(16));
    }
    std::string text;
    const bool read = file_to_string(path.string(), text);
    Filesystem::remove(path);
    if (!read){
        return std::format("could not read {}", path.string());
    }
    const size_t samples = count_lines_starting(text, "sample\t");
    if (samples != 6){
        return std::format("expected 6 sample lines, got {}", samples);
    }
    const size_t windows = count_lines_starting(text, "window\t");
    if (windows != 1){
        return std::format("expected the unmoved window once, got {}", windows);
    }
    return true;
}


struct StarterRngTestCase{
    struct Input{
        std::function<UnitTestResult(Logger&)> body;
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_StarterRng : public UnitTest{
public:
    explicit Test_StarterRng(StarterRngTestCase test_case)
        : UnitTest("PokemonBDSP::StarterRng - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StarterRngTestCase m_case;
};

//  A held provisional state is dropped once six solves in a row fail, so the
//  blink window slides past a misread blink (lab-B 2026-11-23, German run 8:
//  three attempts held a state from 51-54 rolls and gave up at 156 blinks).
UnitTestResult test_provisional_state_steps(Logger&){
    const Pokemon::Xorshift128State a(1, 2, 3, 4);
    const Pokemon::Xorshift128State b(5, 6, 7, 8);
    struct Solve{ std::string_view label; bool solved; Pokemon::Xorshift128State state; ProvisionalStep step; };
    const Solve solves[] = {
        {"no state yet, a failed solve", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"first solve", true, a, ProvisionalStep::PROVISIONAL},
        {"failed solve 1", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"failed solve 2", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"failed solve 3", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"failed solve 4", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"failed solve 5", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"failed solve 6", false, a, ProvisionalStep::DROPPED},
        {"solve after the drop", true, b, ProvisionalStep::PROVISIONAL},
        {"a different state", true, a, ProvisionalStep::CHANGED},
        {"failed solve between two that agree", false, a, ProvisionalStep::KEEP_COLLECTING},
        {"the same state again", true, a, ProvisionalStep::CONFIRMED},
    };
    ProvisionalState provisional;
    size_t events = 44;
    for (const Solve& solve : solves){
        const ProvisionalStep step = update_provisional(provisional, solve.solved, solve.state, events++, /*confirmation_events=*/6);
        if (step != solve.step){
            return std::format("{}: step {}, expected {}", solve.label, (int)step, (int)solve.step);
        }
    }
    if (!provisional.held || provisional.state != a){
        return std::string("the confirmed state is not held");
    }
    return true;
}

const double NPC_TICK_SECONDS = 1.0197;

//  The blinks of `npcs` NPCs rolling from `start` once per tick, NPC c a
//  tenth of a second after NPC c - 1, and one blink misread on the first NPC's
//  first tick that did not blink.
std::vector<std::vector<BlinkEvent>> npc_blinks_with_one_misread(
    const Pokemon::Xorshift128State& start, size_t npcs, size_t ticks
){
    Pokemon::Xorshift128 rng(start);
    std::vector<std::vector<BlinkEvent>> streams(npcs);
    for (size_t tick = 0; tick < ticks; tick++){
        for (size_t c = 0; c < npcs; c++){
            const uint32_t roll = rng.next();
            if (npc_blinks(roll)){
                streams[c].emplace_back(BlinkEvent{(double)tick * NPC_TICK_SECONDS + 0.1 * (double)c, NPC_blink_type(roll)});
            }
        }
    }
    std::vector<BlinkEvent>& first_npc = streams[0];
    for (size_t tick = 0; tick < first_npc.size(); tick++){
        const double seconds = (double)tick * NPC_TICK_SECONDS;
        if (first_npc[tick].seconds > seconds + 0.5){
            first_npc.insert(first_npc.begin() + (std::ptrdiff_t)tick, BlinkEvent{seconds, BlinkType::Single});
            break;
        }
    }
    return streams;
}

//  The blinks the watchers hold: seen by `now`, not yet discarded.
std::vector<std::vector<BlinkEvent>> held_blinks(
    const std::vector<std::vector<BlinkEvent>>& blinks, double watched_from, double now
){
    std::vector<std::vector<BlinkEvent>> streams(blinks.size());
    for (size_t c = 0; c < blinks.size(); c++){
        for (const BlinkEvent& event : blinks[c]){
            if (event.seconds >= watched_from && event.seconds <= now){
                streams[c].emplace_back(event);
            }
        }
    }
    return streams;
}

//  A state first solved once the blink window has slid past a misread blink
//  is confirmed against the same blinks (GamePilot's three-NPC golden run;
//  lab-B 2026-11-23 run 8 held a state from 52 rolls). That pass used to slide
//  the window again before the next solve, and by a different cutoff per NPC,
//  so every later solve failed.
UnitTestResult test_state_found_as_window_fills(Logger&){
    const size_t ticks = 400;
    BlinkRecoveryConfig config;
    config.npcs = 3;
    const std::vector<std::vector<BlinkEvent>> blinks = npc_blinks_with_one_misread(
        Pokemon::Xorshift128State(0x12345678, 0x9abcdef0, 0x0fedcba9, 0x87654321), config.npcs, ticks
    );
    ProvisionalState provisional;
    double watched_from = 0;
    std::optional<Pokemon::Xorshift128State> found;
    for (size_t tick = 0; tick < ticks; tick++){
        const std::vector<std::vector<BlinkEvent>> streams = held_blinks(blinks, watched_from, (double)tick * NPC_TICK_SECONDS + 0.5);
        if (std::any_of(streams.begin(), streams.end(), [](const std::vector<BlinkEvent>& stream){ return stream.empty(); })){
            continue;
        }
        const BlinkWindowPass pass = solve_blink_window(streams, config, provisional);
        watched_from = std::max(watched_from, pass.discard_before.value_or(watched_from));
        switch (pass.step){
        case ProvisionalStep::PROVISIONAL:
            found = pass.solved.state;
            break;
        case ProvisionalStep::CHANGED:
        case ProvisionalStep::DROPPED:
            return std::format("tick {}: the provisional state was {}", tick, pass.step == ProvisionalStep::CHANGED ? "changed" : "dropped");
        case ProvisionalStep::CONFIRMED:
            return found.has_value() && pass.solved.state == *found
                ? UnitTestResult(true)
                : UnitTestResult(std::string("confirmed a state other than the provisional one"));
        default:
            break;
        }
    }
    return std::format("no state confirmed in {} ticks (provisional held: {})", ticks, found.has_value());
}

const StarterRngTestCase STARTER_RNG_CASES[] = {
    {.name = "per-starter tables and timings", .input = {.body = test_starter_tables}, .expectation = {}},
    {.name = "bedroom skip buffer and landing math", .input = {.body = test_skip_buffer_and_landing_math}, .expectation = {}},
    {.name = "aim offset and calibration bias", .input = {.body = test_aim_and_calibration}, .expectation = {}},
    {.name = "attempt loop outcome resolution", .input = {.body = test_attempt_loop_transitions}, .expectation = {}},
    {.name = "default filter row is shiny of any nature", .input = {.body = test_default_shiny_row}, .expectation = {}},
    {.name = "a shiny that did not settle ends the loop unsettled", .input = {.body = test_shiny_unsettled}, .expectation = {}},
    {.name = "a provisional state that stops solving is dropped", .input = {.body = test_provisional_state_steps}, .expectation = {}},
    {.name = "a state found as the blink window fills is confirmed", .input = {.body = test_state_found_as_window_fills}, .expectation = {}},
    {.name = "blink log writes each sample once across overlapping polls", .input = {.body = test_blink_log_writes_each_sample_once}, .expectation = {}},
};

} // namespace


void add_tests_StarterRng(UnitTestDatabase& database){
    for (const StarterRngTestCase& test_case : STARTER_RNG_CASES){
        database.add<Test_StarterRng>(test_case);
    }
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
