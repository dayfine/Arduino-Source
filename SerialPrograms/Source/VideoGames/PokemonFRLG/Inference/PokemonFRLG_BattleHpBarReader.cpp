/*  Battle HP Bar Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_BattleHpBarReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  As fractions of the 240x160 screen: the bar's fill, 48 pixels from its
//  left end. The fill is 3 rows tall, a darker one over two lighter ones,
//  and the box it is in bobs down by a pixel: these 5 rows hold both
//  lighter rows either way (console B, 2026-11-03). The share filled is
//  counted by columns, on the lighter colors: a 2-row box on the darker
//  row and one lighter one, whose pixels were counted, read 17/55 HP as
//  15% of the bar, and as 2% with the box a pixel lower.
const ImageFloatBox HP_BAR_BOX(174.0 / 240, 90.0 / 160, 48.0 / 240, 5.0 / 160);
//  The opponent's bar, in its box top left: 48 pixels from x 52, its
//  lighter rows at y 33 to 35 (console B, 2026-11-03, the S.S. Anne rival's
//  Pidgeotto at full HP and Raticate at 7 of 48 pixels).
const ImageFloatBox OPPONENT_HP_BAR_BOX(52.0 / 240, 32.0 / 160, 48.0 / 240, 5.0 / 160);

//  A column needs this many pixels of a fill color to count as filled: a
//  lighter row is about 6 pixels tall at 1080p.
constexpr size_t MIN_COLUMN_PIXELS = 3;
//  Filled columns for the bar to count as seen: about half a game pixel.
//  At 1 HP a single pixel of red shows (console B, 2026-11-01, 1/25 HP).
constexpr size_t MIN_SEEN_COLUMNS = 3;

//  The lighter fill colors, measured on console B's captures (2026-11-01):
//  green (100, 251, 171), yellow (255, 228, 23), red (255, 113, 68).
std::optional<HpBarColor> fill_color(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    if (g > 200 && r < 160 && b > 120 && b < 210){
        return HpBarColor::GREEN;
    }
    if (r > 220 && g > 180 && b < 80){
        return HpBarColor::YELLOW;
    }
    if (r > 220 && g > 80 && g < 150 && b < 110){
        return HpBarColor::RED;
    }
    return std::nullopt;
}

HpBarReading read_hp_bar(const ImageViewRGB32& game, const ImageFloatBox& box){
    const ImageViewRGB32 bar = extract_box_reference(game, box);
    size_t counts[3] = {};
    size_t filled = 0;
    for (size_t x = 0; x < bar.width(); x++){
        size_t column = 0;
        for (size_t y = 0; y < bar.height(); y++){
            const std::optional<HpBarColor> color = fill_color(bar.pixel(x, y));
            if (color){
                counts[(size_t)*color]++;
                column++;
            }
        }
        if (column >= MIN_COLUMN_PIXELS){
            filled++;
        }
    }
    HpBarReading reading;
    reading.seen = filled >= MIN_SEEN_COLUMNS;
    reading.fill = bar.width() == 0 ? 0 : double(filled) / bar.width();
    const size_t* most = std::max_element(counts, counts + 3);
    reading.color = HpBarColor(most - counts);
    return reading;
}

} // namespace


HpBarReading read_player_hp_bar(const ImageViewRGB32& game){
    return read_hp_bar(game, HP_BAR_BOX);
}

HpBarReading read_opponent_hp_bar(const ImageViewRGB32& game){
    return read_hp_bar(game, OPPONENT_HP_BAR_BOX);
}

std::optional<double> read_opponent_hp_fill(const ImageViewRGB32& screen){
    const HpBarReading bar = read_opponent_hp_bar(extract_box_reference(screen, GameSettings::instance().GAME_BOX));
    return bar.seen ? std::optional<double>(bar.fill) : std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
