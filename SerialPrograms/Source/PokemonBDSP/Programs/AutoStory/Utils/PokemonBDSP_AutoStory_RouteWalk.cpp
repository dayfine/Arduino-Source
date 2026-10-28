/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

bool across(GridDirection direction){
    return direction == GridDirection::EAST || direction == GridDirection::WEST;
}

Milliseconds ms_per_tile(GridDirection direction){
    return across(direction) ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN;
}

//  The left stick for a direction. Row 0 is the top of a map, and the stick's
//  y is positive upward.
void stick_for(GridDirection direction, double& x, double& y){
    x = 0;
    y = 0;
    switch (direction){
    case GridDirection::NORTH:  y = 1; break;
    case GridDirection::EAST:   x = 1; break;
    case GridDirection::SOUTH:  y = -1; break;
    case GridDirection::WEST:   x = -1; break;
    }
}

std::string describe(const MapSet& maps, const std::vector<TilePos>& tiles){
    std::string out;
    for (const TilePos& pos : tiles){
        out += (out.empty() ? "" : ", ") + maps.describe(pos);
    }
    return out;
}

//  The tiles the player passes when walking `tiles` tiles from `from`, the
//  first one after `from` first. Stops early where the player is stopped.
std::vector<TilePos> leg_path(const TileGrid& grid, TilePos from, GridDirection direction, size_t tiles){
    std::vector<TilePos> path;
    size_t travelled = 0;
    while (travelled < tiles){
        const std::optional<TilePos> next = grid.step(from, direction);
        if (!next){
            break;
        }
        travelled += std::abs(next->x - from.x) + std::abs(next->y - from.y);
        from = *next;
        path.push_back(from);
    }
    return path;
}

//  Lower-case letters and digits only: "Route 202" and "ROUTE 2O2 " compare
//  by "route202".
std::string normalize_name(const std::string& text){
    std::string out;
    for (unsigned char ch : text){
        if (std::isalnum(ch)){
            out.push_back((char)std::tolower(ch));
        }
    }
    return out;
}

//  The location banner: a white box at the top left that names a map for a
//  few seconds after the player enters it. Left of the centred name it is plain
//  white (251 on console B, against 27-75 for the trees there otherwise).
const ImageFloatBox LOCATION_BANNER_WHITE{0.035, 0.035, 0.085, 0.050};
const ImageFloatBox LOCATION_BANNER_TEXT{0.100, 0.025, 0.300, 0.080};

//  The banner's map name, normalized, or empty when no banner shows.
std::string read_location_banner(const ImageViewRGB32& screen){
    const ImageStats white = image_stats(extract_box_reference(screen, LOCATION_BANNER_WHITE));
    if (white.average.sum() < 690 || white.stddev.sum() > 30){
        return "";
    }
    return normalize_name(OCR::tesseract_ocr_read(Language::English,
        extract_box_reference(screen, LOCATION_BANNER_TEXT)));
}

//  The map all of `tiles` are on, or empty if they are on several or none.
std::string map_of(const MapSet& maps, const std::vector<TilePos>& tiles){
    std::string name;
    for (const TilePos& pos : tiles){
        const std::optional<MapTile> tile = maps.to_local(pos);
        if (!tile || (!name.empty() && tile->map != name)){
            return "";
        }
        name = tile->map;
    }
    return name;
}

}


Milliseconds route_open_hold(GridDirection direction, size_t tiles){
    const Milliseconds step = ms_per_tile(direction);
    return step * tiles - step / 2;
}

void route_turn(VideoStream& stream, ProControllerContext& context, GridDirection direction){
    double x, y;
    stick_for(direction, x, y);
    stream.log(std::string("[Route] Turning ") + direction_name(direction) + ".");
    pbf_move_left_joystick(context, {x, y}, ROUTE_TURN_TAP, 300ms);
    context.wait_for_all_requests();
}


