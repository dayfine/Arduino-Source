/*  BDSP AutoStory - Story simulation triggers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string>
#include <vector>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::TilePos;


//  The story variables whose coord events the table walks onto, from the
//  row (and step) the table's reasons give, with the maps' coord events
//  (Diamond's zone events; Platinum's for the houses): Barry's doorstep and
//  Mom's warning in 00.01, Barry's Route 201 warning, then its pull-back
//  row 19 once he has warned us (r14, r16); the lake's entrance in 00.02;
//  Rowan's stop and Dawn at Sandgem's edge in 00.03; Dawn's catching
//  tutorial on Route 202 from 00.04's start, Mom's call once she has healed
//  us; the Poketch president on Jubilife's row 40 in 00.05b; Barry on Route
//  203 in 01.01; the Gate's Hiker and Oreburgh's welcome in 01.02; Barry's
//  bump in Oreburgh after Roark in 01.05; and Rowan in Jubilife in 01.07.
//  Left out: the Twinleaf guitarist (spent upstairs), Platinum's rival
//  steps around our bedroom's start, Dawn at Jubilife's steps (BDSP starts
//  her below them: an event), and Floaroma's (10..12, 29) (Diamond only).
namespace detail{
std::vector<VarState> story_vars(){
    return {
        {.var = "VAR_TWINLEAF_TOWN_RIVAL_TRIGGER_STATE", .value = 0, .row = "00.01", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 0, .row = "00.01", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 1, .row = "00.01", .after = "barry_warns_us"},
        {.var = "VAR_VERITY_LAKEFRONT_STATE", .value = 0, .row = "00.02", .after = ""},
        {.var = "VAR_FOLLOWER_RIVAL_STATE", .value = 2, .row = "00.03", .after = ""},
        {.var = "VAR_SANDGEM_TOWN_STATE", .value = 0, .row = "00.03", .after = ""},
        {.var = "VAR_ROUTE_202_STATE", .value = 0, .row = "00.04", .after = ""},
        {.var = "CONSOLE_MOMS_CALL", .value = 0, .row = "00.04", .after = "mom_heals_us"},
        {.var = "VAR_POKETCH_CAMPAIGN_STATE", .value = 1, .row = "00.05b", .after = ""},
        {.var = "VAR_ROUTE_203_RIVAL_STATE", .value = 0, .row = "01.01", .after = ""},
        {.var = "VAR_OREBURGH_GATE_1F_HIKER_STATE", .value = 1, .row = "01.02", .after = ""},
        {.var = "VAR_OREBURGH_CITY_STATE", .value = 0, .row = "01.02", .after = ""},
        {.var = "VAR_OREBURGH_CITY_STATE", .value = 2, .row = "01.05", .after = ""},
        {.var = "VAR_JUBILIFE_CITY_STATE", .value = 3, .row = "01.07", .after = ""},
    };
}

namespace{

//  The maps' coord events (story_coord_events()) whose variable holds.
std::optional<VarState> var_state(const CoordEvent& event){
    for (const VarState& state : story_vars()){
        if (state.var == event.var && state.value == event.value){
            return state;
        }
    }
    return std::nullopt;
}

//  A coord event the console showed on other tiles than its map's.
struct ConsoleTrigger{
    std::string map;
    std::string name;
    std::vector<TilePos> tiles;
};
std::vector<ConsoleTrigger> console_triggers(){
    return {
        //  Mom's call after her heal fires on (6, 9), above the mat: a walk
        //  west along row 9 stopped in it there with all 4 taps moved
        //  (lab-B 2026-11-09 17:29), and the step south from (6, 9) after
        //  it went out of the door, where the same step on 2026-11-04 12:20,
        //  pressed inside her scene, moved nobody. The map has the mat (6, 10)
        //  (r45, r46: her line came after that step was pressed).
        //  Verify runs 1-3 (lab-B 2026-11-09 18:19, 18:30, 18:41): from (7, 9)
        //  west, her call stopped us after at most 1 tile, on (6, 9) each time.
        {.map = "PlayerHouse1F", .name = "moms_call", .tiles = {{6, 9}}},
    };
}

RowTrigger row_trigger(const std::string& map, const CoordEvent& event, const VarState& state){
    RowTrigger out{
        .row = state.row, .after = state.after, .map = map,
        .trigger = {.name = event.name, .tiles = event.tiles}, .var = event.var,
    };
    for (const ConsoleTrigger& seen : console_triggers()){
        if (seen.map == map && seen.name == event.name){
            out.trigger.tiles = seen.tiles;
        }
    }
    return out;
}

} // namespace

std::vector<RowTrigger> story_triggers(){
    std::vector<RowTrigger> triggers;
    for (const auto& [map, json] : bdsp_map_json()){
        for (const CoordEvent& event : story_coord_events(map)){
            const std::optional<VarState> state = var_state(event);
            if (state){
                triggers.push_back(row_trigger(map, event, *state));
            }
        }
    }
    return triggers;
}
} // namespace detail


}
}
}
