/*  PokemonLZA AutoStory - Party row count
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryPartyRowCount.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


size_t party_row_count(const ImageViewRGB32& screen){
    const ImageFloatBox first(160.0 / 1920, 1028.0 / 1080, 50.0 / 1920, 5.0 / 1080);
    size_t count = 0;
    for (size_t slot = 0; slot < 5; slot++){
        const double bar = share_of(screen, shifted(first, 76.6 * slot), [](Color pixel){
            const int r = pixel.red();
            const int g = pixel.green();
            const int b = pixel.blue();
            const bool green = g > 150 && g > r + 80 && g > b + 40;
            const bool yellow = r > 200 && g > 160 && b < 90;
            const bool red = r > 200 && g < 100 && b < 100;
            return green || yellow || red;
        });
        if (bar >= 0.3){
            count++;
        }
    }
    return count;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
