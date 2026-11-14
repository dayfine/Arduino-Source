/*  Overworld Sim: the Field
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What A does on the sim's overworld, facing something: a tree to cut,
 *  the Game Corner's poster, the lift's panel, the water to Surf onto; and
 *  Fly (OverworldSim::use_field()).
 *
 */

#include <algorithm>
#include <format>
#include "Pokemon/Navigation/Pokemon_GateTiles.h"
#include "PokemonFRLG_CodedRooms.h"
#include "PokemonFRLG_FlyMap.h"
#include "PokemonFRLG_OverworldGatedRoute.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_OverworldSpin.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The questions, "<MON> used CUT!" and the tree falling.
constexpr Milliseconds CUT_TIME(4000);
//  The boulder's slide, one tile. Not measured on the console.
constexpr Milliseconds PUSH_TIME(500);

} // namespace


bool OverworldSim::use_field(SimField field, size_t floor){
    switch (field){
    case SimField::CUT_TREE:
        return cut_tree();
    case SimField::POSTER_SWITCH:
        return push_poster_switch();
    case SimField::PUSH_BOULDER:
        return push_boulder();
    case SimField::LIFT_FLOOR:
        return choose_lift_floor(floor);
    case SimField::BIKE_SELECT:
        return press_select();
    case SimField::SURF:
        return start_surf();
    case SimField::FLY:
        return fly_to_town(floor);
    case SimField::OPEN_GATE:
        return open_gate();
    case SimField::PRESS_SWITCH:
        return press_switch();
    }
    return false;
}

void OverworldSim::add_coded_field(){
    for (MapData& loaded : m_maps){
        add_coded_objects(loaded);
    }
}

bool OverworldSim::slide_on(Step& step) const{
    if (!spin_direction(m_maps[step.map], step.player.position)){
        return true;
    }
    step.spin = spin_slide(m_maps[step.map], step.player);
    if (!step.spin){
        return false;
    }
    step.player = step.spin->end;
    return true;
}

Milliseconds OverworldSim::after_slide(const Step& step){
    if (!step.spin){
        return Milliseconds(0);
    }
    //  The game moves the player through the slide; the hold, if it goes
    //  on, walks on from its end.
    m_state.facing = step.spin->facing;
    note(std::format("slid {} tile(s) to {}.", step.spin->tiles, to_string(step.spin->end.position)));
    return MovementMechanics::SPIN_TILE_TIME * step.spin->tiles;
}

std::optional<OverworldSim::Step> OverworldSim::lift_warp(int map_index) const{
    const auto lift = m_script.lift_floors.find(m_maps[map_index].name);
    if (lift == m_script.lift_floors.end() || !m_lift_floor){
        return std::nullopt;
    }
    const MapPosition& out = lift->second[*m_lift_floor];
    const auto to = std::find_if(m_maps.begin(), m_maps.end(), [&](const MapData& map){ return map.name == out.map; });
    if (to == m_maps.end()){
        return std::nullopt;
    }
    return Step{(int)(to - m_maps.begin()), PlayerState{out.point, elevation_at(*to, out.point), false}, true, std::nullopt, false};
}

bool OverworldSim::cut_tree(){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    const TilePos ahead = m_player.position.next(m_state.facing);
    for (size_t c = 0; c < objects.size(); c++){
        if (objects[c].name == "cut_tree" && objects[c].x == ahead.x && objects[c].y == ahead.y
            && m_state.screen == SimScreen::OVERWORLD && !m_frozen
        ){
            m_cut_trees.emplace_back(m_map, objects[c]);
            objects.erase(objects.begin() + c);
            advance_to(m_now + CUT_TIME);
            note("cut the tree on " + to_string(MapPosition{map().name, ahead}) + ".");
            return true;
        }
    }
    violation("no tree to cut " + std::string(direction_name(m_state.facing)) + " of " + to_string(m_state.position) + ".");
    return false;
}

namespace{

//  The map named `name` in `maps`, or nullptr.
template <typename Maps>
auto* map_named_in(Maps& maps, const std::string& name){
    const auto found = std::find_if(maps.begin(), maps.end(), [&](const MapData& map){ return map.name == name; });
    return found == maps.end() ? nullptr : &*found;
}

} // namespace

void OverworldSim::land_fallen_boulder(const MapPosition& hole){
    const MapData* from = map_named_in(m_maps, hole.map);
    const MapWarp* warp = from == nullptr ? nullptr : warp_at(*from, hole.point);
    if (warp == nullptr || !warp->to){
        return;
    }
    static const std::vector<MapData> ALL = overworld_maps();
    const MapData* below = map_named_in(ALL, warp->to->map);
    MapData* shown = map_named_in(m_maps, warp->to->map);
    if (below == nullptr || shown == nullptr){
        return;
    }
    for (const MapObject& object : below->objects){
        const bool hidden = object.name == "pushable_boulder" && object.hide_flag && std::none_of(
            shown->objects.begin(), shown->objects.end(), [&](const MapObject& item){ return item.x == object.x && item.y == object.y; }
        );
        if (hidden){
            shown->objects.push_back(object);
            note("the boulder lands on " + to_string(MapPosition{shown->name, TilePos{object.x, object.y}}) + ".");
        }
    }
}

