/*  Player Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <limits>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  graphics/object_events/pics/people/red_normal.png, frames 0 to 2, rows 12
//  to 30 (the rest is empty): one hex digit per pixel, an index into
//  PALETTE, and '.' where the sprite is transparent. Facing east is facing
//  west mirrored.

//  Standing, facing south.
const char* const SOUTH[] = {
    "....88888888....",
    "...8cbbbbbbc8...",
    "...8bbbbbbbb8...",
    "..8cbbbbbbbbc8..",
    "..fccbbbbbbccf..",
    "..fcccc99ccccf..",
    ".f18acaccaca81f.",
    ".f3f69999996f3f.",
    "..f4366666634f..",
    "..f332f22f233f..",
    "...4334224334...",
    "..f6f433334f6f..",
    ".4266cffffc6624.",
    ".4336ccaacc6334.",
    "..4ffcb99bcff4..",
    "...676ffff676...",
    "...ff77ff77ff...",
    "...f66f..f66f...",
    "....ff....ff....",
};

//  Standing, facing north.
const char* const NORTH[] = {
    "....88888888....",
    "...8cbbbbbbc8...",
    "...8bbbbbbbb8...",
    "..8cbbbbbbbbc8..",
    "..8ccbbbbbbcc8..",
    "..fccccccccccf..",
    "..f8cccccccc8f..",
    ".f148ccffcc841f.",
    ".f3f48888884f3f.",
    "..ff44144144ff..",
    "...fff4444fff...",
    "..f64effffe4ff..",
    ".426ceccccec624.",
    ".434c4dddd4c434.",
    "..4ffeddddeff4..",
    "...67eeeeee76...",
    "...ff7e44e7ff...",
    "...f66ffff66f...",
    "....ff....ff....",
};

//  Standing, facing west.
const char* const WEST[] = {
    "....88888888....",
    "...8cbbbbbbc8...",
    "..8cbbbbbbbb8...",
    ".889bbbbbbbbc8..",
    "f989cbbbbbbcc8..",
    "f9caccccccccc8..",
    "f9accccccccc8f..",
    ".faaaccccc884f..",
    "..46888888444f..",
    "..433f331234f...",
    "...42423323f....",
    "....4333344ef...",
    "....844ff8eddf..",
    "....8cf66fed4f..",
    "....fc866f84ef..",
    ".....ff33ffef...",
    ".....f7447ff....",
    ".....f6c6f......",
    "......fff.......",
};

const uint8_t PALETTE[16][3] = {
    {  0,   0,   0}, {172, 123,  65}, {255, 197, 148}, {222, 148, 115},
    {123,  65,  65}, {  0,   0,   0}, { 57,  57, 123}, {115, 164, 197},
    {106,  41,  41}, {238, 238, 255}, {180, 180, 213}, {255, 106,  74},
    {197,  57,  57}, {255, 222,  90}, {189, 156,  57}, {  0,   0,   0},
};

constexpr int SPRITE_WIDTH = 16;
constexpr int SPRITE_ROWS = 19;
//  Where the sprite's first drawn row sits on the GBA's 240x160 screen while
//  the camera follows the player. Measured on the console.
constexpr int SPRITE_X = 112;
constexpr int SPRITE_Y = 56 + 12;
constexpr int GBA_WIDTH = 240;
constexpr int GBA_HEIGHT = 160;

//  Average difference per channel over the sprite's pixels. On the console a
//  match scored about 12 and the next best sprite over 50.
constexpr double MATCH_MAX = 25;
constexpr double MATCH_MARGIN = 2.0;


//  How far the screen differs from `sprite`, drawn `dy` GBA pixels lower.
double difference(const ImageViewRGB32& game, const char* const sprite[], bool mirror, int dy){
    const double scale_x = (double)game.width() / GBA_WIDTH;
    const double scale_y = (double)game.height() / GBA_HEIGHT;
    long total = 0;
    int pixels = 0;
    for (int row = 0; row < SPRITE_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH; col++){
            const char c = sprite[row][mirror ? SPRITE_WIDTH - 1 - col : col];
            if (c == '.'){
                continue;
            }
            const int index = c <= '9' ? c - '0' : c - 'a' + 10;
            const size_t x = (size_t)((SPRITE_X + col + 0.5) * scale_x);
            const size_t y = (size_t)((SPRITE_Y + dy + row + 0.5) * scale_y);
            if (x >= game.width() || y >= game.height()){
                return std::numeric_limits<double>::max();
            }
            const Color pixel(game.pixel(x, y));
            total += std::abs((int)pixel.red() - PALETTE[index][0]);
            total += std::abs((int)pixel.green() - PALETTE[index][1]);
            total += std::abs((int)pixel.blue() - PALETTE[index][2]);
            pixels++;
        }
    }
    return (double)total / (3 * pixels);
}

}


std::optional<Direction> read_player_facing(const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    struct Candidate{
        Direction direction;
        const char* const* sprite;
        bool mirror;
};
    const Candidate candidates[] = {
        {Direction::SOUTH, SOUTH, false},
        {Direction::NORTH, NORTH, false},
        {Direction::WEST, WEST, false},
        {Direction::EAST, WEST, true},
};

    double best = std::numeric_limits<double>::max();
    double second = best;
    Direction facing = Direction::SOUTH;
    for (const Candidate& candidate : candidates){
        //  A pixel either way, for a box that is slightly off.
        double score = std::numeric_limits<double>::max();
        for (int dy = -1; dy <= 1; dy++){
            score = std::min(score, difference(game, candidate.sprite, candidate.mirror, dy));
        }
        if (score < best){
            second = best;
            best = score;
            facing = candidate.direction;
        }else{
            second = std::min(second, score);
        }
    }
    if (best > MATCH_MAX || second < best * MATCH_MARGIN){
        return std::nullopt;
    }
    return facing;
}



namespace{

class Test_PlayerFacingReader : public UnitTest{
public:
    Test_PlayerFacingReader(const std::string& image, std::optional<Direction> expected)
        : UnitTest("PokemonFRLG::PlayerFacingReader - " + image)
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_expected(expected)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::optional<Direction> facing = read_player_facing(ImageRGB32(m_image));
        if (facing == m_expected){
            return true;
        }
        return std::string("Read ") + (facing ? direction_name(*facing) : "nothing");
    }

private:
    std::string m_image;
    std::optional<Direction> m_expected;
};

}

void add_tests_PlayerFacingReader(UnitTestDatabase& database){
    //  LeafGreen on the Switch, captured with a 1080p card.
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/South-Route24.png", Direction::SOUTH);
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/North-Route24.png", Direction::NORTH);
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/South-CeruleanCave.png", Direction::SOUTH);
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/North-CeruleanCave.png", Direction::NORTH);
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Surfing.png", std::nullopt);
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Bag.png", std::nullopt);
}



}
}
}
