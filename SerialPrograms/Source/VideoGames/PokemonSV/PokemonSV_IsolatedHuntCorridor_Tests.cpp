#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntCorridor.h"
#include "PokemonSV_IsolatedHuntCorridor_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
struct CorridorCase{
    std::string name;
    IsolatedHuntLimits limits{.max_rounds = 2, .ascent = {.enabled = true, .segments_per_ascent = 1}};
    std::vector<IsolatedMarkerState> markers;
    std::vector<IsolatedHuntRound> rounds;
    bool aligned = true;
    bool landed = true;
    bool acquired = false;
    bool pending_refusal = false;
    bool pending_battle = false;
    IsolatedHuntBattleResult battle = IsolatedHuntBattleResult::STOP;
    uint64_t initial_time = 0;
    uint64_t segment_time = 0;
    uint64_t align_time = 0;
    uint64_t flight_time = 0;
    IsolatedHuntStop expected;
    std::string steps;
};
class CorridorActions : public IsolatedHuntActions{
public:
    CorridorCase test;
    size_t marker_next = 0;
    size_t round_next = 0;
    uint64_t clock = 0;
    std::string steps;
    virtual bool reset_start(const IsolatedHuntRequest&) override{ return true; }
    virtual bool prepare_sandwich(const IsolatedHuntRequest&) override{ return true; }
    virtual bool verify_meal_power(const IsolatedHuntRequest&) override{ return true; }
    virtual bool travel_to_anchor(const IsolatedHuntRequest&) override{ steps += "flight "; clock += test.flight_time; return test.landed; }
    virtual IsolatedHuntRound clear_round(const IsolatedHuntRequest&) override{ return {}; }
    virtual bool refresh_spawns(const IsolatedHuntRequest&) override{ steps += "refresh "; return false; }
    virtual bool acquire_refused_target(const IsolatedHuntRequest&) override{ steps += "acquire "; return test.acquired; }
    virtual IsolatedHuntBattleResult handle_battle(const IsolatedHuntRequest&) override{
        steps += "battle "; return test.battle;
    }
    virtual bool refusal_pending() const override{ return test.pending_refusal; }
    virtual bool battle_pending() const override{ return test.pending_battle; }
    virtual uint64_t now_ms() const override{ return clock; }
    virtual IsolatedMarkerState marker_state() override{
        steps += "marker ";
        return marker_next < test.markers.size() ? test.markers[marker_next++] : IsolatedMarkerState::PRESENT;
    }
    virtual bool realign_marker() override{ steps += "align "; clock += test.align_time; return test.aligned; }
    virtual IsolatedHuntRound advance_ascent(const IsolatedHuntRequest&, const IsolatedAscentLimits& limits) override{
        steps += "advance:" + std::to_string(limits.segment_ms) + ":" + std::to_string(limits.clearing_wait_ms) + " ";
        clock += test.segment_time;
        return round_next < test.rounds.size() ? test.rounds[round_next++] : IsolatedHuntRound{.kills = 1};
    }
};
std::vector<CorridorCase> navigation_cases(){
    return {
        {.name = "Missing marker forbids movement and flight", .markers = {IsolatedMarkerState::MISSING},
            .expected = IsolatedHuntStop::MARKER_MISSING, .steps = "marker "},
        {.name = "Arrival flies and verifies a fresh marker before advancing", .markers = {IsolatedMarkerState::ARRIVED},
            .expected = IsolatedHuntStop::ROUND_LIMIT,
            .steps = "marker flight marker align advance:600:5000 flight marker align advance:600:5000 "},
        {.name = "Marker loss between segments stops without resetting",
            .limits = {.max_rounds = 2, .ascent = {.enabled = true, .segments_per_ascent = 6}},
            .markers = {IsolatedMarkerState::PRESENT, IsolatedMarkerState::MISSING},
            .expected = IsolatedHuntStop::MARKER_MISSING, .steps = "marker align advance:600:5000 marker "},
        {.name = "Failed heading correction stops", .aligned = false,
            .expected = IsolatedHuntStop::REANCHOR_FAILED, .steps = "marker align "},
        {.name = "Wrong, absent center or failed landing cannot reanchor", .landed = false,
            .expected = IsolatedHuntStop::TRAVEL_FAILED, .steps = "marker align advance:600:5000 flight "},
        {.name = "Missing marker after flight stops before heading correction",
            .markers = {IsolatedMarkerState::PRESENT, IsolatedMarkerState::MISSING},
            .expected = IsolatedHuntStop::REANCHOR_FAILED, .steps = "marker align advance:600:5000 flight marker "},
    };
}
std::vector<CorridorCase> event_cases(){
    return {
        {.name = "Pending refusal takes priority over exhausted hunt budget",
            .limits = {.max_rounds = 2, .duration_ms = 500, .ascent = {.enabled = true}},
            .pending_refusal = true, .initial_time = 1000,
            .expected = IsolatedHuntStop::UNRESOLVED_REFUSAL, .steps = "acquire "},
        {.name = "Unresolved refusal at cap forbids reset", .rounds = {{.event = IsolatedHuntEvent::REFUSAL}},
            .expected = IsolatedHuntStop::UNRESOLVED_REFUSAL, .steps = "marker align advance:600:5000 acquire "},
        {.name = "Acquired refusal uses shared shiny stop before reset", .rounds = {{.event = IsolatedHuntEvent::REFUSAL}},
            .acquired = true, .expected = IsolatedHuntStop::BATTLE_POLICY,
            .steps = "marker align advance:600:5000 acquire battle "},
        {.name = "Battle policy stops at cap", .rounds = {{.event = IsolatedHuntEvent::BATTLE}},
            .expected = IsolatedHuntStop::BATTLE_POLICY, .steps = "marker align advance:600:5000 battle "},
        {.name = "Battle failure never resets", .rounds = {{.event = IsolatedHuntEvent::BATTLE}},
            .battle = IsolatedHuntBattleResult::FAILED, .expected = IsolatedHuntStop::BATTLE_FAILED,
            .steps = "marker align advance:600:5000 battle "},
        {.name = "Handled battle restores anchor before next segment", .rounds = {{.event = IsolatedHuntEvent::BATTLE}},
            .battle = IsolatedHuntBattleResult::CONTINUE, .expected = IsolatedHuntStop::ROUND_LIMIT,
            .steps = "marker align advance:600:5000 battle flight marker align advance:600:5000 "},
        {.name = "Refusal during navigation forbids departure", .pending_refusal = true,
            .expected = IsolatedHuntStop::UNRESOLVED_REFUSAL, .steps = "acquire "},
        {.name = "Handled navigation battle leaves a failure for inspection", .pending_battle = true,
            .battle = IsolatedHuntBattleResult::CONTINUE, .expected = IsolatedHuntStop::TRAVEL_FAILED,
            .steps = "battle "},
    };
}
std::vector<CorridorCase> budget_cases(){
    return {
        {.name = "Lead still clearing at observation cap forbids flight", .rounds = {{.kills = 1, .clearing_busy = true}},
            .expected = IsolatedHuntStop::CLEARING_TIMEOUT, .steps = "marker align advance:600:5000 "},
        {.name = "Insufficient movement budget stops before reset", .rounds = {{.budget_exhausted = true}},
            .expected = IsolatedHuntStop::TIME_LIMIT, .steps = "marker align advance:600:5000 "},
        {.name = "No progress stops before cap reset", .limits = {.max_rounds = 2, .max_empty_rounds = 1,
            .ascent = {.enabled = true, .segments_per_ascent = 1}}, .rounds = {{.kills = 0}},
            .expected = IsolatedHuntStop::NO_PROGRESS, .steps = "marker align advance:600:5000 "},
        {.name = "Hunt time expiry forbids flight", .limits = {.max_rounds = 2, .duration_ms = 500,
            .ascent = {.enabled = true, .segments_per_ascent = 1}}, .segment_time = 1000,
            .expected = IsolatedHuntStop::TIME_LIMIT, .steps = "marker align advance:600:5000 "},
        {.name = "Meal expiry forbids flight", .limits = {.max_rounds = 2, .power_duration_ms = 500,
            .ascent = {.enabled = true, .segments_per_ascent = 1}}, .segment_time = 1000,
            .expected = IsolatedHuntStop::POWER_EXPIRED, .steps = "marker align advance:600:5000 "},
        {.name = "Meal expiry during alignment forbids movement", .limits = {.max_rounds = 2, .power_duration_ms = 500,
            .ascent = {.enabled = true}}, .align_time = 1000,
            .expected = IsolatedHuntStop::POWER_EXPIRED, .steps = "marker align "},
        {.name = "Flight consumes hunt budget before reanchor", .limits = {.max_rounds = 2, .duration_ms = 500,
            .ascent = {.enabled = true, .segments_per_ascent = 1}}, .flight_time = 1000,
            .expected = IsolatedHuntStop::TIME_LIMIT, .steps = "marker align advance:600:5000 flight marker "},
    };
}
UnitTestResult check_cases(const std::vector<CorridorCase>& cases){
    for (const auto& test : cases){
        CorridorActions actions;
        actions.test = test;
        actions.clock = test.initial_time;
        const auto result = run_isolated_corridor({}, test.limits, actions, 0, 0);
        if (result.stop != test.expected || actions.steps != test.steps){
            return test.name + ": expected stop=" + std::to_string((int)test.expected) + " [" + test.steps +
                "]; read stop=" + std::to_string((int)result.stop) + " [" + actions.steps + "]";
        }
        if (test.expected != IsolatedHuntStop::ROUND_LIMIT && test.expected != IsolatedHuntStop::TIME_LIMIT &&
            test.expected != IsolatedHuntStop::POWER_EXPIRED && can_leave_isolated_hunt(result.stop)){
            return test.name + ": expected no departure after failure; read departure allowed";
        }
    }
    return true;
}

}
UnitTestResult test_isolated_hunt_corridor(){
    if (isolated_stage_timeout_ms(1000, UINT64_MAX, 90000) != 90000){
        return "Expected an unlimited hunt deadline to preserve the bounded startup stage; read overflowed timeout";
    }
    if (isolated_stage_timeout_ms(1000, 900, 90000) != 0 ||
        isolated_stage_timeout_ms(1000, 1250, 90000) != 250){
        return "Expected expired deadline=0ms and remaining deadline=250ms; read incorrect bounded timeout";
    }
    CorridorActions actions;
    IsolatedHuntRequest request;
    const IsolatedHuntLimits limits{.max_rounds = 3, .ascent = {.enabled = true, .segments_per_ascent = 2}};
    const auto result = run_isolated_corridor(request, limits, actions, 0, 0);
    const std::string expected = "marker align advance:600:5000 marker align advance:600:5000 flight marker align advance:600:5000 ";
    if (result.rounds != 3 || result.kills != 3 || result.refreshes != 1 || actions.steps != expected){
        return "Expected three bounded segments and one flight/reanchor [" + expected + "]; read [" + actions.steps + "]";
    }
    CorridorActions invalid_actions;
    auto invalid = limits;
    invalid.ascent.segment_ms = 0;
    const auto rejected = run_isolated_corridor(request, invalid, invalid_actions, 0, 0);
    if (rejected.stop != IsolatedHuntStop::INVALID_PLAN || !invalid_actions.steps.empty()){
        return "Expected invalid zero-length segment rejected before actions; read stop=" + std::to_string((int)rejected.stop);
    }
    for (const auto& cases : {navigation_cases(), event_cases(), budget_cases()}){
        const auto result = check_cases(cases);
        if (result.result != UnitTestResult::PASSED){ return result; }
    }
    return true;
}
}
}
}
