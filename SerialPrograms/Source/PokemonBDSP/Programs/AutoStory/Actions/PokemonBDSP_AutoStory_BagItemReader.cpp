/*  BDSP AutoStory - Bag item reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string_view>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "PokemonBDSP_AutoStory_BagItemReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

//  The story's items in the same form as Packages' *OCR.json dictionaries
//  (language -> slug -> names), so it can move to a resource file unchanged.
//  Names are the games' official ones; Poke Ball's match PokeballNameOCR.json.
constexpr std::string_view STORY_ITEM_OCR = R"JSON({
"eng": {
    "potion": ["Potion"], "antidote": ["Antidote"], "paralyze-heal": ["Paralyze Heal"],
    "awakening": ["Awakening"], "poke-ball": ["Poké Ball", "Poke Ball"], "repel": ["Repel"],
    "super-repel": ["Super Repel"], "max-repel": ["Max Repel"], "escape-rope": ["Escape Rope"],
    "coupon-1": ["Coupon 1"], "coupon-2": ["Coupon 2"], "coupon-3": ["Coupon 3"]
},
"fra": {
    "potion": ["Potion"], "antidote": ["Antidote"], "paralyze-heal": ["Anti-Para"],
    "awakening": ["Réveil"], "poke-ball": ["Poké Ball"], "repel": ["Repousse"],
    "super-repel": ["Super Repousse"], "max-repel": ["Repousse Max"], "escape-rope": ["Corde Sortie"]
},
"deu": {
    "potion": ["Trank"], "antidote": ["Gegengift"], "paralyze-heal": ["Para-Heiler"],
    "awakening": ["Aufwecker"], "poke-ball": ["Pokéball"], "repel": ["Schutz"],
    "super-repel": ["Superschutz"], "max-repel": ["Top-Schutz"], "escape-rope": ["Fluchtseil"]
},
"spa": {
    "potion": ["Poción"], "antidote": ["Antídoto"], "paralyze-heal": ["Antiparalizador"],
    "awakening": ["Despertar"], "poke-ball": ["Poké Ball", "Poke Ball"], "repel": ["Repelente"],
    "super-repel": ["Superrepelente"], "max-repel": ["Repelente Máximo"], "escape-rope": ["Cuerda Huida"]
},
"ita": {
    "potion": ["Pozione"], "antidote": ["Antidoto"], "paralyze-heal": ["Antiparalisi"],
    "awakening": ["Sveglia"], "poke-ball": ["Poké Ball"], "repel": ["Repellente"],
    "super-repel": ["Superrepellente"], "max-repel": ["Repellente Max"], "escape-rope": ["Fune di fuga"]
},
"jpn": {
    "potion": ["キズぐすり"], "antidote": ["どくけし"], "paralyze-heal": ["まひなおし"],
    "awakening": ["ねむけざまし"], "poke-ball": ["モンスターボール"], "repel": ["むしよけスプレー"],
    "super-repel": ["シルバースプレー"], "max-repel": ["ゴールドスプレー"], "escape-rope": ["あなぬけのヒモ"]
},
"kor": {
    "potion": ["상처약"], "antidote": ["해독제"], "paralyze-heal": ["마비치료제"],
    "awakening": ["잠깨는약"], "poke-ball": ["몬스터볼"], "repel": ["벌레회피스프레이"],
    "super-repel": ["실버스프레이"], "max-repel": ["골드스프레이"], "escape-rope": ["동굴탈출로프"]
},
"chi_sim": {
    "potion": ["伤药"], "antidote": ["解毒药"], "paralyze-heal": ["解麻药"],
    "awakening": ["解眠药"], "poke-ball": ["精灵球"], "repel": ["除虫喷雾"],
    "super-repel": ["白银喷雾"], "max-repel": ["黄金喷雾"], "escape-rope": ["离洞绳"]
},
"chi_tra": {
    "potion": ["傷藥"], "antidote": ["解毒藥"], "paralyze-heal": ["解麻藥"],
    "awakening": ["解眠藥"], "poke-ball": ["精靈球"], "repel": ["除蟲噴霧"],
    "super-repel": ["白銀噴霧"], "max-repel": ["黃金噴霧"], "escape-rope": ["離洞繩"]
}
})JSON";

class StoryItemMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const StoryItemMatcher& instance(){
        static const StoryItemMatcher matcher;
        return matcher;
    }

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        //  The list's names are dark gray on the white rows and on the
        //  cursor's highlight.
        static const std::vector<OCR::TextColorRange> DARK_TEXT{
            {0xff000000, 0xff606060},
            {0xff000000, 0xff808080},
            {0xff000000, 0xffa0a0a0},
        };
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, image, DARK_TEXT,
            MAX_LOG10P, MAX_LOG10P_SPREAD, 0.01, 0.50
        );
        return result.results.empty() ? "" : result.results.begin()->second.token;
    }

private:
    StoryItemMatcher()
        : SmallDictionaryMatcher(parse_json(std::string(STORY_ITEM_OCR)).to_object_throw())
    {}
};

} // namespace


std::string read_bag_item_slug(Logger& logger, Language language, const ImageViewRGB32& image){
    return StoryItemMatcher::instance().read(logger, language, image);
}

bool bag_item_named(Language language, std::string_view slug){
    const JsonValue json = parse_json(std::string(STORY_ITEM_OCR));
    const JsonObject* names = json.to_object_throw().get_object(language_data(language).code);
    return names != nullptr && names->get_value(std::string(slug)) != nullptr;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
