/*  Overworld Recovery Frames Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Frame- and image-based tap, water-landing, and interrupt-frame tests
 *  split from PokemonFRLG_OverworldRecovery_Tests.cpp.
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "PokemonFRLG_OverworldRecoveryFrames_Tests.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"
#include "PokemonFRLG_OverworldWalkWatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Console B frames (1920x1080) in CommandLineTests, 12.03 on Route 21,
//  2026-11-13: a tap south against a rock 400 ms apart, the player blocked;
//  and the player landed on Cinnabar's beach behind the roof of the
//  building below, only the head showing.
constexpr std::string_view ROCK_TAP_BEFORE = "PokemonFRLG/OverworldWater/Route21_RockTap_Before.png";
constexpr std::string_view ROCK_TAP_AFTER = "PokemonFRLG/OverworldWater/Route21_RockTap_After.png";
constexpr std::string_view BEACH_BEHIND_ROOF = "PokemonFRLG/OverworldWater/Cinnabar_BeachBehindRoof.png";
const ImageFloatBox CONSOLE_GAME_BOX(0.09375, 0.00462963, 0.8125, 0.962963);

std::optional<ImageRGB32> unit_test_frame(std::string_view name){
    try{
        return ImageRGB32(UNIT_TEST_RESOURCE_PATH() + std::string(name));
    }catch (FileException&){
        return std::nullopt;
    }
}

//  `screen` with the map in `game` moved as a step south moves it: up a tile.
ImageRGB32 stepped_south(const ImageRGB32& screen, const ImageFloatBox& game){
    ImageRGB32 out = screen.copy();
    const size_t top = (size_t)(screen.height() * game.y + 0.5);
    const size_t bottom = top + (size_t)(screen.height() * game.height + 0.5);
    const size_t tile = (size_t)(screen.height() * game.height / 10 + 0.5);
    for (size_t y = top; y + tile < bottom; y++){
        for (size_t x = 0; x < screen.width(); x++){
            out.pixel(x, y) = screen.pixel(x, y + tile);
        }
    }
    return out;
}

uint32_t synthetic_tile_color(size_t tx, size_t ty){
    const uint32_t r = (uint32_t)(50 + ((tx * 37 + ty * 17) % 160));
    const uint32_t g = (uint32_t)(60 + ((tx * 23 + ty * 41) % 150));
    const uint32_t b = (uint32_t)(40 + ((tx * 53 + ty * 29) % 170));
    return 0xff000000 | (r << 16) | (g << 8) | b;
}

ImageRGB32 draw_animated_edge_scene(size_t tile, size_t origin_x, size_t npc_tx, size_t npc_ty){
    const size_t width = 15 * tile;
    const size_t height = 10 * tile;
    ImageRGB32 img(width, height);
    for (size_t y = 0; y < height; y++){
        for (size_t x = 0; x < width; x++){
            const size_t tx = x / tile + origin_x;
            const size_t ty = y / tile;
            uint32_t color = synthetic_tile_color(tx, ty);
            if (x / tile == npc_tx && y / tile == npc_ty){
                color = 0xffff2020;
            }
            //  Player sprite fixed at center tile (7, 4).
            if (x / tile == 7 && y / tile == 4){
                color = 0xff2040ff;
            }
            img.pixel(x, y) = color;
        }
    }
    return img;
}

//  A tap against a rock on the water (console B, 2026-11-13 08:53, 12.03):
//  the water's animation changed the edge bands by 35.2, read as a step.
//  It must read as no move; the same frame moved a tile still reads as one.
UnitTestResult test_water_tap_against_rock(){
    const std::optional<ImageRGB32> before = unit_test_frame(ROCK_TAP_BEFORE);
    const std::optional<ImageRGB32> after = unit_test_frame(ROCK_TAP_AFTER);
    if (!before || !after){
        return UnitTestResult(UnitTestResult::SKIPPED, "Frames not in CommandLineTests: " + std::string(ROCK_TAP_BEFORE));
    }
    if (tap_moved_on_water(*before, *after, CONSOLE_GAME_BOX, Direction::SOUTH)){
        return std::format("a tap against the rock read as a step (bands changed by {:.1f})", tap_scroll_change(*before, *after, CONSOLE_GAME_BOX));
    }
    if (!tap_moved_on_water(*before, stepped_south(*after, CONSOLE_GAME_BOX), CONSOLE_GAME_BOX, Direction::SOUTH)){
        return "a step south on the water read as no move";
    }
    return true;
}

//  A blocked tap on land while wandering NPCs or animated tiles change an
//  edge band by more than WALL_TAP_MOVED_RMSD (Cerulean City west 13 across
//  little_boy: bottom/right bands changed by 30-40 while standing still).
//  tap_step_moved() must reject the blocked tap and still accept a 1-tile
//  west scroll even with the same animated edge band.
UnitTestResult test_tap_blocked_with_animated_edge(){
    const ImageFloatBox game(0, 0, 1, 1);
    const ImageRGB32 before = draw_animated_edge_scene(/*tile=*/16, /*origin_x=*/10, /*npc_tx=*/12, /*npc_ty=*/8);
    //  Blocked tap: origin unchanged, NPC moved in bottom-right edge band.
    const ImageRGB32 blocked = draw_animated_edge_scene(/*tile=*/16, /*origin_x=*/10, /*npc_tx=*/13, /*npc_ty=*/8);
    if (tap_scroll_change(before, blocked, game) <= WALL_TAP_MOVED_RMSD){
        return std::format("Expected animated edge band to exceed WALL_TAP_MOVED_RMSD, got {:.1f}.", tap_scroll_change(before, blocked, game));
    }
    if (tap_step_moved(before, blocked, game, Direction::WEST)){
        return std::format("Blocked tap with animated edge band read as a step (change {:.1f}).", tap_scroll_change(before, blocked, game));
    }
    //  Real 1-tile step west: origin moves west (10 -> 9), NPC also moves.
    const ImageRGB32 stepped = draw_animated_edge_scene(/*tile=*/16, /*origin_x=*/9, /*npc_tx=*/13, /*npc_ty=*/8);
    if (!tap_step_moved(before, stepped, game, Direction::WEST)){
        return "Real 1-tile step west with animated edge band read as no move.";
    }
    return true;
}

