/*  Trainer Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <string>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_TrainerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

//  rocker.png's standing frames 0 (south), 1 (north) and 2 (west), rows 11
//  to 30 of each 16x32 frame (the rest is empty), as palette indices; 0 is
//  transparent.
constexpr int ROCKER_FIRST_ROW = 11;
constexpr int ROCKER_ROWS = 20;
constexpr int SPRITE_WIDTH = 16;
const char* const ROCKER_SOUTH[ROCKER_ROWS] = {
    "0000007777000000",
    "0000776666770000",
    "0007665555667000",
    "0007655665567000",
    "0076656556566700",
    "0076775555776700",
    "0f272167761272f0",
    "0f272111111272f0",
    "00f32f1111f23f00",
    "000f22422422f000",
    "000f32222223f000",
    "0042f433334f2400",
    "04222dffffd22240",
    "0422dc6cc6cd2240",
    "0433dcc55ccd3340",
    "00ddfcbbbbcfdd00",
    "000d7ffeeff7d000",
    "000fd77dd77df000",
    "000f66f00f66f000",
    "0000ff0000ff0000",
};
const char* const ROCKER_NORTH[ROCKER_ROWS] = {
    "0000007777000000",
    "0000776556770000",
    "0007655555567000",
    "0007655555567000",
    "0076655555566700",
    "0076665555666700",
    "0f276666666672f0",
    "0f276666666672f0",
    "00f7766666677f00",
    "000f77777777f000",
    "000f77777777f000",
    "0042fd7777df2400",
    "04223dffffd32240",
    "0422dccccccd2240",
    "043fdcbbbbcdf340",
    "00ddffbbbbffdd00",
    "000d7ffffff7d000",
    "000fd77dd77df000",
    "000f66f00f66f000",
    "0000ff0000ff0000",
};
const char* const ROCKER_WEST[ROCKER_ROWS] = {
    "0000007777000000",
    "0000775555770000",
    "0007555555567000",
    "0776555555557000",
    "0755655555666700",
    "0075566666666700",
    "00f7711766667f00",
    "00f1111277777f00",
    "004111f27237f000",
    "004224227237f000",
    "00042222337f0000",
    "0000433334f00000",
    "0000df744bd00000",
    "000075411dbd0000",
    "0000db411dbd0000",
    "0000df433dfd0000",
    "0000d7f44fd00000",
    "00000d7777d00000",
    "00000d667d000000",
    "000000fff0000000",
};

//  graphics/object_events/palettes/npc_blue.pal.
constexpr uint32_t NPC_BLUE[16] = {
    0xff73c5a4, 0xffffd5b4, 0xfff6bd94, 0xffde9473, 0xff7b4141, 0xffffde4a, 0xffd5ac20, 0xff836200,
    0xffa48bee, 0xff6a52bd, 0xff413962, 0xffee7341, 0xffac4139, 0xff4a3131, 0xffffffff, 0xff000000,
};

ImageRGB32 frame_of(const char* const rows[ROCKER_ROWS]){
    ImageRGB32 frame(SPRITE_WIDTH, ROCKER_ROWS);
    for (int row = 0; row < ROCKER_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH; col++){
            const char digit = rows[row][col];
            const int index = digit <= '9' ? digit - '0' : digit - 'a' + 10;
            frame.pixel(col, row) = index == 0 ? 0x00000000 : NPC_BLUE[index];
        }
    }
    return frame;
}

} // namespace


const PlayerSprites* trainer_sprites(std::string_view sprite){
    static const PlayerSprites ROCKER{frame_of(ROCKER_SOUTH), frame_of(ROCKER_NORTH), frame_of(ROCKER_WEST)};
    return sprite == "rocker" ? &ROCKER : nullptr;
}

std::optional<Direction> read_trainer_facing(
    const ImageViewRGB32& screen, const PlayerSprites& sprites, int dx_tiles, int dy_tiles
){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return match_facing_at(game, sprites, ROCKER_FIRST_ROW, dx_tiles, dy_tiles);
}


namespace{

constexpr int GBA_WIDTH = 240;
constexpr int GBA_HEIGHT = 160;
constexpr int SCALE = 4;
//  Kayden from the tile the pass waits on: two west, one north.
constexpr int KAYDEN_DX = -2;
constexpr int KAYDEN_DY = -1;

//  A GBA screen at SCALE times its size, the gym's floor grey, with
//  `sprite` (mirrored if `mirror`) drawn `dx`, `dy` tiles from the
//  player's frame (112, 56), and the player's spot left empty.
ImageRGB32 synthetic_game(const ImageViewRGB32* sprite, bool mirror, int dx, int dy){
    ImageRGB32 game(SCALE * GBA_WIDTH, SCALE * GBA_HEIGHT);
    game.fill(0xff9c9c94);
    const int x0 = 112 + 16 * dx;
    const int y0 = 56 + 16 * dy + ROCKER_FIRST_ROW;
    for (int row = 0; sprite != nullptr && row < ROCKER_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH; col++){
            const uint32_t pixel = sprite->pixel(mirror ? SPRITE_WIDTH - 1 - col : col, row);
            if (Color(pixel).alpha() == 0){
                continue;
            }
            for (int y = 0; y < SCALE; y++){
                for (int x = 0; x < SCALE; x++){
                    game.pixel(SCALE * (x0 + col) + x, SCALE * (y0 + row) + y) = pixel;
                }
            }
        }
    }
    return game;
}

std::string facing_text(const std::optional<Direction>& facing){
    return facing ? std::string(direction_name(*facing)) : std::string("nothing");
}

//  Kayden's frames drawn where he stands from the pass's tile read as the
//  way they face; nothing there, or Kayden a tile off, reads as nothing.
class Test_TrainerFacingSynthetic : public UnitTest{
public:
    Test_TrainerFacingSynthetic()
        : UnitTest("PokemonFRLG::TrainerFacingReader - rocker frames on a synthetic gym floor")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlayerSprites& rocker = *trainer_sprites("rocker");
        struct Case{
            const ImageViewRGB32* sprite;
            bool mirror;
            int dx;
            std::optional<Direction> expected;
        };
        const Case cases[] = {
            {&rocker.south, false, KAYDEN_DX, Direction::SOUTH},
            {&rocker.north, false, KAYDEN_DX, Direction::NORTH},
            {&rocker.west, false, KAYDEN_DX, Direction::WEST},
            {&rocker.west, true, KAYDEN_DX, Direction::EAST},
            {nullptr, false, KAYDEN_DX, std::nullopt},
            {&rocker.south, false, KAYDEN_DX + 1, std::nullopt},
        };
        for (const Case& test : cases){
            const ImageRGB32 game = synthetic_game(test.sprite, test.mirror, test.dx, KAYDEN_DY);
            const std::optional<Direction> facing = match_facing_at(game, rocker, ROCKER_FIRST_ROW, KAYDEN_DX, KAYDEN_DY);
            if (facing != test.expected){
                return std::format("Read {} instead of {}.", facing_text(facing), facing_text(test.expected));
            }
        }
        return trainer_sprites("hiker") == nullptr ? UnitTestResult(true) : UnitTestResult("A hiker's frames that aren't there.");
    }
};

} // namespace

void add_tests_TrainerFacingReader(UnitTestDatabase& database){
    database.add<Test_TrainerFacingSynthetic>();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
