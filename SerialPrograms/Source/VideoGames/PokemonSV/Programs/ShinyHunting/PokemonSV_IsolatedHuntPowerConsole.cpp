#include <array>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV_IsolatedHuntText.h"
#include "PokemonSV_IsolatedHuntPower.h"
#include "PokemonSV_IsolatedHuntPowerConsole.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
class ConsolePower : public IsolatedPowerActions{
public:
    ConsolePower(VideoStream& stream, ProControllerContext& context, Language language)
        : m_stream(stream), m_context(context), m_language(language){}
    virtual bool open_panel() override{
        for (size_t attempt = 0; attempt < 3; attempt++){
            pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 1040ms);
            m_context.wait_for_all_requests();
            if (!read_encounter_power().empty()){ return true; }
        }
        return false;
    }
    virtual std::string read_encounter_power() override{
        m_context.wait_for(250ms);
        const auto screen = m_stream.video().snapshot();
        const auto text = read_isolated_encounter_power(screen, m_language);
        m_stream.log("Isolated power OCR: " + text);
        return text;
    }
    virtual bool close_panel() override{
        pbf_press_button(m_context, BUTTON_B, 160ms, 1040ms);
        OverworldWatcher overworld(m_stream.logger(), COLOR_BLUE);
        return wait_until(m_stream, m_context, 5000ms, {overworld}) == 0;
    }
private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    Language m_language;
};
}
std::string read_isolated_encounter_power(const ImageViewRGB32& screen, Language language){
    const double width = language == Language::English ? 0.340 : 0.200;
    for (double y : std::array<double, 3>{0.160, 0.216, 0.272}){
        // Separate the widely spaced columns so OCR does not merge the type into the level.
        const auto label = OCR::ocr_read(language,
            extract_box_reference(screen, ImageFloatBox{0.530, y, width, 0.050}));
        const auto normalized = normalize_isolated_text(label);
        if (normalized.find(U"encounterpower") == std::u32string::npos &&
            normalized.find(U"遭遇力") == std::u32string::npos){ continue; }
        const auto level = OCR::ocr_read(Language::English,
            extract_box_reference(screen, ImageFloatBox{0.885, y, 0.050, 0.050}));
        return label + " " + level;
    }
    return "";
}
bool check_isolated_meal_power(VideoStream& stream, ProControllerContext& context,
    Language language, IsolatedEncounterType type, uint8_t level
){
    ConsolePower actions(stream, context, language);
    const bool verified = verify_isolated_power(actions, language, type, level);
    stream.log("Isolated verification: stage=meal power, method=OCR, type=" +
        std::string(type == IsolatedEncounterType::FIRE ? "fire" : "grass") + ", level=" +
        std::to_string(level) + ", accepted=" + std::to_string(verified));
    return verified;
}
}
}
}