//  Landed behind a roof (console B, 2026-11-13 09:46, 12.03 land_beach):
//  the walking sprite hides, but no water shows on the player's tile, as it
//  does around a surfing player.
UnitTestResult test_landed_behind_roof(){
    const std::optional<ImageRGB32> beach = unit_test_frame(BEACH_BEHIND_ROOF);
    const std::optional<ImageRGB32> surfing = unit_test_frame(ROCK_TAP_BEFORE);
    if (!beach || !surfing){
        return UnitTestResult(UnitTestResult::SKIPPED, "Frames not in CommandLineTests: " + std::string(BEACH_BEHIND_ROOF));
    }
    if (surfing_on_screen(*beach, CONSOLE_GAME_BOX)){
        return "landed behind a roof read as surfing";
    }
    if (!surfing_on_screen(*surfing, CONSOLE_GAME_BOX)){
        return "surfing read as landed";
    }
    return true;
}

//  Console B, 2026-11-13 08:53 (12.03): "a message after 2152ms" stopped
//  "south 35 (to wall)" on Route 21 and left no frame to check it by. The
//  frame a leg's interrupt shows is saved, and an empty one isn't.
UnitTestResult test_interrupt_frame(){
    Logger& logger = global_logger_tagged();
    const std::string path = save_leg_interrupt_frame(logger, ImageRGB32(64, 48), "a message");
    if (path.empty() || !Filesystem::exists(Filesystem::Path(path))){
        return std::format("Expected the interrupt's frame saved, got \"{}\".", path);
    }
    Filesystem::remove(Filesystem::Path(path));
    if (path.find("AutoStory/route/") == std::string::npos || path.find("interrupt-a_message") == std::string::npos){
        return std::format("Expected it under AutoStory/route/, named for the interrupt: {}", path);
    }
    if (!save_leg_interrupt_frame(logger, ImageRGB32(), "a message").empty()){
        return "Expected no file for an empty frame.";
    }
    return true;
}

struct RecoveryFrameCase{
    struct Input{
        UnitTestResult (*body)();
    };
    struct Expectation{
        std::string_view description;
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_OverworldRecoveryFrames : public UnitTest{
public:
    explicit Test_OverworldRecoveryFrames(RecoveryFrameCase scenario)
        : UnitTest("PokemonFRLG::OverworldRecovery - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RecoveryFrameCase m_scenario;
};

const RecoveryFrameCase RECOVERY_FRAME_CASES[] = {
    {
        .name = "a tap against a rock on the water",
        .input = {.body = test_water_tap_against_rock},
        .expectation = {.description = "the water's animation is no step"},
    },
    {
        .name = "a blocked tap with an animated edge band",
        .input = {.body = test_tap_blocked_with_animated_edge},
        .expectation = {.description = "an NPC or animated tile in an edge band does not turn a blocked tap into a step"},
    },
    {
        .name = "landed behind a roof",
        .input = {.body = test_landed_behind_roof},
        .expectation = {.description = "no water on the player's tile: on foot, sprite hidden or not"},
    },
    {
        .name = "the frame of a leg's interrupt",
        .input = {.body = test_interrupt_frame},
        .expectation = {.description = "saved under DebugDumps/AutoStory/route/, named for the interrupt"},
    },
};

} // namespace


void add_tests_OverworldRecoveryFrames(UnitTestDatabase& database){
    for (const RecoveryFrameCase& scenario : RECOVERY_FRAME_CASES){
        database.add<Test_OverworldRecoveryFrames>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
