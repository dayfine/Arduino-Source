/*  AutoStory Moved Trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <format>
#include <optional>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

//  "<x>,<y>" of a TRAINER_AT_FLAG, or nothing if it isn't that.
std::optional<std::pair<int, int>> parse_tile(std::string_view text){
    const size_t comma = text.find(',');
    if (comma == std::string_view::npos){
        return std::nullopt;
    }
    int x = 0;
    int y = 0;
    const std::string_view x_text = text.substr(0, comma);
    const std::string_view y_text = text.substr(comma + 1);
    const std::from_chars_result x_read = std::from_chars(x_text.data(), x_text.data() + x_text.size(), x);
    const std::from_chars_result y_read = std::from_chars(y_text.data(), y_text.data() + y_text.size(), y);
    if (x_read.ec != std::errc() || x_read.ptr != x_text.data() + x_text.size()
        || y_read.ec != std::errc() || y_read.ptr != y_text.data() + y_text.size()){
        return std::nullopt;
    }
    return std::pair{x, y};
}

//  The way from `from` to `to`, on the same row or column.
Direction towards(const TilePos& from, const TilePos& to){
    if (to.x == from.x){
        return to.y < from.y ? Direction::NORTH : Direction::SOUTH;
    }
    return to.x < from.x ? Direction::WEST : Direction::EAST;
}

} // namespace


std::vector<TrainerMove> moved_trainers(const std::set<std::string>& run_flags){
    std::vector<TrainerMove> moved;
    for (const std::string& flag : run_flags){
        if (!flag.starts_with(TRAINER_AT_FLAG)){
            continue;
        }
        const std::string_view rest = std::string_view(flag).substr(TRAINER_AT_FLAG.size());
        const size_t equals = rest.find('=');
        const std::optional<std::pair<int, int>> tile = equals == std::string_view::npos ? std::nullopt : parse_tile(rest.substr(equals + 1));
        if (!tile || equals == 0){
            throw ParseException(std::format("run flag \"{}\" isn't \"{}<trainer>=<x>,<y>\"", flag, TRAINER_AT_FLAG));
        }
        moved.emplace_back(TrainerMove{.trainer = std::string(rest.substr(0, equals)), .x = tile->first, .y = tile->second});
    }
    return moved;
}

std::string trainer_at_flag(const TrainerMove& move){
    return std::format("{}{}={},{}", TRAINER_AT_FLAG, move.trainer, move.x, move.y);
}

std::optional<TrainerMove> trainer_walking_up(
    const std::vector<MapData>& maps, const MapPosition& tile, const std::set<std::string>& beaten
){
    const MapData* map = nullptr;
    for (const MapData& candidate : maps){
        map = candidate.name == tile.map ? &candidate : map;
    }
    std::optional<TrainerMove> move;
    for (size_t c = 0; map != nullptr && c < map->objects.size(); c++){
        const Pokemon::MapObject& object = map->objects[c];
        if (!trainer_may_see(*map, object, tile.point) || beaten.contains(trainer_id(*map, object))){
            continue;
        }
        if (move || object.wander){
            return std::nullopt;
        }
        const TilePos beside = tile.point.next(towards(tile.point, TilePos{object.x, object.y}));
        move = TrainerMove{.trainer = trainer_id(*map, object), .x = beside.x, .y = beside.y};
    }
    return move;
}

std::vector<MapData> with_trainers_moved(std::vector<MapData> maps, const std::vector<TrainerMove>& moved){
    for (const TrainerMove& move : moved){
        for (MapData& map : maps){
            for (Pokemon::MapObject& object : map.objects){
                if (!object.trainer || trainer_id(map, object) != move.trainer){
                    continue;
                }
                object.x = move.x;
                object.y = move.y;
                object.wander.reset();
                object.trainer.reset();
            }
        }
    }
    return maps;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