bool OverworldSim::push_boulder(){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    const TilePos ahead = m_player.position.next(m_state.facing);
    const auto boulder = std::find_if(objects.begin(), objects.end(), [&](const MapObject& object){
        return object.name == "pushable_boulder" && object.x == ahead.x && object.y == ahead.y;
    });
    if (boulder == objects.end() || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation("no boulder " + std::string(direction_name(m_state.facing)) + " of " + to_string(m_state.position) + ".");
        return false;
    }
    //  A boulder pushed onto a hole falls through it to the floor below,
    //  where the game shows the boulder it hid until then (Victory Road
    //  3F's (34, 18): 2F's (33, 19), FLAG_HIDE_VICTORY_ROAD_2F_BOULDER).
    const TilePos into = ahead.next(m_state.facing);
    if (map().tiles.contains(into) && map().tiles.at(into) == TileKind::FALL_HOLE){
        const MapPosition hole{map().name, into};
        objects.erase(boulder);
        advance_to(m_now + PUSH_TIME);
        note("pushed the boulder into the hole at " + to_string(hole) + ".");
        land_fallen_boulder(hole);
        return true;
    }
    //  The boulder moves where a player on its tile could step.
    const PlayerState on_boulder{ahead, elevation_at(map(), ahead), false};
    if (classify_step(map(), on_boulder, m_state.facing) != StepKind::WALK){
        violation("the boulder on " + to_string(MapPosition{map().name, ahead}) + " does not move " + std::string(direction_name(m_state.facing)) + ".");
        return false;
    }
    const TilePos beyond = ahead.next(m_state.facing);
    boulder->x = beyond.x;
    boulder->y = beyond.y;
    advance_to(m_now + PUSH_TIME);
    note("pushed the boulder to " + to_string(MapPosition{map().name, beyond}) + ".");
    //  A boulder on a floor switch's button turns it on for good (Victory
    //  Road's floors: the switch's flag, then the barrier redrawn).
    const Pokemon::MapSwitch* pressed = m_state.gate_maps.empty()
        ? nullptr
        : Pokemon::switch_with_button(m_state.gate_maps[m_map], beyond);
    if (pressed != nullptr){
        m_state.gates.switches_on.insert(Pokemon::switch_key(*pressed));
        read_gates();
        note(std::format("switch {} is on.", Pokemon::switch_key(*pressed)));
    }
    return true;
}

bool OverworldSim::choose_lift_floor(size_t floor){
    const auto lift = m_script.lift_floors.find(map().name);
    if (lift == m_script.lift_floors.end() || floor >= lift->second.size() || m_frozen){
        violation("no lift panel on " + to_string(m_state.position) + " with a floor " + std::to_string(floor) + ".");
        return false;
    }
    m_lift_floor = floor;
    advance_to(m_now + m_script.timing.message);
    //  The ride shakes the lift; the player can't move until it stops.
    m_frozen = true;
    schedule(m_now + m_script.timing.lift_ride, SimScreen::OVERWORLD, true);
    note("the lift goes to " + to_string(lift->second[floor]) + ".");
    return true;
}

bool OverworldSim::start_surf(){
    const bool water = classify_step(map(), m_player, m_state.facing) == StepKind::START_SURF;
    if (!water || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation("no water to Surf onto " + std::string(direction_name(m_state.facing)) + " of " + to_string(m_state.position) + ".");
        return false;
    }
    advance_to(m_now + m_script.timing.surf_start);
    m_player = PokemonFRLG::after_step(map(), m_player, m_state.facing, StepKind::START_SURF);
    m_state.on_bike = false;
    update_view();
    note("surfed onto " + to_string(m_state.position) + ".");
    return true;
}

void OverworldSim::read_gates(){
    //  Only the tiles: the objects the sim has changed stay as they are.
    //  Maps without a gate were not copied.
    if (m_state.gate_maps.empty() && has_gates(m_maps)){
        m_state.gate_maps = m_maps;
    }
    for (size_t c = 0; c < m_state.gate_maps.size(); c++){
        if (!m_state.gate_maps[c].gates.empty()){
            m_maps[c].tiles = Pokemon::resolve_tiles(m_state.gate_maps[c], m_state.gates);
        }
    }
}

bool OverworldSim::open_gate(){
    if (m_state.gate_maps.empty()){
        violation("no gates on the maps.");
        return false;
    }
    const MapData& source = m_state.gate_maps[m_map];
    const Pokemon::MapGate* gate = Pokemon::closed_card_key_gate_faced(source, m_player.position, m_state.facing, m_state.gates);
    if (gate == nullptr || !m_script.card_key_held || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation(std::format(
            "no card-key gate to open {} of {}{}.",
            direction_name(m_state.facing), to_string(m_state.position), m_script.card_key_held ? "" : " (no Card Key)"
        ));
        return false;
    }
    //  "Opened the door with the CARD KEY!", then the door.
    advance_to(m_now + m_script.timing.message);
    m_state.gates.opened.insert(Pokemon::gate_key(source.name, gate->id));
    read_gates();
    note("opened " + Pokemon::gate_key(source.name, gate->id) + ".");
    return true;
}

