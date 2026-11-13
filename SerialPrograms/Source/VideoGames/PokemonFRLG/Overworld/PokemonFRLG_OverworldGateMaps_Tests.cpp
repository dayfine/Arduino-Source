/*  Overworld Gate Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The real maps, pinned PokemonSimulation resources: SilphCo_5F (card-key
 *  doors) and PokemonMansion_1F and _3F (statue switch, barriers, holes).
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_GatedPlanner.h"
#include "PokemonFRLG_OverworldGateMaps_Tests.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;

namespace{


//  Silph 5F, Mansion 1F and 3F, in that order.
const std::vector<MapData>& maps(){
    static const std::vector<MapData> loaded{
        load_map("SilphCo_5F"),
        load_map("PokemonMansion_1F"),
        load_map("PokemonMansion_3F"),
    };
    return loaded;
}
constexpr size_t SILPH = 0;
constexpr size_t MANSION_1F = 1;
constexpr size_t MANSION_3F = 2;

const ConnectedMaps& world(){
    static const ConnectedMaps connected(maps());
    return connected;
}


//  Walks the maps with the gates as the state says; a step onto a step-on
//  warp takes it. Walk-into warps are not modeled: they are not the topic.
class GateGame{
public:
    using State = MapPlace;
    using StateHash = std::hash<MapPlace>;

    std::optional<Step<State>> press(const State& state, Direction direction, const GateState& gates) const{
        const MapData& map = world().map(state.map);
        const TilePos next = state.tile.next(direction);
        if (!map.tiles.contains(next) || !walkable(resolved_tile(map, next, gates))){
            return std::nullopt;
        }
        Step<State> step;
        step.next = {state.map, next};
        step.facing = direction;
        for (const MapWarp& warp : map.warps){
            if (warp.enter != WarpEntry::STEP_ON || warp.x != next.x || warp.y != next.y){
                continue;
            }
            const std::optional<MapPlace> arrival = warp.to ? world().warp_arrival(warp) : std::nullopt;
            if (!arrival){
                return std::nullopt;
            }
            step.next = *arrival;
            step.warped = true;
            step.ends_leg = true;
        }
        return step;
    }
    bool held_at(const State& state, Direction direction, const GateState& gates) const{
        const MapData& map = world().map(state.map);
        const TilePos next = state.tile.next(direction);
        return !map.tiles.contains(next) || !walkable(resolved_tile(map, next, gates));
    }
    MapPlace place_of(const State& state) const{
        return state;
    }

private:
    static bool walkable(TileKind tile){
        return tile == TileKind::OPEN || tile == TileKind::TALL_GRASS || tile == TileKind::FALL_HOLE;
    }
};

using GatedLegs = std::vector<Leg<GatedState<MapPlace>>>;

std::optional<GatedLegs> plan(
    const MapPlace& from, const MapPlace& to, const GateState& gates, bool card_key_held
){
    GateRules rules;
    rules.card_key_held = card_key_held;
    return plan_gated_legs(
        GateGame(), world(), rules, GatedStart<MapPlace>{from, gates},
        GatedGoal<MapPlace>{std::function<bool(const MapPlace&)>([to](const MapPlace& place){ return place == to; })}
    );
}

int count_actions(const GatedLegs& legs, int action){
    int count = 0;
    for (const auto& leg : legs){
        if (leg.kind == LegKind::ACTION && leg.action == action){
            count++;
        }
    }
    return count;
}


//  Reader

UnitTestResult test_reads_card_key_doors(){
    const MapData& silph = maps()[SILPH];
    if (silph.gates.size() != 3 || !silph.switches.empty()){
        return std::format("Expected 3 gates and no switches, got {} and {}.", silph.gates.size(), silph.switches.size());
    }
    const auto found = silph.gates.find("door_1");
    if (found == silph.gates.end()){
        return "No door_1.";
    }
    const MapGate& door = found->second;
    if (door.kind != GateKind::CARD_KEY || door.flag != "FLAG_SILPH_5F_DOOR_1" || door.open_at_new_game ||
        door.open_tile != TileKind::OPEN || door.tiles.size() != 4
    ){
        return "door_1 is not a closed card-key door of 4 tiles opening to floor with its flag.";
    }
    if (silph.tiles.at(TilePos{7, 9}) != TileKind::GATE){
        return "Tile (7, 9) is not a gate.";
    }
    return true;
}

UnitTestResult test_reads_switches(){
    const MapData& first = maps()[MANSION_1F];
    if (first.gates.size() != 4 || first.switches.size() != 1){
        return std::format("Expected 4 gates and 1 switch, got {} and {}.", first.gates.size(), first.switches.size());
    }
    const MapGate& open_first = first.gates.at("barrier_2");
    if (open_first.kind != GateKind::SWITCH || open_first.switch_id != "mansion" || open_first.open_when_on || !open_first.open_at_new_game){
        return "1F barrier_2 should open when the switch is off.";
    }
    if (!first.gates.at("barrier_1").open_when_on){
        return "1F barrier_1 should open when the switch is on.";
    }
    const MapSwitch& pressing = first.switches.at("mansion");
    if (!pressing.toggle || pressing.floors.size() != 4 || pressing.flag != "FLAG_POKEMON_MANSION_SWITCH_STATE"){
        return "The switch is a toggle on 4 floors with its flag.";
    }
    if (pressing.statues.size() != 1 || pressing.statues[0].tile != TilePos{5, 5} ||
        pressing.statues[0].face_from != TilePos{5, 6} || pressing.statues[0].facing != Direction::NORTH
    ){
        return "1F statue should be (5, 5), pressed from (5, 6) facing north.";
    }
    return true;
}

UnitTestResult test_reads_one_way_warps(){
    int drops = 0;
    for (const MapWarp& warp : maps()[MANSION_3F].warps){
        drops += warp.one_way ? 1 : 0;
    }
    if (drops != 5){
        return std::format("Expected 5 one-way warps on 3F, got {}.", drops);
    }
    for (const MapWarp& warp : maps()[MANSION_1F].warps){
        if (warp.one_way){
            return "A 1F warp is one-way.";
        }
    }
    return true;
}

//  A 3x3 map with `gates` and `switches` (JSON object text) on tiles "G..".
std::string parse_error(const std::string& rows, const std::string& gates, const std::string& switches){
    const std::string text = std::format(R"({{
        "format": "pokesim-runtime-map/2", "name": "T",
        "source": {{"decomp_file": "x", "revision": "y", "date": "z"}},
        "tiles": [{}], "points": {{}}, "warps": [], "connections": [],
        "gates": {}, "switches": {}
    }})", rows, gates, switches);
    try{
        Pokemon::parse_map(parse_json(text), "T.json");
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

UnitTestResult test_gate_errors(){
    const std::string rows = R"("G..", "...", "...")";
    const std::string door = R"({"d": {"tiles": [[0, 0]], "open_glyph": ".", "new_game": "closed", "opens_by": "card_key_face_a", "flag": "F"}})";
    if (!parse_error(rows, door, "{}").empty()){
        return "A valid gate failed to parse.";
    }
    if (parse_error(rows, "{}", "{}").empty()){
        return "A G tile in no gate was accepted.";
    }
    if (parse_error(R"("...", "...", "...")", door, "{}").empty()){
        return "A gate on a tile that is not G was accepted.";
    }
    const std::string unknown_switch = R"({"d": {"tiles": [[0, 0]], "open_glyph": ".", "new_game": "closed", "switch": "nope", "open_when": "on"}})";
    if (parse_error(rows, unknown_switch, "{}").empty()){
        return "A gate on an unlisted switch was accepted.";
    }
    const std::string far_statue = R"({"s": {"flag": "F", "toggle": true, "floors": ["T"], "statues": [{"coord": [9, 9], "face_from": [1, 1], "facing": "north"}]}})";
    if (parse_error(rows, door, far_statue).empty()){
        return "A statue outside the map was accepted.";
    }
    return true;
}


//  Victory Road: each floor's boulder-fed floor switch opens its barrier, a
//  top and a bottom tile. The boulder is not modelled: the switch has no
//  statue to press, only its flag, which the game sets for good.
UnitTestResult test_victory_road_barriers(){
    struct Floor{
        const char* name;
        size_t gates;
        size_t switches;
        const char* switch_id;
        const char* barrier;
        const char* flag;
        TilePos top;
    };
    const Floor floors[] = {
        {"VictoryRoad_1F", 2, 1, "floor_switch_1", "barrier_1_top", "VAR_MAP_SCENE_VICTORY_ROAD_1F", {12, 14}},
        {"VictoryRoad_2F", 4, 2, "floor_switch_2", "barrier_2_top", "VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER2", {33, 16}},
        {"VictoryRoad_3F", 2, 1, "floor_switch_1", "barrier_1_top", "VAR_MAP_SCENE_VICTORY_ROAD_3F", {12, 12}},
    };
    for (const Floor& floor : floors){
        const MapData map = load_map(floor.name);
        if (map.gates.size() != floor.gates || map.switches.size() != floor.switches){
            return std::format("{}: expected {} gates and {} switches, got {} and {}.",
                floor.name, floor.gates, floor.switches, map.gates.size(), map.switches.size());
        }
        const auto found = map.switches.find(floor.switch_id);
        if (found == map.switches.end() || found->second.toggle || found->second.flag != floor.flag || !found->second.statues.empty()){
            return std::format("{}: {} should be a one-way flag switch with no statue.", floor.name, floor.switch_id);
        }
        const MapGate& top = map.gates.at(floor.barrier);
        if (top.kind != GateKind::SWITCH || top.switch_id != floor.switch_id || !top.open_when_on || top.open_at_new_game){
            return std::format("{}: the barrier should be closed at new game and open with {}.", floor.name, floor.switch_id);
        }
        GateState off;
        GateState on;
        on.switches_on.insert(floor.switch_id);
        if (resolved_tile(map, floor.top, off) != TileKind::WALL || resolved_tile(map, floor.top, on) == TileKind::WALL){
            return std::format("{}: the barrier at ({}, {}) should be a wall until {} is on.", floor.name, floor.top.x, floor.top.y, floor.switch_id);
        }
    }
    return true;
}

//  State and tiles

UnitTestResult test_tiles_follow_state(){
    const MapData& silph = maps()[SILPH];
    GateState state;
    if (resolved_tile(silph, TilePos{7, 9}, state) != TileKind::WALL){
        return "A closed door is not a wall.";
    }
    state.opened.insert(gate_key("SilphCo_5F", "door_1"));
    if (resolved_tile(silph, TilePos{7, 9}, state) != TileKind::OPEN || resolved_tile(silph, TilePos{7, 18}, state) != TileKind::WALL){
        return "Only door_1 should be open.";
    }
    if (resolve_tiles(silph, state).at(TilePos{8, 10}) != TileKind::OPEN){
        return "resolve_tiles disagrees.";
    }

    //  The Mansion's gates follow the one switch on every floor.
    const MapData& first = maps()[MANSION_1F];
    const MapData& third = maps()[MANSION_3F];
    GateState off;
    GateState on;
    on.switches_on.insert("mansion");
    if (resolved_tile(first, TilePos{22, 10}, off) != TileKind::WALL || resolved_tile(first, TilePos{22, 10}, on) != TileKind::TALL_GRASS){
        return "1F barrier_1 should open with the switch on.";
    }
    if (resolved_tile(third, TilePos{21, 6}, off) != TileKind::TALL_GRASS || resolved_tile(third, TilePos{21, 6}, on) != TileKind::WALL){
        return "3F barrier_1 should close with the switch on.";
    }
    return true;
}

UnitTestResult test_gate_change(){
    GateState state;
    GateChange change;
    change.opened.insert("SilphCo_5F/door_1");
    change.switches["mansion"] = true;
    apply_gate_change(state, change);
    if (!state.opened.contains("SilphCo_5F/door_1") || !state.switch_on("mansion")){
        return "The change was not applied.";
    }
    GateChange off;
    off.switches["mansion"] = false;
    GateState after = state;
    apply_gate_change(after, off);
    if (after.switch_on("mansion") || !after.opened.contains("SilphCo_5F/door_1")){
        return "Turning the switch off changed more, or nothing.";
    }
    GateState rebuilt;
    apply_gate_change(rebuilt, gate_change_between(GateState(), state));
    if (!(rebuilt == state)){
        return "gate_change_between from nothing does not rebuild the state.";
    }
    apply_gate_change(rebuilt, gate_change_between(state, after));
    if (!(rebuilt == after)){
        return "gate_change_between does not carry state to state.";
    }
    const GateState copy = after;
    if (std::hash<GateState>()(copy) != std::hash<GateState>()(rebuilt)){
        return "Equal states hash differently.";
    }
    return true;
}


//  Planner

UnitTestResult test_silph_door_needs_key(){
    const MapPlace from{SILPH, {10, 9}};
    const MapPlace to{SILPH, {5, 9}};
    if (plan(from, to, GateState(), false)){
        return "Planned into the room without the Card Key.";
    }
    const auto legs = plan(from, to, GateState(), true);
    if (!legs){
        return "No route with the Card Key.";
    }
    if (count_actions(*legs, ACTION_OPEN_GATE) != 1){
        return std::format("Expected one open-gate leg, got {}.", count_actions(*legs, ACTION_OPEN_GATE));
    }
    GateChange expected;
    expected.opened.insert("SilphCo_5F/door_1");
    for (const auto& leg : *legs){
        if (leg.kind == LegKind::ACTION){
            if (!(gate_change_of(leg) == expected)){
                return "The open-gate leg did not open door_1 alone.";
            }
            if (leg.direction != Direction::WEST || !(leg.start.inner == MapPlace{SILPH, {9, 9}})){
                return "Expected to open it facing west from (9, 9).";
            }
        }
    }
    if (!gates_after(GateState(), *legs).opened.contains("SilphCo_5F/door_1")){
        return "The state after the plan does not have door_1 open.";
    }
    return true;
}

UnitTestResult test_silph_open_door_needs_no_key(){
    GateState opened;
    opened.opened.insert(gate_key("SilphCo_5F", "door_1"));
    const auto legs = plan({SILPH, {10, 9}}, {SILPH, {5, 9}}, opened, false);
    if (!legs){
        return "No route through an open door.";
    }
    if (count_actions(*legs, ACTION_OPEN_GATE) != 0){
        return "Opened a door that was open.";
    }
    return true;
}

UnitTestResult test_mansion_press_opens_barrier(){
    //  (19, 13) lies behind barrier_1 and barrier_2 of 1F, which open with the switch on.
    const MapPlace from{MANSION_1F, {5, 8}};
    const MapPlace to{MANSION_1F, {19, 13}};
    const auto legs = plan(from, to, GateState(), false);
    if (!legs){
        return "No route.";
    }
    if (count_actions(*legs, ACTION_PRESS_SWITCH) != 1){
        return std::format("Expected one statue press, got {}.", count_actions(*legs, ACTION_PRESS_SWITCH));
    }
    if (!gates_after(GateState(), *legs).switch_on("mansion")){
        return "The switch is not on after the plan.";
    }
    for (const auto& leg : *legs){
        if (leg.kind == LegKind::ACTION && (leg.direction != Direction::NORTH || !(leg.start.inner == MapPlace{MANSION_1F, {5, 6}}))){
            return "Expected to press the statue from (5, 6) facing north.";
        }
    }
    return true;
}

UnitTestResult test_mansion_press_again(){
    //  Switch already on; (25, 21) is reachable only with it off.
    GateState on;
    on.switches_on.insert("mansion");
    const auto legs = plan({MANSION_1F, {5, 6}}, {MANSION_1F, {25, 21}}, on, false);
    if (!legs){
        return "No route.";
    }
    if (count_actions(*legs, ACTION_PRESS_SWITCH) != 1 || gates_after(on, *legs).switch_on("mansion")){
        return "Expected one press, ending with the switch off.";
    }
    return true;
}

UnitTestResult test_mansion_no_press_needed(){
    //  Reachable with the switch as it is: no press.
    const auto legs = plan({MANSION_1F, {5, 6}}, {MANSION_1F, {5, 8}}, GateState(), false);
    if (!legs || count_actions(*legs, ACTION_PRESS_SWITCH) != 0){
        return "Pressed a statue that was not needed.";
    }
    return true;
}

UnitTestResult test_hole_one_way(){
    //  The 3F hole at (18, 18) drops to 1F's (19, 22); nothing leads back up.
    const auto down = plan({MANSION_3F, {18, 17}}, {MANSION_1F, {19, 22}}, GateState(), false);
    if (!down){
        return "No route down the hole.";
    }
    bool warped = false;
    for (const auto& leg : *down){
        warped = warped || leg.warps;
    }
    if (!warped){
        return "The route does not take the warp.";
    }
    if (plan({MANSION_1F, {19, 22}}, {MANSION_3F, {18, 17}}, GateState(), false)){
        return "Planned back up a hole.";
    }
    return true;
}


struct GatesTestCase{
    struct Input{
        std::string name;
        UnitTestResult (*test)();
    };

    struct Expectation{
        std::string_view description;
    };

    Input input;
    Expectation expectation;
};

class Test_Gates : public UnitTest{
public:
    explicit Test_Gates(GatesTestCase scenario)
        : UnitTest(std::format("PokemonFRLG::OverworldGateMaps - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    GatesTestCase m_scenario;
};

const GatesTestCase GATES_CASES[] = {
    {
        .input = {.name = "reads card-key doors", .test = test_reads_card_key_doors},
        .expectation = {.description = "Silph 5F doors parse with tiles, flag, open glyph"},
    },
    {
        .input = {.name = "reads switches and statues", .test = test_reads_switches},
        .expectation = {.description = "Mansion gates follow the switch; statue tile and press spot"},
    },
    {
        .input = {.name = "reads one-way warps", .test = test_reads_one_way_warps},
        .expectation = {.description = "3F holes are one-way"},
    },
    {
        .input = {.name = "gate errors", .test = test_gate_errors},
        .expectation = {.description = "bad gates fail to parse"},
    },
    {
        .input = {.name = "tiles follow the state", .test = test_tiles_follow_state},
        .expectation = {.description = "closed gate is a wall, open is its glyph, Mansion follows the switch"},
    },
    {
        .input = {.name = "gate change", .test = test_gate_change},
        .expectation = {.description = "apply and between agree"},
    },
    {
        .input = {.name = "a card-key door needs the key", .test = test_silph_door_needs_key},
        .expectation = {.description = "open-gate leg only with the Card Key held"},
    },
    {
        .input = {.name = "an open door needs no key", .test = test_silph_open_door_needs_no_key},
        .expectation = {.description = "no open-gate leg when the state has it open"},
    },
    {
        .input = {.name = "a statue press opens a barrier", .test = test_mansion_press_opens_barrier},
        .expectation = {.description = "one press-switch leg; switch on afterwards"},
    },
    {
        .input = {.name = "a press again closes it", .test = test_mansion_press_again},
        .expectation = {.description = "switch is part of the search state"},
    },
    {
        .input = {.name = "no press when not needed", .test = test_mansion_no_press_needed},
        .expectation = {.description = "does not toggle without reason"},
    },
    {
        .input = {.name = "a hole is one-way", .test = test_hole_one_way},
        .expectation = {.description = "down yes, back up no"},
    },
    {
        .input = {.name = "Victory Road barriers", .test = test_victory_road_barriers},
        .expectation = {.description = "floor switches open the barriers; boulders are not modelled"},
    },
};

} // namespace


void add_tests_OverworldGateMaps(UnitTestDatabase& database){
    for (const auto& scenario : GATES_CASES){
        database.add<Test_Gates>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
