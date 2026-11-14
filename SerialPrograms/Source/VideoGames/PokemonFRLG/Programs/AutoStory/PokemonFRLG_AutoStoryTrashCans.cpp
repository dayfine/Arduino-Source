/*  AutoStory Trash Cans
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;
using Pokemon::TilePos;


TilePos trash_can_tile(int can){
    return TilePos{1 + 2 * ((can - 1) % 5), 10 + 2 * ((can - 1) / 5)};
}

std::vector<int> trash_can_neighbors(int can){
    const int column = (can - 1) % 5;
    std::vector<int> cans;
    if (can > 5){
        cans.push_back(can - 5);
    }
    if (column < 4){
        cans.push_back(can + 1);
    }
    if (can <= 10){
        cans.push_back(can + 5);
    }
    if (column > 0){
        cans.push_back(can - 1);
    }
    return cans;
}

TrashCanStand trash_can_stand(int can){
    const TilePos tile = trash_can_tile(can);
    return TrashCanStand{TilePos{tile.x, tile.y - 1}, Direction::SOUTH};
}

const std::vector<int>& trash_can_order(){
    static const std::vector<int> ORDER{1, 2, 3, 4, 5, 10, 9, 8, 7, 6, 11, 12, 13, 14, 15};
    return ORDER;
}

std::vector<Pokemon::MapData> trash_can_maps(const std::vector<Pokemon::MapData>& maps){
    struct Moved{
        std::string_view name;
        TilePos tile;
    };
    static constexpr Moved MOVED[] = {
        {"sailor", {8, 14}},
        {"balding_man", {2, 12}},
        {"gentleman", {6, 8}},
    };
    std::vector<Pokemon::MapData> moved = maps;
    for (Pokemon::MapData& map : moved){
        if (map.name != "VermilionCity_Gym"){
            continue;
        }
        for (Pokemon::MapObject& object : map.objects){
            for (const Moved& trainer : MOVED){
                if (object.name == trainer.name){
                    object.x = trainer.tile.x;
                    object.y = trainer.tile.y;
                    object.trainer.reset();
                }
            }
        }
    }
    return moved;
}

std::string_view trash_can_end_name(TrashCanEnd end){
    switch (end){
    case TrashCanEnd::OPENED:   return "opened";
    case TrashCanEnd::GAVE_UP:  return "gave_up";
    case TrashCanEnd::FAILED:   return "failed";
    }
    return "?";
}


namespace{

//  The guess beside `first`: the next can in the search order, as the
//  player is likely to walk that way anyway, else the first beside it.
int guess_beside(int first){
    const std::vector<int> beside = trash_can_neighbors(first);
    const std::vector<int>& order = trash_can_order();
    const size_t at = std::find(order.begin(), order.end(), first) - order.begin();
    for (size_t c = 1; c < order.size(); c++){
        const int next = order[(at + c) % order.size()];
        if (std::find(beside.begin(), beside.end(), next) != beside.end()){
            return next;
        }
    }
    return beside[0];
}

} // namespace


TrashCanResult run_trash_cans(Logger& logger, const CheckCan& check, size_t max_checks){
    TrashCanResult result;
    const std::vector<int>& order = trash_can_order();
    size_t next = 0;
    std::optional<int> first;
    while (result.checks < max_checks){
        const int can = first ? guess_beside(*first) : order[next];
        const CanPages pages = check(can);
        result.checks++;
        result.last_can = can;
        if (pages == CanPages::FAILED){
            logger.log(std::format("Trash cans: checking can {} failed.", can), COLOR_RED);
            result.end = TrashCanEnd::FAILED;
            return result;
        }
        if (!first){
            if (pages == CanPages::TWO){
                logger.log(std::format("Trash cans: the first switch is under can {}; the second may be under {} of the cans beside it.", can, trash_can_neighbors(can).size()));
                first = can;
            }else{
                next = (next + 1) % order.size();
            }
            continue;
        }
        if (pages == CanPages::ONE){
            logger.log(std::format("Trash cans: the second switch is under can {}: the door is open, after {} checks and {} resets.", can, result.checks, result.resets));
            result.end = TrashCanEnd::OPENED;
            return result;
        }
        //  Reset: the new first switch is as likely under this can as any.
        result.resets++;
        logger.log(std::format("Trash cans: not under can {}; the locks reset ({}).", can, result.resets));
        first.reset();
        next = std::find(order.begin(), order.end(), can) - order.begin();
    }
    logger.log(std::format("Trash cans: the door is still shut after {} checks.", result.checks), COLOR_RED);
    result.end = TrashCanEnd::GAVE_UP;
    return result;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
