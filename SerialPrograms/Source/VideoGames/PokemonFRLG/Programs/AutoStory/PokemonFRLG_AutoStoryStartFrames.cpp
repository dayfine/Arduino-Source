/*  AutoStory Start Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <limits>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr size_t FRAME_WIDTH = 240;
constexpr size_t FRAME_HEIGHT = 160;
constexpr size_t TILE = 16;
//  A tile is compared as its four 8x8 quarters' average colours, so an
//  animated tile, whose pattern shifts a few pixels, keeps its averages.
constexpr size_t BLOCK = 8;

//  The tile's colour. A tile has changed when its quarters differ by more
//  than this, on average per colour channel (0 to 255); from console B's
//  captures (Route 10 outside the Center and on its grass, S.S. Anne's 1F
//  corridor) and edits of them, as people who walked and water animating.
constexpr unsigned TILE_CHANGE = 24;
//  ...unless its detail is the same (below): Pokemon Tower's fog
//  (WEATHER_FOG_HORIZONTAL) is a cloud texture blended over the whole
//  screen, and on 5F (10.05, 2026-11-12) it changed the colour of up to 78
//  tiles between the frame kept after the save and the same pose reloaded,
//  each by at most 38. The fog moves with the camera's offset since the map
//  loaded (pret field_weather_effect.c, sprite->y2 = gSpriteCoordOffsetY)
//  and scrolls with time, so a save loaded shows it elsewhere, here 16
//  pixels lower, whatever the wait. A colour change beyond the fog's
//  changes the tile alone.
constexpr unsigned TILE_COLOUR_CHANGE = 48;

//  The tile's detail: each pixel less the average of the 5x5 pixels around
//  it, which takes out the fog's smooth clouds and keeps the tiles' edges.
constexpr int DETAIL_RADIUS = 2;
//  Detail moved by up to this many pixels is the same detail: water and
//  flowers animate in shifts of a few pixels.
constexpr int DETAIL_SHIFT = 2;
//  A tile has changed when its detail differs by more than this, on
//  average per pixel and channel, or by more than DETAIL_SOME_CHANGE with
//  its colour beyond TILE_CHANGE. On the Tower 5F pair: 19 of 150 tiles;
//  one step off on Tower 3F to 5F, the Celadon Department Store and the
//  Lavender house: 41 to 76; on Route 10's grass: 49 to 51; along the
//  S.S. Anne's plain corridor: 36, and six steps: 43 (the colour alone:
//  20 and 36).
constexpr double DETAIL_CHANGE = 10;
constexpr double DETAIL_SOME_CHANGE = 8;

//  The same place when at most a quarter of the tiles changed: room for 6
//  people who moved (at most 4 tiles each: where they stood and stand).
constexpr size_t MOST_CHANGED_PERCENT = 25;

//  The block at (`bx`, `by`): its channels' totals in `a` less those in
//  `b`, summed as absolute values.
uint64_t block_difference(const ImageViewRGB32& a, const ImageViewRGB32& b, size_t bx, size_t by){
    int64_t sums[3] = {0, 0, 0};
    for (size_t y = by; y < by + BLOCK; y++){
        for (size_t x = bx; x < bx + BLOCK; x++){
            const uint32_t p = a.pixel(x, y);
            const uint32_t q = b.pixel(x, y);
            for (int c = 0; c < 3; c++){
                sums[c] += (int64_t)((p >> (8 * c)) & 0xff) - (int64_t)((q >> (8 * c)) & 0xff);
            }
        }
    }
    return (uint64_t)(std::llabs(sums[0]) + std::llabs(sums[1]) + std::llabs(sums[2]));
}

//  The tile at (`tx`, `ty`)'s quarters' average difference per channel.
unsigned tile_difference(const ImageViewRGB32& a, const ImageViewRGB32& b, size_t tx, size_t ty){
    uint64_t total = 0;
    for (size_t by = ty * TILE; by < (ty + 1) * TILE; by += BLOCK){
        for (size_t bx = tx * TILE; bx < (tx + 1) * TILE; bx += BLOCK){
            total += block_difference(a, b, bx, by);
        }
    }
    return (unsigned)(total / (TILE * TILE * 3));
}

//  A start frame's detail: per pixel and channel, the value less the
//  average around it (DETAIL_RADIUS, the edges repeated).
class FrameDetail{
public:
    explicit FrameDetail(const ImageViewRGB32& frame)
        : m_values(FRAME_WIDTH * FRAME_HEIGHT * 3)
    {
        constexpr int SIDE = 2 * DETAIL_RADIUS + 1;
        for (int y = 0; y < (int)FRAME_HEIGHT; y++){
            for (int x = 0; x < (int)FRAME_WIDTH; x++){
                int sums[3] = {0, 0, 0};
                for (int dy = -DETAIL_RADIUS; dy <= DETAIL_RADIUS; dy++){
                    for (int dx = -DETAIL_RADIUS; dx <= DETAIL_RADIUS; dx++){
                        add_channels(sums, frame.pixel(clamped(x + dx, FRAME_WIDTH), clamped(y + dy, FRAME_HEIGHT)));
                    }
                }
                const uint32_t pixel = frame.pixel(x, y);
                for (int c = 0; c < 3; c++){
                    m_values[index(x, y, c)] = (int)((pixel >> (8 * c)) & 0xff) * SIDE * SIDE - sums[c];
                }
            }
        }
    }

    //  The value at (`x`, `y`), channel `c`, times the averaged area.
    int value(int x, int y, int c) const{
        return m_values[index(x, y, c)];
    }

private:
    static size_t clamped(int v, size_t size){
        return (size_t)std::clamp(v, 0, (int)size - 1);
    }
    static void add_channels(int sums[3], uint32_t pixel){
        for (int c = 0; c < 3; c++){
            sums[c] += (int)((pixel >> (8 * c)) & 0xff);
        }
    }
    static size_t index(int x, int y, int c){
        return ((size_t)y * FRAME_WIDTH + (size_t)x) * 3 + (size_t)c;
    }

    std::vector<int> m_values;
};

//  The tile at (`tx`, `ty`) of `a`'s detail against `b`'s moved by
//  (`sx`, `sy`): the average difference per pixel and channel, over the
//  pixels inside `b`.
double shifted_detail_difference(const FrameDetail& a, const FrameDetail& b, size_t tx, size_t ty, int sx, int sy){
    constexpr int AREA = (2 * DETAIL_RADIUS + 1) * (2 * DETAIL_RADIUS + 1);
    int64_t total = 0;
    int64_t count = 0;
    for (int y = (int)(ty * TILE); y < (int)((ty + 1) * TILE); y++){
        for (int x = (int)(tx * TILE); x < (int)((tx + 1) * TILE); x++){
            const int bx = x + sx;
            const int by = y + sy;
            if (bx < 0 || by < 0 || bx >= (int)FRAME_WIDTH || by >= (int)FRAME_HEIGHT){
                continue;
            }
            for (int c = 0; c < 3; c++){
                total += std::abs(a.value(x, y, c) - b.value(bx, by, c));
            }
            count += 3;
        }
    }
    return (double)total / (double)(count * AREA);
}

//  The tile at (`tx`, `ty`)'s detail difference, the closest of the shifts
//  up to DETAIL_SHIFT.
double detail_difference(const FrameDetail& a, const FrameDetail& b, size_t tx, size_t ty){
    double best = std::numeric_limits<double>::max();
    for (int sy = -DETAIL_SHIFT; sy <= DETAIL_SHIFT; sy++){
        for (int sx = -DETAIL_SHIFT; sx <= DETAIL_SHIFT; sx++){
            best = std::min(best, shifted_detail_difference(a, b, tx, ty, sx, sy));
        }
    }
    return best;
}

bool tile_changed(unsigned colour, double detail){
    return detail > DETAIL_CHANGE
        || colour > TILE_COLOUR_CHANGE
        || (colour > TILE_CHANGE && detail > DETAIL_SOME_CHANGE);
}

} // namespace


ImageRGB32 start_frame(const ImageViewRGB32& game_screen){
    return game_screen.scale_to(FRAME_WIDTH, FRAME_HEIGHT);
}

StartFrameDifference compare_start_frames(const ImageViewRGB32& reference, const ImageViewRGB32& frame){
    const ImageRGB32 a = start_frame(reference);
    const ImageRGB32 b = start_frame(frame);
    const FrameDetail a_detail(a);
    const FrameDetail b_detail(b);
    StartFrameDifference difference;
    for (size_t ty = 0; ty < FRAME_HEIGHT / TILE; ty++){
        for (size_t tx = 0; tx < FRAME_WIDTH / TILE; tx++){
            difference.tiles++;
            if (tile_changed(tile_difference(a, b, tx, ty), detail_difference(a_detail, b_detail, tx, ty))){
                difference.changed++;
            }
        }
    }
    return difference;
}

bool same_start_place(const StartFrameDifference& difference){
    return difference.tiles > 0 && difference.changed * 100 <= difference.tiles * MOST_CHANGED_PERCENT;
}

std::string to_string(const StartFrameDifference& difference){
    return std::format("{} of {} tiles changed", difference.changed, difference.tiles);
}


StartRead read_start_frames(const std::optional<ImageRGB32>& reference, const StartFrameSource& next_frame, size_t tries){
    if (!reference){
        return {StartFound::NOT_READ, "no start frame kept for this checkpoint"};
    }
    std::string seen;
    for (size_t attempt = 0; attempt < tries; attempt++){
        const ImageRGB32 frame = next_frame();
        if (!frame){
            return {StartFound::NOT_READ, "no video"};
        }
        const StartFrameDifference difference = compare_start_frames(*reference, frame);
        if (same_start_place(difference)){
            return {StartFound::AT_START, "the start frame matches: " + to_string(difference)};
        }
        seen += (seen.empty() ? "" : ", ") + to_string(difference);
    }
    return {StartFound::ELSEWHERE, "the screen isn't the checkpoint's start frame: " + seen};
}


StartFrameStore::StartFrameStore(std::string folder)
    : m_folder(std::move(folder))
{}

bool StartFrameStore::save(const std::string& checkpoint, const ImageViewRGB32& frame, const std::string& source) const{
    if (m_folder.empty()){
        return false;
    }
    Filesystem::create_directories(m_folder);
    if (!frame.save(m_folder + checkpoint + ".png")){
        return false;
    }
    JsonObject json;
    json["checkpoint"] = checkpoint;
    json["kept"] = now_to_filestring();
    json["source"] = source;
    JsonValue(std::move(json)).dump(m_folder + checkpoint + ".json");
    return true;
}

std::optional<ImageRGB32> StartFrameStore::load(const std::string& checkpoint) const{
    const std::string path = m_folder + checkpoint + ".png";
    if (m_folder.empty() || !Filesystem::exists(path)){
        return std::nullopt;
    }
    ImageRGB32 frame(path);
    if (!frame){
        return std::nullopt;
    }
    return frame;
}

StartFrameStore program_start_frames(){
    return StartFrameStore(USER_FILE_PATH() + "PokemonFRLG/AutoStoryStartFrames/");
}


ImageRGB32 console_start_frame(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    const VideoSnapshot snapshot = console.video().snapshot();
    if (!snapshot){
        return ImageRGB32();
    }
    return start_frame(extract_box_reference(snapshot, GameSettings::instance().GAME_BOX));
}

StartRead check_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
){
    size_t frames = 0;
    return read_start_frames(store.load(checkpoint), [&]{
        if (frames++ > 0){
            context.wait_for(CONSOLE_START_FRAME_WAIT);
        }
        return console_start_frame(console, context);
    }, CONSOLE_START_FRAME_TRIES);
}

bool keep_console_frame(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store,
    const std::string& checkpoint, const std::string& source
){
    const ImageRGB32 frame = console_start_frame(console, context);
    return frame && store.save(checkpoint, frame, source);
}

StartRead keep_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
){
    if (!keep_console_frame(console, context, store, checkpoint, "kept by the program's option from the save loaded")){
        return {StartFound::NOT_READ, "couldn't keep the screen as " + checkpoint + "'s start frame"};
    }
    return {StartFound::AT_START, "kept the screen as " + checkpoint + "'s start frame"};
}


void add_start_check(
    Pokemon::Checkpoint& checkpoint, const std::optional<Pokemon::Pose>& start, StartReader read,
    std::function<void(const std::string& line)> log
){
    if (!start){
        return;
    }
    checkpoint.start.push_back(Pokemon::condition("save_at_start_pose",
        [read = std::move(read), log = std::move(log)](Pokemon::RunState&){
            const StartRead found = read();
            switch (found.found){
            case StartFound::NOT_READ:
                log("start pose not read: " + (found.detail.empty() ? "" : found.detail + "; ")
                    + "going on from the save as the checkpoint's start.");
                return true;
            case StartFound::AT_START:
                if (!found.detail.empty()){
                    log("start pose matches: " + found.detail);
                }
                return true;
            case StartFound::ELSEWHERE:
                log("start pose mismatch: " + found.detail);
                return false;
            }
            return false;
        }
    ));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
