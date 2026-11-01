/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <optional>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_PokemonCenter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

//  How many tiles the 1080p view spans across: a tile is about 125 px.
constexpr double SCREEN_WIDTH_TILES = 1920.0 / 125;
//  The camera keeps the player in the middle of the screen across, and the
//  centre of his tile a little above the middle down (r121a and r122a: his
//  feet at y = 590 of 1080).
constexpr double PLAYER_X = 0.5;
constexpr double PLAYER_Y = 0.505;
//  How much of the screen's height one row of tiles takes near the player:
//  the doorway straight ahead of him (r121a) is centred this far above his
//  tile's centre. Rows further north look shorter, so this undercounts them.
constexpr double SCREEN_ROW_HEIGHT = 0.12;
//  Doorways are looked for down to here, 0 to 1: a door the player can walk
//  into is north of him.
constexpr double DOORWAY_SEARCH_BOTTOM = 0.67;
//  A doorway found within this many rows of straight ahead is at the front
//  row; further, the walk to the door ended short.
constexpr double DOORWAY_FRONT_ROWS = 1.5;

//  The doorway is looked for on a view this size: 4 px of 1080p per pixel.
constexpr size_t SMALL_WIDTH = 480;
constexpr size_t SMALL_HEIGHT = 270;
//  A doorway's lit columns run at least this many small rows (the Mart's 12,
//  the Center's 15); the buildings' lit roof bands 3 to 5.
constexpr size_t DOORWAY_COLUMN_ROWS = 8;
//  A doorway has this many lit small pixels (the Mart's 260 to 350); a roof
//  band's run 60 to 75.
constexpr double DOORWAY_MIN_MASS = 80;

//  Whether a pixel is the pale yellow-white of a lit doorway: the Mart's is
//  (235, 243, 205), the Center's (255, 255, 185). The buildings' white roof
//  bands are bluer, their orange bands and the light they throw on the
//  ground outside are darker.
bool lit_doorway_pixel(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    return r >= 215 && g >= 215 && b >= 150 && b <= g - 15 && r >= g - 25;
}

//  Each column of the small view: how many lit doorway pixels it has in the
//  search band, and the sum of their rows.
struct LitColumns{
    std::vector<size_t> count;
    std::vector<double> row_sum;
};
LitColumns lit_columns(const ImageViewRGB32& screen){
    const ImageRGB32 small = screen.scale_to(SMALL_WIDTH, SMALL_HEIGHT);
    LitColumns columns{std::vector<size_t>(small.width(), 0), std::vector<double>(small.width(), 0)};
    const size_t bottom = (size_t)(DOORWAY_SEARCH_BOTTOM * (double)small.height());
    for (size_t y = 0; y < bottom; y++){
        for (size_t x = 0; x < small.width(); x++){
            if (lit_doorway_pixel(small.pixel(x, y))){
                columns.count[x]++;
                columns.row_sum[x] += (double)y;
            }
        }
    }
    return columns;
}

//  A run of lit columns: its lit pixels, and the sums of their columns and rows.
struct LitRun{
    double mass = 0;
    double column_sum = 0;
    double row_sum = 0;
};
//  The run of columns with at least DOORWAY_COLUMN_ROWS lit that starts at
//  `x`, allowing gaps of up to 3 columns (the door's frame). Moves `x` past it.
LitRun lit_run(const LitColumns& columns, size_t& x){
    LitRun run;
    size_t gap = 0;
    for (; x < columns.count.size() && gap <= 3; x++){
        if (columns.count[x] < DOORWAY_COLUMN_ROWS){
            gap++;
            continue;
        }
        run.mass += (double)columns.count[x];
        run.column_sum += (double)columns.count[x] * (double)x;
        run.row_sum += columns.row_sum[x];
        gap = 0;
    }
    return run;
}

//  Where a building's lit doorway is across and down the screen, 0 to 1: the
//  biggest run of lit columns within 0.35 of the middle (a second building's
//  door may show further off). Nothing if no doorway shows.
std::optional<std::pair<double, double>> lit_doorway_position(const ImageViewRGB32& screen){
    const LitColumns columns = lit_columns(screen);
    const double width = (double)columns.count.size();
    std::optional<std::pair<double, double>> best;
    double best_mass = 0;
    for (size_t x = 0; x < columns.count.size(); ){
        if (columns.count[x] < DOORWAY_COLUMN_ROWS){
            x++;
            continue;
        }
        const LitRun run = lit_run(columns, x);
        const double center = run.column_sum / run.mass / width;
        if (run.mass < DOORWAY_MIN_MASS || std::abs(center - PLAYER_X) >= 0.35 || run.mass <= best_mass){
            continue;
        }
        best = {center, run.row_sum / run.mass / (double)SMALL_HEIGHT};
        best_mass = run.mass;
    }
    return best;
}