std::vector<TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context,
    const MapSet& maps,
    std::vector<TilePos> from,
    const std::function<bool(const TilePos&)>& goal,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
){
    //  Our own copy, so things found in the way can be added to it.
    TileGrid grid = maps.world();
    //  Which way the player faces, once a leg has set it.
    std::optional<GridDirection> facing;
    for (size_t plans = 0; plans < 8; plans++){
        const std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(grid, from, goal);
        if (!legs){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": no plan from " + describe(maps, from) + ".", stream);
        }
        std::string text = "[Route] " + label + ": plan from " + describe(maps, from) + ":";
        for (const PlannedLeg& leg : *legs){
            text += "\n    " + to_string(leg.leg) + " to " + describe(maps, leg.after);
        }
        stream.log(text);

        bool replanned = false;
        for (const PlannedLeg& planned : *legs){
            const TileLeg& leg = planned.leg;
            double x, y;
            stick_for(leg.direction, x, y);
            const std::string leg_label = label + ": " + to_string(leg);

            //  Note any location banner the walk passes, to check map changes.
            std::string banner;
            auto watch_banner = [&](const ImageViewRGB32& screen){
                if (banner.empty()){
                    banner = read_location_banner(screen);
                }
                return false;
            };

            WalkResult result;
            if (leg.ends_at_wall){
                //  Long enough to reach the wall from the farthest tile, with room
                //  for a slow start; past the wall the player stands still.
                const Milliseconds limit = ROUTE_START_DELAY + ms_per_tile(leg.direction) * leg.tiles * 5 / 4 + 1000ms;
                result = walk_until(stream, context, x, y, limit, leg_label, watch_banner);
                //  A wild battle may end the walk early. Walking to a wall again
                //  ends in the same place, so finish the leg.
                const bool interrupted = result.wild_battles > 0;
                for (size_t again = 0; again < 3 && result.wild_battles > 0 &&
                    (result.stop == WalkStop::LIMIT || result.stop == WalkStop::BLOCKED); again++
                ){
                    stream.log("[Route] " + leg_label + ": a wild battle on the way; walking to the wall again.");
                    result = walk_until(stream, context, x, y, limit, leg_label, watch_banner);
                }
                //  A wall well before the nearest one the map has: something the
                //  map does not know is in the way. Work out where we stopped from
                //  the time walked, put it on our map and plan again from there.
                size_t nearest = SIZE_MAX;
                for (const TilePos& pos : from){
                    nearest = std::min(nearest, walk_to_wall(grid, pos, leg.direction).tiles);
                }
                const Milliseconds moving = result.walked - 800ms - ROUTE_START_DELAY;
                const size_t walked_tiles = moving.count() <= 0 ? 0 : (size_t)(moving / ms_per_tile(leg.direction));
                if (!interrupted && result.stop == WalkStop::BLOCKED && walked_tiles + 3 < nearest){
                    std::vector<TilePos> stopped;
                    for (const TilePos& pos : from){
                        const std::vector<TilePos> path = leg_path(grid, pos, leg.direction, SIZE_MAX);
                        for (size_t k = walked_tiles == 0 ? 0 : walked_tiles - 1; k <= walked_tiles + 1 && k <= path.size(); k++){
                            stopped.push_back(k == 0 ? pos : path[k - 1]);
                        }
                    }
                    std::sort(stopped.begin(), stopped.end(), [](const TilePos& a, const TilePos& b){
                        return a.y != b.y ? a.y < b.y : a.x < b.x;
                    });
                    stopped.erase(std::unique(stopped.begin(), stopped.end()), stopped.end());
                    if (stopped.empty()){
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            leg_label + ": stopped early, and no tile fits.", stream);
                    }
                    if (stopped.size() == 1){
                        const TilePos blocker = stopped[0].next(leg.direction);
                        grid.set(blocker, TileType::BLOCKED);
                        stream.log("[Route] " + leg_label + ": something the map lacks is at " +
                            maps.describe(blocker) + "; marked it.", COLOR_ORANGE);
                    }
                    stream.log("[Route] " + leg_label + ": stopped early after about " + std::to_string(walked_tiles) +
                        " tiles, at " + describe(maps, stopped) + "; planning again.", COLOR_ORANGE);
                    facing = leg.direction;
                    from = stopped;
                    replanned = true;
                    break;
                }
            }else{
                //  The hold is timed for a player facing the way to go.
                if (facing != leg.direction){
                    route_turn(stream, context, leg.direction);
                }
                result = walk_for(stream, context, x, y, route_open_hold(leg.direction, leg.tiles), leg_label);
                if (result.wild_battles > 0){
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        leg_label + ": a wild battle cut an open leg short; position unknown.", stream);
                }
            }
            facing = leg.direction;
            if (banner.empty()){
                context.wait_for_all_requests();
                if (VideoSnapshot screen = stream.video().snapshot()){
                    watch_banner(*screen.frame);
                }
            }
            //  Entering another map shows its name; a missing or wrong name means
            //  the walk did not end where the plan says.
            const std::string map_before = map_of(maps, from);
            const std::string map_after = map_of(maps, planned.after);
            if (!map_after.empty() && map_after != map_before && banner != normalize_name(map_after)){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": expected to enter " + map_after + ", but the location banner read \"" + banner + "\".", stream);
            }
            if (!banner.empty()){
                stream.log("[Route] " + leg_label + ": location banner \"" + banner + "\".");
            }
            //  Where each leg ended, to compare with the plan after a run.
            if (VideoSnapshot screen = stream.video().snapshot()){
                const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
                Filesystem::create_directories(Filesystem::Path(folder));
                screen.frame->save(folder + now_to_filestring() + "-" + normalize_name(leg_label) + ".png");
            }
            if (result.stop == WalkStop::TRANSITION){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": unexpected transition.", stream);
            }

            //  A trainer who sees us freezes the player and walks over; the
            //  dialogue may open after the walk has ended.
            const std::vector<std::vector<TilePos>> paths = [&]{
                std::vector<std::vector<TilePos>> out;
                for (const TilePos& pos : from){
                    out.push_back(leg_path(grid, pos, leg.direction,
                        leg.ends_at_wall ? SIZE_MAX : leg.tiles + 1));
                }
                return out;
            }();
            std::vector<size_t> watching;
            for (size_t t = 0; t < trainers.size(); t++){
                const std::vector<TilePos> sight = sight_tiles(grid, trainers[t]);
                for (const std::vector<TilePos>& path : paths){
                    if (std::any_of(path.begin(), path.end(), [&](const TilePos& pos){
                        return std::find(sight.begin(), sight.end(), pos) != sight.end();
                    })){
                        watching.push_back(t);
                        break;
                    }
                }
            }
            bool talking = result.stop == WalkStop::DIALOG;
            if (!talking){
                ShortDialogWatcher dialog(COLOR_YELLOW);
                const Milliseconds wait = watching.empty() ? 500ms : 4000ms;
                talking = wait_until(stream, context, wait, {{dialog}}) == 0;
            }
            if (!talking){
                from = planned.after;
                continue;
            }

            if (watching.empty()){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": a dialogue opened where no known trainer can see.", stream);
            }
            //  Each possible start tile was stopped on the first tile of its path
            //  inside a watching trainer's sight.
            const TrainerSight trainer = trainers[watching[0]];
            std::vector<TilePos> stopped;
            for (const std::vector<TilePos>& path : paths){
                for (const TilePos& pos : path){
                    bool seen = false;
                    for (size_t t : watching){
                        const std::vector<TilePos> sight = sight_tiles(grid, trainers[t]);
                        seen |= std::find(sight.begin(), sight.end(), pos) != sight.end();
                    }
                    if (seen){
                        stopped.push_back(pos);
                        break;
                    }
                }
            }
            if (stopped.empty()){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": " + trainer.name + " talked, but no path enters their sight.", stream);
            }
            stream.log("[Route] " + label + ": " + trainer.name + " stopped us at " + describe(maps, stopped) + ".");
            fight(trainer.name);
            trainers.erase(trainers.begin() + watching[0]);
            from = stopped;
            replanned = true;
            break;
        }
        if (!replanned){
            stream.log("[Route] " + label + ": arrived at " + describe(maps, from) + ".");
            return from;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": too many plans.", stream);
}



}
}
}
