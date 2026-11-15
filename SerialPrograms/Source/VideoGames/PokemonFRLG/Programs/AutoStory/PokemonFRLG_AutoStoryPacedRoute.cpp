/*  AutoStory Paced Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <set>
#include <string>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The story's maps, with the Rockets who leave once Silph Co. is cleared
//  gone if `silph_cleared`.
std::vector<MapData> make_story_maps(bool silph_cleared){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    //  The one ball the story picks up in its way: B2F's MOON STONE
    //  blocks column 2 of the arrow floor (09.02's moon_stone).
    flags.insert("FLAG_HIDE_ROCKET_HIDEOUT_B2F_MOON_STONE");
    //  Route 16's Snorlax (31, 13), gone once 11.01 has played the POKE
    //  FLUTE and run: the routes after it go over its tile. None before
    //  it does (10.01 goes north by the Cut tree), and 11.01 walks up to
    //  it from the east.
    flags.insert("FLAG_HIDE_ROUTE_16_SNORLAX");
    if (silph_cleared){
        //  Giovanni's Rockets leave Silph Co. and Saffron City when he has
        //  lost on 11F (SilphCo_11F_EventScript_Giovanni sets both).
        flags.insert("FLAG_HIDE_SILPH_ROCKETS");
        flags.insert("FLAG_HIDE_SAFFRON_ROCKETS");
    }
    return overworld_maps(flags);
}

} // namespace

const std::vector<MapData>& story_maps(){
    static const std::vector<MapData> maps = make_story_maps(false);
    return maps;
}

namespace{

//  story_maps() without the Rockets (SILPH_CLEARED_FLAG).
const std::vector<MapData>& cleared_story_maps(){
    static const std::vector<MapData> maps = make_story_maps(true);
    return maps;
}

} // namespace

const std::vector<MapData>& run_story_maps(const std::set<std::string>& run_flags){
    const bool cleared = run_flags.contains(std::string(SILPH_CLEARED_FLAG));
    const std::vector<MapData>& base = cleared ? cleared_story_maps() : story_maps();
    const std::vector<TrainerMove> moved = moved_trainers(run_flags);
    if (moved.empty()){
        return base;
    }
    //  One copy per set of trainers moved, kept while the program runs:
    //  callers hold references into it. `run_flags` is a set, so the key
    //  lists the moves in one order.
    std::string key = cleared ? "cleared" : "all";
    for (const TrainerMove& move : moved){
        key += std::format("|{}={},{}", move.trainer, move.x, move.y);
    }
    static Mutex lock;
    static std::map<std::string, std::vector<MapData>> moved_maps;
    LockGuard<Mutex> guard(lock);
    auto it = moved_maps.find(key);
    if (it == moved_maps.end()){
        it = moved_maps.emplace(key, with_trainers_moved(base, moved)).first;
    }
    return it->second;
}

std::set<std::string> beaten_trainers(const std::set<std::string>& run_flags){
    std::set<std::string> beaten;
    for (const std::string& flag : run_flags){
        if (flag.starts_with(TRAINER_BEATEN_FLAG)){
            beaten.insert(flag.substr(TRAINER_BEATEN_FLAG.size()));
        }
    }
    return beaten;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