//  Whether the whole view is a different scene from "before".
bool scene_changed_since(VideoStream& stream, ProControllerContext& context, const VideoSnapshot& before){
    context.wait_for_all_requests();
    VideoSnapshot now = stream.video().snapshot();
    return before && now && walk_screen_change(*before.frame, *now.frame) > WALK_SCENE_CHANGE;
}

//  Moves across by "tiles", east if positive, west if negative.
void step_across(VideoStream& stream, ProControllerContext& context, double tiles){
    const Direction side = tiles > 0 ? Direction::EAST : Direction::WEST;
    route_turn(stream, context, side);
    const Milliseconds hold = std::max<Milliseconds>(Milliseconds(40),
        Milliseconds((int64_t)(std::abs(tiles) * ROUTE_MS_PER_TILE_ACROSS.count())));
    pbf_move_left_joystick(context, {tiles > 0 ? 1.0 : -1.0, 0}, hold, 300ms);
}

//  Whether the screen now shows a Pokemon Center's inside.
bool center_inside_now(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot now = stream.video().snapshot();
    return now && pokemon_center_inside(*now.frame);
}

//  Walks north into the door straight ahead, `doorway` away. Returns whether
//  we are inside.
bool walk_into_door(
    VideoStream& stream, ProControllerContext& context,
    const VideoSnapshot& outside, const LitDoorway& doorway, const std::string& label
){
    //  Time for the rows to the door (none when it is straight ahead), the
    //  door opening, and a start delay.
    const double rows = std::max(0.0, doorway.rows_up - 1);
    const Milliseconds limit = 1500ms + Milliseconds((int64_t)(rows * ROUTE_MS_PER_TILE_UP_DOWN.count()));
    //  The Center's fade is too light to read as black (r126 runs 2 and 3: a
    //  screen change of 49 at 400 ms), so its inside, once it shows, stops
    //  the walk too.
    const WalkStop stop = walk_until(stream, context, 0, 1, limit, label + ": into the door",
        [](const ImageViewRGB32& screen){ return pokemon_center_inside(screen); }).stop;
    if (stop == WalkStop::TRANSITION || stop == WalkStop::ARRIVED){
        pbf_wait(context, 2000ms);
        return true;
    }
    //  The door slides open before it takes us in, and its fade is not always
    //  black enough to read as a transition, and may finish after the walk; a
    //  whole new scene says we are in. Not after a walk of several rows, which
    //  scrolls the view: the next look decides.
    pbf_wait(context, 2000ms);
    if (center_inside_now(stream, context)){
        stream.log("[Route] " + label + ": a Pokemon Center's inside shows; inside.");
        return true;
    }
    if (doorway.rows_up > DOORWAY_FRONT_ROWS || !scene_changed_since(stream, context, outside)){
        return false;
    }
    stream.log("[Route] " + label + ": the scene changed; inside.");
    return true;
}

//  Where the doorway is on screen, in the log.
void log_doorway(VideoStream& stream, const std::string& label, const LitDoorway& doorway){
    stream.log("[Route] " + label + ": doorway " + std::to_string(doorway.tiles_across) +
        " tiles across, " + std::to_string(doorway.rows_up) + " rows up (at " +
        std::to_string(doorway.x) + ", " + std::to_string(doorway.y) + " of the screen).");
}

}


bool pokemon_center_inside(const ImageViewRGB32& screen){
    //  The orange back wall along the top, in two of three places: a plant
    //  or a bookcase can stand in one (Jubilife's top left), and at night
    //  it is darker (r 160..200 in Jubilife, 254 in Sandgem by day).
    size_t wall = 0;
    for (double x : {0.02, 0.25, 0.65}){
        const ImageStats box = image_stats(extract_box_reference(screen, ImageFloatBox(x, 0.03, 0.08, 0.06)));
        const double r = box.average.r, g = box.average.g, b = box.average.b;
        wall += r >= 140 && r > g + 40 && g > b + 40;
    }
    if (wall < 2){
        return false;
    }
    //  Black below the floor, either side of the step out of the door.
    for (const ImageFloatBox& box : {ImageFloatBox(0.02, 0.95, 0.30, 0.04), ImageFloatBox(0.65, 0.95, 0.33, 0.04)}){
        if (image_stats(extract_box_reference(screen, box)).average.sum() > 90){
            return false;
        }
    }
    //  And the floor lit between them: not a fade (349 in Jubilife at night).
    return image_stats(extract_box_reference(screen, ImageFloatBox(0.30, 0.55, 0.40, 0.15))).average.sum() > 250;
}

