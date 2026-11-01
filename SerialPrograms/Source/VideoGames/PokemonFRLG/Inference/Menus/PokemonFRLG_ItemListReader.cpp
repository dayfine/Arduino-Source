/*  Item List Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Language.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on a capture of the Items pocket (console B, 2026-11-01): text
//  rows from y 13 to 21, 16 apart; names from x 97, "x" at 198, the count
//  to 221.
constexpr double ROW_TOP = 11;
constexpr double ROW_STEP = 16;
constexpr double ROW_HEIGHT = 12;

class ItemNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const ItemNameMatcher& instance(){
        static const ItemNameMatcher matcher;
        return matcher;
    }

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, DARK_TEXT_FILTERS(),
            MAX_LOG10P, MAX_LOG10P_SPREAD, 0.01, 0.50
        );
        return result.results.empty() ? "" : result.results.begin()->second.token;
    }

private:
    ItemNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : {
            std::pair<const char*, const char*>{"potion", "Potion"},
            {"antidote", "Antidote"},
            {"parlyz-heal", "Parlyz Heal"},
            {"poke-ball", "Poke Ball"},
            {"cancel", "Cancel"},
        }){
            JsonArray names;
            names.push_back(name);
            english[slug] = std::move(names);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }
};

ImageViewRGB32 game_box(const ImageViewRGB32& screen){
    return extract_box_reference(screen, GameSettings::instance().GAME_BOX);
}

}


ImageFloatBox item_list_name_box(int row){
    return ImageFloatBox(96.0 / 240, (ROW_TOP + ROW_STEP * row) / 160, 72.0 / 240, ROW_HEIGHT / 160);
}
ImageFloatBox bag_item_count_box(int row){
    return ImageFloatBox(208.0 / 240, (ROW_TOP + ROW_STEP * row) / 160, 15.0 / 240, ROW_HEIGHT / 160);
}

std::string read_item_list_name(Logger& logger, Language language, const ImageViewRGB32& screen, int row){
    return ItemNameMatcher::instance().read(logger, language, extract_box_reference(game_box(screen), item_list_name_box(row)));
}

std::vector<BagRow> read_bag_pocket(Logger& logger, Language language, const ImageViewRGB32& screen){
    std::vector<BagRow> rows;
    for (int row = 0; row < ITEM_LIST_ROWS; row++){
        const std::string item = read_item_list_name(logger, language, screen, row);
        if (item.empty() || item == "cancel"){
            break;
        }
        const ImageViewRGB32 count = extract_box_reference(game_box(screen), bag_item_count_box(row));
        rows.push_back(BagRow{item, OCR::read_number_waterfill(logger, count, 0xff000000, 0xff808080)});
    }
    return rows;
}



}
}
}
