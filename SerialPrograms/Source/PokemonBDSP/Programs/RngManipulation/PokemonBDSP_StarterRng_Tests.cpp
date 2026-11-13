/*  BDSP Starter RNG Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
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
        const std::string got_slug = starter_slug(row.starter);
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


UnitTestResult test_attempt_loop_transitions(Logger&){
    BdspPokemonResult sample_target{};
    sample_target.ec = 0x12345678;
    sample_target.pid = 0x87654321;
    sample_target.nature = 3;

    //  1. Miss when below max_resets continues the loop.
    {
        StarterRngRunResult result;
        BdspStarterAttemptResult miss{
            .outcome = BdspAttemptOutcome::Missed,
            .shiny = false,
            .target = sample_target,
            .target_advance = 1500,
        };
        if (resolve_starter_rng_step(miss, 1, 3, result)){
            return "expected loop to continue on miss 1 of 3";
        }
        if (result.resets_used != 1 || result.target_advance != 1500){
            return "expected resets_used = 1 and target_advance = 1500 after miss";
        }
    }

    //  2. Non-shiny hit stops with Outcome::HIT.
    {
        StarterRngRunResult result;
        BdspStarterAttemptResult hit{
            .outcome = BdspAttemptOutcome::Hit,
            .shiny = false,
            .target = sample_target,
            .target_advance = 1505,
        };
        if (!resolve_starter_rng_step(hit, 2, 3, result)){
            return "expected loop to terminate on non-shiny hit";
        }
        if (result.outcome != StarterRngOutcome::HIT
            || result.resets_used != 2
            || result.target_advance != 1505
            || result.target.pid != sample_target.pid
        ){
            return "unexpected result fields on non-shiny hit";
        }
    }

    //  3. Shiny hit stops with Outcome::SHINY.
    {
        StarterRngRunResult result;
        BdspStarterAttemptResult shiny_hit{
            .outcome = BdspAttemptOutcome::Hit,
            .shiny = true,
            .target = sample_target,
            .target_advance = 2048,
        };
        if (!resolve_starter_rng_step(shiny_hit, 1, 0, result)){
            return "expected loop to terminate on shiny hit";
        }
        if (result.outcome != StarterRngOutcome::SHINY
            || result.resets_used != 1
            || result.target_advance != 2048
        ){
            return "unexpected result fields on shiny hit";
        }
    }

    //  4. Miss reaching max_resets stops with Outcome::OUT_OF_RESETS.
    {
        StarterRngRunResult result;
        BdspStarterAttemptResult miss{
            .outcome = BdspAttemptOutcome::Missed,
            .shiny = false,
            .target = sample_target,
            .target_advance = 1600,
        };
        if (!resolve_starter_rng_step(miss, 3, 3, result)){
            return "expected loop to terminate when resets_used == max_resets";
        }
        if (result.outcome != StarterRngOutcome::OUT_OF_RESETS || result.resets_used != 3){
            return "expected OUT_OF_RESETS on miss at reset limit";
        }
    }

    //  5. Abandoned attempt reaching max_resets stops with Outcome::ABANDONED and preserves reason.
    {
        StarterRngRunResult result;
        BdspStarterAttemptResult abandoned{
            .outcome = BdspAttemptOutcome::Abandoned,
            .shiny = false,
            .failure_reason = "no matching target within the allowed wait time",
        };
        if (!resolve_starter_rng_step(abandoned, 1, 1, result)){
            return "expected loop to terminate on abandoned single attempt";
        }
        if (result.outcome != StarterRngOutcome::ABANDONED
            || result.resets_used != 1
            || result.last_failure_reason != "no matching target within the allowed wait time"
        ){
            return "expected ABANDONED with failure_reason preserved";
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


struct StarterRngTestCase{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

class Test_StarterRng : public UnitTest{
public:
    explicit Test_StarterRng(StarterRngTestCase test_case)
        : UnitTest("PokemonBDSP::StarterRng - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StarterRngTestCase m_case;
};

const StarterRngTestCase STARTER_RNG_CASES[] = {
    {"per-starter tables and timings", test_starter_tables},
    {"bedroom skip buffer and landing math", test_skip_buffer_and_landing_math},
    {"aim offset and calibration bias", test_aim_and_calibration},
    {"attempt loop outcome resolution", test_attempt_loop_transitions},
    {"default filter row is shiny of any nature", test_default_shiny_row},
};

} // namespace


void add_tests_StarterRng(UnitTestDatabase& database){
    for (const StarterRngTestCase& test_case : STARTER_RNG_CASES){
        database.add<Test_StarterRng>(test_case);
    }
}


}
}
}
