/*  Overworld Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_PokemonCenterRoom.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view MAP_NAMES[] = {
    //  From a new game to Brock.
    "PalletTown",
    "PalletTown_PlayersHouse_1F",
    "PalletTown_PlayersHouse_2F",
    "PalletTown_RivalsHouse",
    "PalletTown_ProfessorOaksLab",
    "Route1",
    "ViridianCity",
    "ViridianCity_Mart",
    "ViridianCity_PokemonCenter_1F",
    "Route22",
    "Route2",
    "Route2_ViridianForest_SouthEntrance",
    "ViridianForest",
    "Route2_ViridianForest_NorthEntrance",
    "PewterCity",
    "PewterCity_PokemonCenter_1F",
    "PewterCity_Mart",
    "PewterCity_Museum_1F",
    "PewterCity_Gym",
    //  Route 3 to the Magikarp salesman in Route 4's Pokemon Center.
    "Route3",
    "Route4",
    "Route4_PokemonCenter_1F",
    //  Mt. Moon, from Route 4 west of it to Route 4 east of it.
    "MtMoon_1F",
    "MtMoon_B1F",
    "MtMoon_B2F",
    //  Cerulean Cave, for Mewtwo.
    "CeruleanCity_PokemonCenter_1F",
    "CeruleanCity",
    "Route24",
    //  Cerulean City's Mart and Gym, and Route 25 to Bill's Sea Cottage.
    "CeruleanCity_Mart",
    "CeruleanCity_Gym",
    "Route25",
    "Route25_SeaCottage",
    //  The burgled house, the way south out of Cerulean; Route 5, the
    //  Underground Path and Route 6 to Vermilion City, and the S.S.
    //  Anne's way to the Captain.
    "CeruleanCity_House2",
    "Route5",
    "UndergroundPath_NorthEntrance",
    "UndergroundPath_NorthSouthTunnel",
    "UndergroundPath_SouthEntrance",
    "Route6",
    "VermilionCity",
    "VermilionCity_PokemonCenter_1F",
    "SSAnne_Exterior",
    "SSAnne_1F_Corridor",
    "SSAnne_2F_Corridor",
    "SSAnne_CaptainsOffice",
    //  Vermilion Gym, past the tree Cut fells.
    "VermilionCity_Gym",
    //  The BIKE VOUCHER and the BICYCLE.
    "VermilionCity_PokemonFanClub",
    "CeruleanCity_BikeShop",
    //  East over Route 9 to Route 10's Pokemon Center.
    "Route9",
    "Route10",
    "Route10_PokemonCenter_1F",
    //  Rock Tunnel to Lavender Town's Pokemon Center.
    "RockTunnel_1F",
    "RockTunnel_B1F",
    "LavenderTown",
    "LavenderTown_PokemonCenter_1F",
    "LavenderTown_Mart",
    //  West from Lavender: Route 8, the Underground Path under Saffron and
    //  Route 7 to Celadon City.
    "Route8",
    "UndergroundPath_EastEntrance",
    "UndergroundPath_EastWestTunnel",
    "UndergroundPath_WestEntrance",
    "Route7",
    "CeladonCity",
    //  Celadon Gym, past the tree Cut fells.
    "CeladonCity_Gym",
    //  The Game Corner's poster and the Rocket Hideout down to B4F.
    "CeladonCity_GameCorner",
    "RocketHideout_B1F",
    "RocketHideout_B2F",
    "RocketHideout_B3F",
    "RocketHideout_B4F",
    //  The Pokemon Tower to Mr. Fuji, and his house in Lavender Town.
    "PokemonTower_1F",
    "PokemonTower_2F",
    "PokemonTower_3F",
    "PokemonTower_4F",
    "PokemonTower_5F",
    "PokemonTower_6F",
    "PokemonTower_7F",
    "LavenderTown_VolunteerPokemonHouse",
    //  Celadon Mansion's floors to the roof room's Eevee.
    "CeladonCity_Condominiums_1F",
    "CeladonCity_Condominiums_2F",
    "CeladonCity_Condominiums_3F",
    "CeladonCity_Condominiums_Roof",
    "CeladonCity_Condominiums_RoofRoom",
    //  The Department Store's 1F and its 5F's X items, by the lift (coded:
    //  department_store_lift_room()).
    "CeladonCity_DepartmentStore_1F",
    "CeladonCity_DepartmentStore_5F",
    //  Route 16's house with HM02 Fly, past the Cut tree and the north gate.
    "Route16",
    "Route16_NorthEntrance_1F",
    "Route16_House",
    //  Route 18 from the Cycling Road's foot (Route 17 has no map: its
    //  slope is ridden by presses, PokemonFRLG_AutoStorySlope.h), its gate,
    //  and Fuchsia City to the Safari Zone's door.
    "Route18",
    "Route18_EastEntrance_1F",
    "FuchsiaCity",
    //  Fuchsia's Mart: Super Potions before Koga's gym (12.01).
    "FuchsiaCity_Mart",
    //  The Safari Zone, pokeguy's way through it: its entrance, the
    //  center, east, north and west areas, and the Secret House (HM03).
    "FuchsiaCity_SafariZone_Entrance",
    "SafariZone_Center",
    "SafariZone_East",
    "SafariZone_North",
    "SafariZone_West",
    "SafariZone_SecretHouse",
    //  Koga's gym, and the Warden's house (GOLD TEETH for HM04).
    "FuchsiaCity_Gym",
    "FuchsiaCity_WardensHouse",
    //  Route 7's east gate, where the guard takes the TEA, and Saffron City.
    "Route7_EastEntrance",
    "SaffronCity",
    //  Sabrina's gym: nine rooms joined by teleport pads (warps).
    "SaffronCity_Gym",
    //  Cinnabar Island's Center and Blaine's gym, through its quiz doors.
    "CinnabarIsland",
    "CinnabarIsland_Gym",
    //  Pallet Town south over Route 21 to Cinnabar, and Viridian Gym with its
    //  spinner tiles (the map file's glyphs: PokemonFRLG_OverworldSpin.h).
    "Route21_North",
    "Route21_South",
    "ViridianCity_Gym",
    //  Silph Co.'s floors, 1F to 11F (the lift's map, SilphCo_Elevator, is
    //  not built in).
    "SilphCo_1F",
    "SilphCo_2F",
    "SilphCo_3F",
    "SilphCo_4F",
    "SilphCo_5F",
    "SilphCo_6F",
    "SilphCo_7F",
    "SilphCo_8F",
    "SilphCo_9F",
    "SilphCo_10F",
    "SilphCo_11F",
    //  The Pokemon Mansion's four floors, for the SECRET KEY.
    "PokemonMansion_1F",
    "PokemonMansion_2F",
    "PokemonMansion_3F",
    "PokemonMansion_B1F",
    "CeruleanCave_1F",
    "CeruleanCave_2F",
    "CeruleanCave_B1F",
};


//  Whether `maps` has a map named `name`.
bool has_map(const std::vector<MapData>& maps, const std::string& name){
    return std::any_of(maps.begin(), maps.end(), [&](const MapData& map){ return map.name == name; });
}

//  The coded room (pokemon_center_room()) of each Pokemon Center whose
//  door is on one of `maps` but which has no map file of its own:
//  Celadon City's.
void add_unmapped_centers(std::vector<MapData>& maps){
    const size_t towns = maps.size();
    for (size_t town = 0; town < towns; town++){
        for (const MapWarp& warp : maps[town].warps){
            if (warp.to && is_pokemon_center(warp.to->map) && !has_map(maps, warp.to->map)){
                add_pokemon_center_room(maps, town);
                break;
            }
        }
    }
}

//  Pallet Town's fat man wanders over (7..19, 15..19), which holds the whole
//  shore (7..10, 16) the way onto Route 21's water goes by. A planned route
//  never crosses a wanderer's box, so none would reach the water. He stands on
//  his own tile (13, 17) in the planner's model: east of the shore, off the
//  way; a person in the way on the console is waited out (17.01).
void waive_pallet_fat_man_box(MapData& map){
    if (map.name != "PalletTown"){
        return;
    }
    for (MapObject& object : map.objects){
        if (object.name == "fat_man"){
            object.wander.reset();
        }
    }
}

const std::vector<MapData>& all_maps(){
    static const std::vector<MapData> MAPS = []{
        std::vector<MapData> maps;
        for (std::string_view name : MAP_NAMES){
            maps.push_back(load_map(std::string(name)));
            waive_pallet_fat_man_box(maps.back());
        }
        add_unmapped_centers(maps);
        return maps;
    }();
    return MAPS;
}

} // namespace


MapData parse_map(const std::string& json, const std::string& source){
    return Pokemon::parse_map(parse_json(json), source);
}

MapData load_map(const std::string& name){
    const std::string source = "Maps/" + name + ".json";
    const std::vector<std::pair<std::string, std::string>>& files = overworld_map_json();
    const auto file = std::find_if(files.begin(), files.end(), [&](const auto& item){ return item.first == name; });
    if (file == files.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, source + " is not built into the program.");
    }
    MapData map = parse_map(file->second, source);
    if (map.name != name){
        throw ParseException(source + ": name is \"" + map.name + "\", not \"" + name + "\".");
    }
    return map;
}

std::vector<MapData> overworld_maps(const std::set<std::string>& set_flags){
    std::vector<MapData> maps = all_maps();
    for (MapData& map : maps){
        hide_objects(map, set_flags);
    }
    return maps;
}

TilePos map_point(const std::vector<MapData>& maps, const std::string& map, const std::string& point){
    for (const MapData& data : maps){
        if (data.name != map){
            continue;
        }
        const auto iter = data.points.find(point);
        if (iter != data.points.end()){
            return iter->second;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No point \"" + point + "\" on map " + map + ".");
}

bool warp_keeps_bike(const std::string& map){
    static const std::set<std::string, std::less<>> MAPS{
        "Route16_NorthEntrance_1F", "Route18_EastEntrance_1F", "Route16", "Route18",
        //  The Safari Zone's areas allow cycling, and their edge warps lead
        //  from one to the next (pokefirered SafariZone_* map.json).
        "SafariZone_Center", "SafariZone_East", "SafariZone_North", "SafariZone_West",
    };
    return MAPS.contains(map);
}

std::set<std::string> map_flag_set(const std::vector<MapData>& maps, const std::string& name){
    std::set<std::string> flags;
    for (const MapData& map : maps){
        const auto iter = map.flag_sets.find(name);
        if (iter != map.flag_sets.end()){
            flags.insert(iter->second.begin(), iter->second.end());
        }
    }
    return flags;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