namespace{

//  Near-black pixels in one edge strip of the screen, out of all its pixels.
struct EdgeDark{
    size_t dark = 0;
    size_t count = 0;
};
void add_pixel(EdgeDark& edge, bool dark){
    edge.dark += dark;
    edge.count++;
}
bool mostly_dark(const EdgeDark& edge){
    return edge.count > 0 && edge.dark * 100 >= edge.count * 15;
}

}

bool indoors(const ImageViewRGB32& screen){
    const size_t width = screen.width(), height = screen.height();
    const size_t strip_h = height * 12 / 100, strip_w = width * 12 / 100;
    size_t warm = 0;
    size_t count = 0;
    EdgeDark top, bottom, left, right;
    for (size_t y = 0; y < height; y += 8){
        for (size_t x = 0; x < width; x += 8){
            const Color pixel(screen.pixel(x, y));
            count++;
            warm += pixel.red() > 150 && pixel.red() > pixel.blue() + 70 && pixel.green() > pixel.blue() + 20;
            const bool dark = pixel.red() + pixel.green() + pixel.blue() < 60;
            if (y < strip_h){ add_pixel(top, dark); }
            if (y >= height - strip_h){ add_pixel(bottom, dark); }
            if (x < strip_w){ add_pixel(left, dark); }
            if (x >= width - strip_w){ add_pixel(right, dark); }
        }
    }
    if (count == 0 || warm * 4 <= count){
        return false;
    }
    return mostly_dark(top) || mostly_dark(bottom) || mostly_dark(left) || mostly_dark(right);
}

std::optional<double> find_lit_lamp_x(const ImageViewRGB32& screen, const ImageFloatBox& box){
    const size_t x0 = (size_t)(box.x * screen.width()), x1 = (size_t)((box.x + box.width) * screen.width());
    const size_t y0 = (size_t)(box.y * screen.height()), y1 = (size_t)((box.y + box.height) * screen.height());
    double sum = 0;
    size_t count = 0;
    for (size_t y = y0; y < y1 && y < screen.height(); y++){
        for (size_t x = x0; x < x1 && x < screen.width(); x++){
            const Color pixel(screen.pixel(x, y));
            if (pixel.red() > 235 && pixel.green() > 220 && pixel.blue() > 150){
                sum += x;
                count++;
            }
        }
    }
    //  Jubilife's lamps at night: 50 to 100 such pixels at 480 px across.
    if (count * 480 * 480 < 30 * screen.width() * screen.width()){
        return std::nullopt;
    }
    return sum / count / screen.width();
}

double tiles_across_at(double screen_x){
    return (screen_x - PLAYER_X) * SCREEN_WIDTH_TILES;
}

std::optional<LitDoorway> find_lit_doorway(const ImageViewRGB32& screen){
    //  A Center's lit floor has the doorway's colour.
    if (pokemon_center_inside(screen)){
        return std::nullopt;
    }
    const std::optional<std::pair<double, double>> position = lit_doorway_position(screen);
    if (!position){
        return std::nullopt;
    }
    return LitDoorway{
        position->first,
        position->second,
        (position->first - PLAYER_X) * SCREEN_WIDTH_TILES,
        (PLAYER_Y - position->second) / SCREEN_ROW_HEIGHT,
    };
}

std::vector<TilePos> tiles_short_of_door(
    VideoStream& stream, ProControllerContext& context,
    const TileGrid& grid, const TilePos& door, const std::string& label
){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    const std::optional<LitDoorway> doorway = screen ? find_lit_doorway(*screen.frame) : std::nullopt;
    if (!doorway){
        stream.log("[Route] " + label + ": no lit doorway on screen; not walking again.");
        return {};
    }
    log_doorway(stream, label, *doorway);
    if (doorway->rows_up <= DOORWAY_FRONT_ROWS){
        return {};
    }
    return tiles_by_lit_doorway(*doorway, grid, door);
}

