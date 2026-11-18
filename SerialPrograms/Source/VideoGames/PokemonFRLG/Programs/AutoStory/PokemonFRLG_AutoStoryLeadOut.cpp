/*  AutoStory Lead Out
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <string>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryLeadOut.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


LeadOnField lead_on_field(const LeadOnFieldInput& input){
    if (!input.own_out.empty() && input.own_out == input.lead){
        return LeadOnField::OUT;
    }
    if (!input.own_out.empty() && !input.member_sent){
        return LeadOnField::FORCED_OUT;
    }
    return input.box_gone ? LeadOnField::FAINTED : LeadOnField::OUT;
}

namespace{

//  The player's HP box's name, as WildEncounterReader's for the foe.
const ImageFloatBox OWN_NAME_BOX(0.585, 0.490, 0.260, 0.063);
//  The party list's names: the big top-left slot's, then the column's,
//  OWN_PARTY_ROW apart.
const ImageFloatBox FIRST_SLOT_NAME_BOX(0.131, 0.235, 0.158, 0.058);
constexpr double SLOT_NAME_X = 0.488;
constexpr double SLOT_NAME_Y = 0.085;
constexpr double SLOT_NAME_WIDTH = 0.204;
constexpr double SLOT_NAME_HEIGHT = 0.058;
constexpr double OWN_PARTY_ROW = 0.150;
constexpr size_t PARTY_SIZE = 6;

std::string read_name(Logger& logger, Language language, const ImageViewRGB32& name_box, const std::set<std::string>& species, bool bright){
    const std::vector<OCR::TextColorRange>& filters = bright ? BRIGHT_TEXT_FILTERS() : DARK_TEXT_FILTERS();
    const OCR::StringMatchResult result = Pokemon::PokemonNameReader(species).read_substring(
        logger, language, preprocess_for_ocr(name_box), filters,
        /*min_text_ratio=*/0.01, /*max_text_ratio=*/0.50, WildEncounterReader::MAX_LOG10P
    );
    return result.results.empty() ? std::string() : result.results.begin()->second.token;
}

} // namespace

std::string read_own_battler(Logger& logger, Language language, const ImageViewRGB32& screen){
    if (language == Language::None){
        return "";
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const std::string read = read_name(logger, language, extract_box_reference(game, OWN_NAME_BOX), battle_species(), /*bright=*/false);
    logger.log(read.empty() ? "Battle: our Pokemon out wasn't read." : std::format("Battle: our {} is out.", read));
    return read;
}

std::optional<size_t> party_slot_of(Logger& logger, Language language, const ImageViewRGB32& screen, std::string_view species){
    if (language == Language::None){
        return std::nullopt;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const std::set<std::string> wanted{std::string(species)};
    for (size_t slot = 0; slot < PARTY_SIZE; slot++){
        const ImageFloatBox box = slot == 0 ? FIRST_SLOT_NAME_BOX : ImageFloatBox(
            SLOT_NAME_X, SLOT_NAME_Y + OWN_PARTY_ROW * static_cast<double>(slot - 1), SLOT_NAME_WIDTH, SLOT_NAME_HEIGHT
        );
        if (read_name(logger, language, extract_box_reference(game, box), wanted, /*bright=*/true) == species){
            return slot;
        }
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
