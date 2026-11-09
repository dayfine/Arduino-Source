/*  AutoStory Start Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
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

//  The thresholds, from console B's captures (Route 10 outside the
//  Center and on its grass, S.S. Anne's 1F corridor) and edits of them,
//  with 8x8 quarters and 24 below:
//  - the same frame with the player's 16x32 sprite copied onto one or two
//    other tiles, as a person who walked there: 1 to 4 tiles changed of
//    150; the river's pattern shifted 2 or 4 pixels, as its animation
//    does: 0 to 10;
//  - another place on the same route, or Route 10 against the S.S. Anne:
//    131 and 148;
//  - the view one step off: 20 to 78, depending on how plain the floor is;
//    six steps along the S.S. Anne's plain corridor: 36.
//  A quarter of the tiles (37) leaves room for a river and 6 people who
//  moved (at most 4 tiles each: where they stood and stand), and catches
//  another place and 5 of the 6 single steps off; a step along a plain
//  corridor can pass.

//  A tile has changed when its quarters differ by more than this, on
//  average per colour channel (0 to 255).
constexpr unsigned TILE_CHANGE = 24;
//  The same place when at most a quarter of the tiles changed.
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

} // namespace


ImageRGB32 start_frame(const ImageViewRGB32& game_screen){
    return game_screen.scale_to(FRAME_WIDTH, FRAME_HEIGHT);
}

StartFrameDifference compare_start_frames(const ImageViewRGB32& reference, const ImageViewRGB32& frame){
    const ImageRGB32 a = start_frame(reference);
    const ImageRGB32 b = start_frame(frame);
    StartFrameDifference difference;
    for (size_t ty = 0; ty < FRAME_HEIGHT / TILE; ty++){
        for (size_t tx = 0; tx < FRAME_WIDTH / TILE; tx++){
            difference.tiles++;
            if (tile_difference(a, b, tx, ty) > TILE_CHANGE){
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