std::vector<TilePos> tiles_by_lit_doorway(const LitDoorway& doorway, const TileGrid& grid, const TilePos& door){
    const TilePos guess{door.x - (int)std::lround(doorway.tiles_across), door.y + (int)std::lround(doorway.rows_up)};
    std::vector<TilePos> tiles;
    for (int y = guess.y - 1; y <= guess.y + 1; y++){
        for (int x = guess.x - 1; x <= guess.x + 1; x++){
            if (grid.standable(TilePos{x, y})){
                tiles.push_back(TilePos{x, y});
            }
        }
    }
    return tiles;
}

bool line_up_with_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
){
    route_status(stream, label + ": lining up with the door.");
    for (size_t tries = 0; tries < 6; tries++){
        context.wait_for_all_requests();
        VideoSnapshot screen = stream.video().snapshot();
        if (screen && pokemon_center_inside(*screen.frame)){
            stream.log("[Route] " + label + ": a Pokemon Center's inside shows; not in front of the door.");
            return false;
        }
        const std::optional<LitDoorway> doorway = screen ? find_lit_doorway(*screen.frame) : std::nullopt;
        if (!doorway){
            stream.log("[Route] " + label + ": no lit doorway on screen.");
            return false;
        }
        log_doorway(stream, label, *doorway);
        //  Steps overshoot: within a quarter tile, r128 measured 0.29, -0.28,
        //  0.48, -0.67 and 0.48 tiles in turn and never settled. The door
        //  still takes us in from 0.4 (enter_lit_door()).
        if (std::abs(doorway->tiles_across) <= 0.4){
            return true;
        }
        step_across(stream, context, doorway->tiles_across);
        //  Let a door that took us in show its inside before looking again.
        pbf_wait(context, 1000ms);
    }
    stream.log("[Route] " + label + ": not lined up after 6 steps.");
    return false;
}

void enter_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
){
    //  Movement is not locked to tiles, and the door takes us in only when we
    //  are lined up with it. Its doorway is lit: find it on screen, where the
    //  camera keeps the player in the middle, move across by its offset, and
    //  walk in once it is straight ahead, looking again after every move.
    route_status(stream, label + ": lining up with the door.");
    VideoSnapshot last_outside;
    for (size_t tries = 0; tries < 12; tries++){
        context.wait_for_all_requests();
        VideoSnapshot outside = stream.video().snapshot();
        if (!outside){
            continue;
        }
        //  Inside a Center, its lit floor would read as another doorway.
        if (pokemon_center_inside(*outside.frame)){
            stream.log("[Route] " + label + ": a Pokemon Center's inside shows; inside.");
            return;
        }
        const std::optional<LitDoorway> doorway = find_lit_doorway(*outside.frame);
        if (!doorway){
            //  The door may have taken us in after all, more slowly than we watched.
            pbf_wait(context, 1500ms);
            if (!scene_changed_since(stream, context, last_outside)){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    label + ": no lit doorway on screen.", stream);
            }
            stream.log("[Route] " + label + ": the scene changed; inside.");
            pbf_wait(context, 1500ms);
            return;
        }
        last_outside = outside;
        log_doorway(stream, label, *doorway);
        //  We stand on tiles, so within half a tile is the door's column;
        //  a step moves about 0.7 of a reading, and at 0.25 r144a stepped
        //  between 0.3 and -0.4 twelve times. Try the door there, and step
        //  toward it if it does not take us.
        if (std::abs(doorway->tiles_across) > 0.5){
            step_across(stream, context, doorway->tiles_across);
            continue;
        }
        if (walk_into_door(stream, context, outside, *doorway, label)){
            pbf_wait(context, 1500ms);
            return;
        }
        step_across(stream, context, doorway->tiles_across);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": not through the door after 12 tries.", stream);
}

void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
){
    enter_lit_door(stream, context, label);

    //  The nurse's counter is straight ahead of the door.
    route_status(stream, label + ": talking to the nurse.");
    if (walk_to_talk(stream, context, 0, 1, 6000ms, label + ": to the nurse").stop != WalkStop::DIALOG){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": nurse not reached.", stream);
    }
    //  A through her lines; the cursor starts on "Yes". Stop once no dialogue
    //  has shown for a while, so A does not start the talk over.
    size_t quiet = 0;
    for (size_t presses = 0; presses < 30 && quiet < 2; presses++){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            quiet = 0;
            pbf_press_button(context, BUTTON_A, 80ms, 700ms);
        }else{
            quiet++;
        }
    }
    context.wait_for_all_requests();

    route_status(stream, label + ": healed; walking out.");
    leave_door_south(stream, context, label + ": out the door");
}



void move_across(VideoStream& stream, ProControllerContext& context, double tiles){
    step_across(stream, context, tiles);
}


}
}
}
