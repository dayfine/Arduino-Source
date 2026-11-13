/*  BDSP AutoStory - Intro Menu Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonBDSP_AutoStory_IntroMenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Cards are flat: the frames' strips have stddev sums of 2 to 18.
const double MAX_STDDEV_SUM = 25;

enum class CardLook{ GREY, BLUE, OTHER };

//  Grey on the frames: (225, 233, 238) language buttons, (210, 218, 223)
//  portraits. Blue: (167, 206, 255) and (152, 195, 251). The background
//  between the cards is a bluer (203, 232, 253), neither.
CardLook strip_look(const ImageViewRGB32& screen, const ImageFloatBox& strip){
    const ImageStats stats = image_stats(extract_box_reference(screen, strip));
    if (stats.stddev.sum() > MAX_STDDEV_SUM){
        return CardLook::OTHER;
    }
    const FloatPixel& c = stats.average;
    const double low = std::min({c.r, c.g, c.b});
    const double high = std::max({c.r, c.g, c.b});
    if (low >= 195 && high - low <= 25){
        return CardLook::GREY;
    }
    if (c.b >= 230 && c.r >= 120 && c.b - c.r >= 50){
        return CardLook::BLUE;
    }
    return CardLook::OTHER;
}

//  A card's look: all its strips must agree.
CardLook card_look(const ImageViewRGB32& screen, const std::vector<ImageFloatBox>& strips){
    const CardLook first = strip_look(screen, strips[0]);
    for (size_t c = 1; c < strips.size(); c++){
        if (strip_look(screen, strips[c]) != first){
            return CardLook::OTHER;
        }
    }
    return first;
}

//  The one blue card among grey ones; nullopt otherwise.
std::optional<size_t> grid_cursor(const ImageViewRGB32& screen, const std::vector<std::vector<ImageFloatBox>>& cards){
    std::optional<size_t> cursor;
    for (size_t card = 0; card < cards.size(); card++){
        const CardLook look = card_look(screen, cards[card]);
        if (look == CardLook::OTHER || (look == CardLook::BLUE && cursor)){
            return std::nullopt;
        }
        if (look == CardLook::BLUE){
            cursor = card;
        }
    }
    return cursor;
}

//  A strip inside each button, left of its text (1080p: x 330-460 on the left
//  column, 1040-1170 on the right; 60 px tall around each row's centre).
const std::vector<std::vector<ImageFloatBox>>& language_buttons(){
    static const std::vector<std::vector<ImageFloatBox>> cards = []{
        const double column_x[2] = {0.172, 0.542};
        const double row_center_y[5] = {0.154, 0.280, 0.404, 0.528, 0.652};
        std::vector<std::vector<ImageFloatBox>> ret;
        for (size_t item = 0; item < LANGUAGE_SELECT_BUTTONS; item++){
            ret.push_back({ImageFloatBox(column_x[item % 2], row_center_y[item / 2] - 0.028, 0.068, 0.056)});
        }
        return ret;
    }();
    return cards;
}

//  Two strips of background beside each face (1080p: portraits from x 168,
//  576, 984, 1392 and y 124, 456; the strips 12-44 and 320-356 px in, 80-180
//  px down).
const std::vector<std::vector<ImageFloatBox>>& look_portraits(){
    static const std::vector<std::vector<ImageFloatBox>> cards = []{
        const double column_x[4] = {0.0875, 0.3000, 0.5125, 0.7250};
        const double row_y[2] = {0.1148, 0.4222};
        std::vector<std::vector<ImageFloatBox>> ret;
        for (size_t item = 0; item < LOOK_SELECT_PORTRAITS; item++){
            const double x = column_x[item % 4];
            const double y = row_y[item / 4] + 0.0741;
            ret.push_back({ImageFloatBox(x + 0.0063, y, 0.0167, 0.0926), ImageFloatBox(x + 0.1667, y, 0.0188, 0.0926)});
        }
        return ret;
    }();
    return cards;
}

} // namespace


std::optional<size_t> language_select_cursor(const ImageViewRGB32& screen){
    return grid_cursor(screen, language_buttons());
}

std::optional<size_t> look_select_cursor(const ImageViewRGB32& screen){
    return grid_cursor(screen, look_portraits());
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
