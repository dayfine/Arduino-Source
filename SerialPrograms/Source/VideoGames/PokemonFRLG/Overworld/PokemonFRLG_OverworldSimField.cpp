/*  Overworld Sim: the Field
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What A does on the sim's overworld, facing something: a tree to cut,
 *  the Game Corner's poster, the lift's panel (OverworldSim::use_field()).
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_CodedRooms.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The questions, "<MON> used CUT!" and the tree falling.
constexpr Milliseconds CUT_TIME(4000);

} // namespace


bool OverworldSim::use_field(SimField field, size_t floor){
    switch (field){
    case SimField::CUT_TREE:
        return cut_tree();
    case SimField::POSTER_SWITCH:
        return push_poster_switch();
    case SimField::LIFT_FLOOR:
        return choose_lift_floor(floor);
    case SimField::BIKE_SELECT:
        return press_select();
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
    return Step{(int)(to - m_maps.begin()), PlayerState{out.point, elevation_at(*to, out.point), false}, true, false, false};
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

bool OverworldSim::choose_lift_floor(size_t floor){
    const auto lift = m_script.lift_floors.find(map().name);
    if (lift == m_script.lift_floors.end() || floor >= lift->second.size() || m_frozen){
        violation("no lift panel on " + to_string(m_state.position) + " with a floor " + std::to_string(floor) + ".");
        return false;
    }
    m_lift_floor = floor;
    advance_to(m_now + m_script.timing.message);
    note("the lift goes to " + to_string(lift->second[floor]) + ".");
    return true;
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
