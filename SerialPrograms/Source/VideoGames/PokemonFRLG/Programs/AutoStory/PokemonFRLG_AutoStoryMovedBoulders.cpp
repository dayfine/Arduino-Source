/*  AutoStory Moved Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Pokemon_GateTiles.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_AutoStoryMovedBoulders.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The prefix of a pushed boulder's run flag (moved_boulders()).
constexpr std::string_view BOULDER_AT_FLAG = "boulder_at:";

//  "<x>,<y>", or nothing if `text` isn't that.
std::optional<TilePos> parse_flag_tile(std::string_view text){
    const size_t comma = text.find(',');
    if (comma == std::string_view::npos){
        return std::nullopt;
    }
    TilePos tile;
    const std::string_view x_text = text.substr(0, comma);
    const std::string_view y_text = text.substr(comma + 1);
    const std::from_chars_result x_read = std::from_chars(x_text.data(), x_text.data() + x_text.size(), tile.x);
    const std::from_chars_result y_read = std::from_chars(y_text.data(), y_text.data() + y_text.size(), tile.y);
    if (x_read.ec != std::errc() || x_read.ptr != x_text.data() + x_text.size()
        || y_read.ec != std::errc() || y_read.ptr != y_text.data() + y_text.size()){
        return std::nullopt;
    }
    return tile;
}

std::string boulder_at_flag(const BoulderMove& move){
    return std::format("{}{}@{},{}={},{}", BOULDER_AT_FLAG, move.map, move.from.x, move.from.y, move.at.x, move.at.y);
}

}  // namespace


std::vector<BoulderMove> moved_boulders(const std::set<std::string>& run_flags){
    std::vector<BoulderMove> moved;
    for (const std::string& flag : run_flags){
        if (!flag.starts_with(BOULDER_AT_FLAG)){
            continue;
        }
        const std::string_view rest = std::string_view(flag).substr(BOULDER_AT_FLAG.size());
        const size_t at_sign = rest.find('@');
        const size_t equals = rest.find('=');
        const bool shaped = at_sign != std::string_view::npos && at_sign != 0 && equals != std::string_view::npos && at_sign < equals;
        const std::optional<TilePos> from = shaped ? parse_flag_tile(rest.substr(at_sign + 1, equals - at_sign - 1)) : std::nullopt;
        const std::optional<TilePos> at = shaped ? parse_flag_tile(rest.substr(equals + 1)) : std::nullopt;
        if (!from || !at){
            throw ParseException(std::format("run flag \"{}\" isn't \"{}<map>@<x>,<y>=<x>,<y>\"", flag, BOULDER_AT_FLAG));
        }
        moved.emplace_back(BoulderMove{.map = std::string(rest.substr(0, at_sign)), .from = *from, .at = *at});
    }
    return moved;
}

void note_boulder_pushed(const MapPosition& start, Direction direction, int64_t tiles, std::set<std::string>& run_flags){
    const TilePos boulder = start.point.next(direction);
    BoulderMove move{.map = start.map, .from = boulder, .at = boulder};
    for (const BoulderMove& before : moved_boulders(run_flags)){
        if (before.map == start.map && before.at == boulder){
            run_flags.erase(boulder_at_flag(before));
            move.from = before.from;
        }
    }
    for (int64_t c = 0; c < tiles; c++){
        move.at = move.at.next(direction);
    }
    run_flags.insert(boulder_at_flag(move));
}

std::vector<MapData> with_boulders_moved(std::vector<MapData> maps, const std::vector<BoulderMove>& moved){
    for (const BoulderMove& move : moved){
        for (MapData& map : maps){
            if (map.name != move.map){
                continue;
            }
            for (Pokemon::MapObject& object : map.objects){
                if (object.name == "pushable_boulder" && object.x == move.from.x && object.y == move.from.y){
                    object.x = move.at.x;
                    object.y = move.at.y;
                    break;
                }
            }
        }
    }
    return maps;
}

void boulders_back_after_map_change(std::string_view map_before, std::string_view map_after, std::set<std::string>& run_flags){
    if (map_before == map_after){
        return;
    }
    std::set<std::string> on_switches;
    for (const BoulderMove& move : moved_boulders(run_flags)){
        if (Pokemon::switch_with_button(load_map(move.map), move.at) != nullptr){
            on_switches.insert(boulder_at_flag(move));
        }
    }
    std::erase_if(run_flags, [&](const std::string& flag){
        return flag.starts_with(BOULDER_AT_FLAG) && !on_switches.contains(flag);
    });
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
