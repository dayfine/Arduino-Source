/*  AutoStory Battle Text
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "PokemonFRLG_AutoStoryBattleText.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The battle text box's two lines, as a fraction of the game screen.
const ImageFloatBox BATTLE_TEXT_BOX(0.04, 0.755, 0.9, 0.2);
//  Fewer letters than this is no text.
constexpr size_t MIN_LETTERS = 5;

} // namespace

std::optional<std::string> read_battle_text(Language language, const ImageViewRGB32& screen){
    if (language != Language::English){
        return std::nullopt;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageRGB32 text_image = preprocess_for_ocr(extract_box_reference(game, BATTLE_TEXT_BOX), 3, 1);
    for (const OCR::TextColorRange& filter : BRIGHT_TEXT_FILTERS()){
        size_t text_pixels = 0;
        const ImageRGB32 filtered = to_blackwhite_rgb32_range(text_pixels, text_image, true, filter.mins, filter.maxs);
        std::string text = text_pixels == 0 ? "" : OCR::ocr_read(language, filtered, OCR::PageSegMode::SINGLE_BLOCK);
        std::erase_if(text, [](char c){ return !std::isalpha(static_cast<unsigned char>(c)); });
        if (text.size() >= MIN_LETTERS){
            std::ranges::transform(text, text.begin(), [](char c){ return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
            return text;
        }
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
