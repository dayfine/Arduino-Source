/*  Player Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The sprite sheets' frames are 16x32, and the standing ones are 0 (south),
//  1 (north) and 2 (west). Rows 12 to 30 are drawn; the rest is empty. The
//  boy and the girl share the layout (pokefirered field_player_avatar.c:
//  OBJ_EVENT_GFX_RED_NORMAL and OBJ_EVENT_GFX_GREEN_NORMAL).
constexpr std::string_view BOY_SPRITE_SHEET = "PokemonFRLG/PlayerSprites/red_normal.png";
constexpr std::string_view GIRL_SPRITE_SHEET = "PokemonFRLG/PlayerSprites/green_normal.png";
constexpr int SHEET_FRAME_WIDTH = 16;
constexpr int SHEET_FIRST_ROW = 12;

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


ImageRGB32 sheet_frame(const ImageViewRGB32& sheet, int frame){
    return sheet.sub_image(frame * SHEET_FRAME_WIDTH, SHEET_FIRST_ROW, SPRITE_WIDTH, SPRITE_ROWS).copy();
}

//  How far the screen differs from `sprite`, drawn `dy` GBA pixels lower.
double difference(const ImageViewRGB32& game, const ImageViewRGB32& sprite, bool mirror, int dy){
    const double scale_x = (double)game.width() / GBA_WIDTH;
    const double scale_y = (double)game.height() / GBA_HEIGHT;
    long total = 0;
    int pixels = 0;
    for (int row = 0; row < SPRITE_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH; col++){
            const Color expected(sprite.pixel(mirror ? SPRITE_WIDTH - 1 - col : col, row));
            if (expected.alpha() == 0){
                continue;
            }
            const size_t x = (size_t)((SPRITE_X + col + 0.5) * scale_x);
            const size_t y = (size_t)((SPRITE_Y + dy + row + 0.5) * scale_y);
            if (x >= game.width() || y >= game.height()){
                return std::numeric_limits<double>::max();
            }
            const Color pixel(game.pixel(x, y));
            total += std::abs((int)pixel.red() - expected.red());
            total += std::abs((int)pixel.green() - expected.green());
            total += std::abs((int)pixel.blue() - expected.blue());
            pixels++;
        }
    }
    return pixels == 0 ? std::numeric_limits<double>::max() : (double)total / (3 * pixels);
}

} // namespace


namespace{

PlayerSprites load_player_sprites(std::string_view sheet_path){
    const ImageRGB32 sheet(std::format("{}{}", RESOURCE_PATH(), sheet_path));
    return PlayerSprites{sheet_frame(sheet, 0), sheet_frame(sheet, 1), sheet_frame(sheet, 2)};
}

} // namespace

const PlayerSprites& player_sprites(PlayerGender gender){
    switch (gender){
    case PlayerGender::GIRL:{
        static const PlayerSprites GIRL = load_player_sprites(GIRL_SPRITE_SHEET);
        return GIRL;
    }
    case PlayerGender::BOY:
        break;
    }
    static const PlayerSprites BOY = load_player_sprites(BOY_SPRITE_SHEET);
    return BOY;
}

std::optional<Direction> read_player_facing(const ImageViewRGB32& screen, PlayerGender gender){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return match_player_facing(game, player_sprites(gender));
}

std::optional<Direction> match_player_facing(const ImageViewRGB32& game, const PlayerSprites& sprites){
    struct Candidate{
        Direction direction;
        const ImageViewRGB32& sprite;
        bool mirror;
    };
    const Candidate candidates[] = {
        {Direction::SOUTH, sprites.south, false},
        {Direction::NORTH, sprites.north, false},
        {Direction::WEST, sprites.west, false},
        {Direction::EAST, sprites.west, true},
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
    //  The image's name ends with the way the player faces, or "None" where
    //  no standing player shows: <anything>_<South|North|West|East|None>.
    Test_PlayerFacingReader(const std::string& image)
        : UnitTest("PokemonFRLG::PlayerFacingReader - " + image)
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(m_image).stem().string());
        if (words.size() < 2){
            return "Error: filename must be <anything>_<South|North|West|East|None>.";
        }
        std::string expected = words.back();
        std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        std::optional<Direction> facing;
        try{
            facing = read_player_facing(ImageRGB32(m_image), PlayerGender::BOY);
        }catch (FileException& e){
            //  The screenshots and the sprite sheet aren't in CI's resources yet.
            return UnitTestResult(UnitTestResult::SKIPPED, "File not found: " + e.message());
        }
        const std::string read = facing ? direction_name(*facing) : "none";
        if (read == expected){
            return true;
        }
        return "Expected " + expected + ", read " + read;
    }

private:
    std::string m_image;
};

//  Made-up sprites, so the matching is tested without the sprite sheet:
//  solid red facing south, solid blue facing north, and green on the left
//  half facing west.
PlayerSprites fixture_sprites(){
    PlayerSprites sprites{
        ImageRGB32(SPRITE_WIDTH, SPRITE_ROWS),
        ImageRGB32(SPRITE_WIDTH, SPRITE_ROWS),
        ImageRGB32(SPRITE_WIDTH, SPRITE_ROWS),
    };
    sprites.south.fill(0xffff0000);
    sprites.north.fill(0xff0000ff);
    sprites.west.fill(0x00000000);
    for (int row = 0; row < SPRITE_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH / 2; col++){
            sprites.west.pixel(col, row) = 0xff00ff00;
        }
    }
    return sprites;
}

//  A grey GBA screen at twice its size, with `sprite` drawn where the
//  player stands.
ImageRGB32 fixture_screen(const ImageViewRGB32* sprite, bool mirror){
    ImageRGB32 game(2 * GBA_WIDTH, 2 * GBA_HEIGHT);
    game.fill(0xff808080);
    for (int row = 0; sprite != nullptr && row < SPRITE_ROWS; row++){
        for (int col = 0; col < SPRITE_WIDTH; col++){
            const uint32_t pixel = sprite->pixel(mirror ? SPRITE_WIDTH - 1 - col : col, row);
            if (Color(pixel).alpha() == 0){
                continue;
            }
            for (int dy = 0; dy < 2; dy++){
                for (int dx = 0; dx < 2; dx++){
                    game.pixel(2 * (SPRITE_X + col) + dx, 2 * (SPRITE_Y + row) + dy) = pixel;
                }
            }
        }
    }
    return game;
}

class Test_PlayerFacingFixture : public UnitTest{
public:
    Test_PlayerFacingFixture()
        : UnitTest("PokemonFRLG::PlayerFacingReader - made-up sprites")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        const PlayerSprites sprites = fixture_sprites();
        struct Case{
            const ImageViewRGB32* sprite;
            bool mirror;
            std::optional<Direction> expected;
        };
        const Case cases[] = {
            {&sprites.south, false, Direction::SOUTH},
            {&sprites.north, false, Direction::NORTH},
            {&sprites.west, false, Direction::WEST},
            {&sprites.west, true, Direction::EAST},
            {nullptr, false, std::nullopt},
        };
        for (const Case& test : cases){
            const std::optional<Direction> facing = match_player_facing(fixture_screen(test.sprite, test.mirror), sprites);
            if (facing != test.expected){
                return std::string("Read ") + (facing ? direction_name(*facing) : "nothing")
                    + " instead of " + (test.expected ? direction_name(*test.expected) : "nothing");
            }
        }
        return true;
    }
};

//  The real sheets, the boy's then the girl's, drawn on a grey screen: each
//  reads with its own sheet, and neither reads with the other's.
class Test_PlayerFacingGenders : public UnitTest{
public:
    Test_PlayerFacingGenders()
        : UnitTest("PokemonFRLG::PlayerFacingReader - boy and girl sheets")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        try{
            for (PlayerGender gender : {PlayerGender::BOY, PlayerGender::GIRL}){
                const std::string error = check_gender(gender);
                if (!error.empty()){
                    return error;
                }
            }
        }catch (FileException& e){
            return UnitTestResult(UnitTestResult::SKIPPED, "File not found: " + e.message());
        }
        return true;
    }

private:
    static std::string check_gender(PlayerGender gender){
        const std::string_view name = gender == PlayerGender::BOY ? "boy" : "girl";
        const PlayerSprites& sprites = player_sprites(gender);
        struct Case{
            const ImageViewRGB32* sprite;
            bool mirror;
            Direction expected;
        };
        const Case cases[] = {
            {&sprites.south, false, Direction::SOUTH},
            {&sprites.north, false, Direction::NORTH},
            {&sprites.west, false, Direction::WEST},
            {&sprites.west, true, Direction::EAST},
        };
        for (const Case& test : cases){
            const ImageRGB32 screen = fixture_screen(test.sprite, test.mirror);
            const std::optional<Direction> own = match_player_facing(screen, sprites);
            if (own != test.expected){
                return std::format("The {} facing {}: read {} with its own sheet.",
                    name, direction_name(test.expected), own ? direction_name(*own) : "nothing"
                );
            }
            const PlayerGender other = gender == PlayerGender::BOY ? PlayerGender::GIRL : PlayerGender::BOY;
            if (match_player_facing(screen, player_sprites(other)) == test.expected){
                return std::format("The {} facing {} read with the other player's sheet.", name, direction_name(test.expected));
            }
        }
        return "";
    }
};

} // namespace

void add_tests_PlayerFacingReader(UnitTestDatabase& database){
    database.add<Test_PlayerFacingFixture>();
    database.add<Test_PlayerFacingGenders>();
    //  LeafGreen on the Switch, captured with a 1080p card.
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Route24_South.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Route24_North.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/CeruleanCave_South.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/CeruleanCave_North.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Surfing_None.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/Bag_None.png");
    //  Fuchsia's Center (0, 5), after the turn tap east and before it, facing
    //  south: the escalator's rail is drawn over the legs and half the face
    //  (console B, 2026-11-13 00:39, 12.03). Too little shows to tell the
    //  way from the head alone (the cap matches every sprite), so these read
    //  nothing; they must never read a wrong way.
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/FuchsiaCenterEscalatorEast_None.png");
    database.add<Test_PlayerFacingReader>("PokemonFRLG/PlayerFacingReader/FuchsiaCenterEscalatorSouth_None.png");
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