bool OverworldSim::press_switch(){
    const Pokemon::MapSwitch* pressed = m_state.gate_maps.empty()
        ? nullptr
        : Pokemon::switch_pressed(m_state.gate_maps[m_map], m_player.position, m_state.facing);
    if (pressed == nullptr || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation(std::format("no statue to press {} of {}.", direction_name(m_state.facing), to_string(m_state.position)));
        return false;
    }
    //  The prompt, YES, and the text after it.
    advance_to(m_now + m_script.timing.message);
    const std::string key = Pokemon::switch_key(*pressed);
    const bool now_on = pressed->toggle ? !m_state.gates.switch_on(key) : true;
    if (now_on){
        m_state.gates.switches_on.insert(key);
    }else{
        m_state.gates.switches_on.erase(key);
    }
    read_gates();
    note(std::format("switch {} is {}.", pressed->id, now_on ? "on" : "off"));
    return true;
}

bool OverworldSim::fly_to_town(size_t town){
    const std::vector<FlyTown>& towns = kanto_fly_towns();
    const MapPosition* landing = town < towns.size() ? &towns[town].landing : nullptr;
    const auto to = std::find_if(m_maps.begin(), m_maps.end(), [&](const MapData& map){
        return landing != nullptr && map.name == landing->map;
    });
    if (to == m_maps.end() || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation(std::format("no Fly to town {} from {}.", town, to_string(m_state.position)));
        return false;
    }
    m_state.warped_from.push_back(map().name);
    m_state.warps++;
    regrow_cut_trees();
    m_map = (int)(to - m_maps.begin());
    m_player = PlayerState{landing->point, elevation_at(*to, landing->point), false};
    m_state.facing = Direction::SOUTH;
    m_state.on_bike = false;
    update_view();
    m_frozen = true;
    schedule(m_now + m_script.timing.message, SimScreen::FADE, false);
    schedule(m_now + m_script.timing.fly, SimScreen::OVERWORLD, true);
    note("flew to " + to_string(m_state.position) + ".");
    return true;
}

Pokemon::LegSpec OverworldSimWalker::leg_spec(const RouteStep& step) const{
    Pokemon::LegSpec leg;
    leg.direction = step.direction;
    leg.tiles = step.tiles + step.warp_slack;
    leg.to_wall = step.to_wall;
    leg.tile_time = OverworldWalkSteps::tile_time(step, m_speed);
    leg.stop_on_interrupt = !step.to_wall;
    leg.stop_if_blocked = step.to_wall && m_rules.stop_if_blocked;
    if (m_rules.hold_over_connections){
        leg.stalled_steps = step.connection_steps;
    }
    return leg;
}

std::optional<WalkedTiles> OverworldSimWalker::use_gate(const RouteStep& step){
    if (m_sim.state().facing != step.direction){
        sim_turn_to(m_sim, step.direction, m_rules.confirm_turn);
    }
    const bool opens = step.action == RouteAction::OPEN_GATE;
    if (!m_sim.use_field(opens ? SimField::OPEN_GATE : SimField::PRESS_SWITCH)){
        fail("walk_overworld_route(): " + to_string(step) + " did not open anything.");
    }
    return std::nullopt;
}

std::optional<WalkedTiles> OverworldSimWalker::surf(const RouteStep& step){
    if (m_sim.state().facing != step.direction){
        sim_turn_to(m_sim, step.direction, m_rules.confirm_turn);
    }
    if (!m_sim.use_field(SimField::SURF)){
        fail("walk_overworld_route(): no Surf prompt facing the water.");
    }
    return std::nullopt;
}

void OverworldSim::regrow_cut_trees(){
    for (auto& [index, tree] : m_cut_trees){
        m_maps[index].objects.push_back(tree);
    }
    m_cut_trees.clear();
}

bool OverworldSim::push_poster_switch(){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    const TilePos ahead = m_player.position.next(m_state.facing);
    const auto poster = std::find_if(objects.begin(), objects.end(), [&](const MapObject& object){
        return object.name == "poster" && object.x == ahead.x && object.y == ahead.y;
    });
    if (poster == objects.end() || m_state.screen != SimScreen::OVERWORLD || m_frozen){
        violation("no poster " + std::string(direction_name(m_state.facing)) + " of " + to_string(m_state.position) + ".");
        return false;
    }
    std::erase_if(objects, [](const MapObject& object){ return object.name == "poster_stairs"; });
    advance_to(m_now + m_script.timing.message);
    note("pushed the switch behind the poster: the stairs are open.");
    return true;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
