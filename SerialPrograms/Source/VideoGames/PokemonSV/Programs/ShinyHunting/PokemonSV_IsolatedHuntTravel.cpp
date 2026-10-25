#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "VideoGames/PokemonSV/Inference/Map/PokemonSV_MapMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV_IsolatedHuntFlight.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_RegionTravel.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "PokemonSV_IsolatedHuntTravel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

class CentralFlight : public IsolatedFlightActions{
public:
    CentralFlight(VideoStream& stream, ProControllerContext& context, Language language)
        : m_stream(stream), m_context(context), m_language(language)
    {}
    virtual bool open_confirmation() override{
        pbf_press_button(m_context, BUTTON_A, 160ms, 1040ms);
        MapFlyMenuWatcher menu(COLOR_BLUE);
        if (wait_until(m_stream, m_context, 5000ms, {menu}) < 0){ return false; }
        pbf_press_button(m_context, BUTTON_A, 160ms, 1040ms);
        PromptDialogWatcher prompt(COLOR_BLUE, {0.686, 0.494, 0.171, 0.163});
        return wait_until(m_stream, m_context, 5000ms, {prompt}) == 0;
    }
    virtual std::string read_destination() override{
        m_context.wait_for(250ms);
        const auto screen = m_stream.video().snapshot();
        const auto text = OCR::ocr_read(m_language,
            extract_box_reference(screen, ImageFloatBox{0.300, 0.335, 0.400, 0.068}));
        m_stream.log("Flight destination read: " + text);
        return text;
    }
    virtual bool confirmation_visible() override{
        PromptDialogDetector prompt(COLOR_BLUE, {0.686, 0.494, 0.171, 0.163});
        return prompt.detect(m_stream.video().snapshot());
    }
    virtual bool confirm_and_wait() override{
        pbf_press_button(m_context, BUTTON_A, 160ms, 1040ms);
        OverworldWatcher overworld(m_stream.logger(), COLOR_BLUE);
        return wait_until(m_stream, m_context, 60000ms, {overworld}) == 0;
    }
private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    Language m_language;
};

bool refly_local_anchor(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location
){
    open_map_from_overworld(info, stream, context);
    context.wait_for_all_requests();
    const auto kind = location == IsolatedHuntLocation::CHARGESTONE_CAVE ? FlyPoint::FAST_TRAVEL : FlyPoint::POKECENTER;
    return fly_to_visible_closest_flypoint_cur_zoom_level(info, stream, context, kind);
}

}

bool travel_to_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location, bool from_zero_gate, Language language
){
    if (location == IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL){
        open_map_from_overworld(info, stream, context);
        if (from_zero_gate){
            stream.log("Central candidate seek: one zoom out, west 950ms, south 650ms; name verification required.");
            // Candidate derived from the Central-to-Zero-Gate map traversal; verify identity before flying.
            pbf_press_button(context, BUTTON_ZL, 160ms, 840ms);
            pbf_move_left_joystick(context, {-1, 0}, 950ms, 200ms);
            pbf_move_left_joystick(context, {0, -1}, 650ms, 200ms);
        }
        if (!detect_closest_flypoint_and_move_map_cursor_there(info, stream, context, FlyPoint::POKECENTER)){
            return false;
        }
        CentralFlight flight(stream, context, language);
        const bool arrived = fly_to_isolated_central(flight, language);
        if (!arrived){ stream.log("Central flight failed; stopping for inspection.", COLOR_RED); }
        return arrived;
    }
    if (!from_zero_gate){
        return refly_local_anchor(info, stream, context, location);
    }
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        fly_from_paldea_to_blueberry_entrance(info, stream, context);
        return_to_plaza(info, stream, context);
        central_to_chargestone(info, stream, context);
        return true;
    }
    return false;
}

void approach_isolated_patch(VideoStream& stream, ProControllerContext& context, IsolatedHuntLocation location){
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        stream.log("Approach: existing Chargestone court; verify this is the Chespin Cave habitat.");
        pbf_move_left_joystick(context, {0, +1}, 2000ms, 400ms);
        pbf_move_left_joystick(context, {+1, 0}, 1440ms, 400ms);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    }else{
        stream.log("Central approach requires supervised matching of the demonstrated path.", COLOR_BLUE);
    }
    context.wait_for_all_requests();
}

bool return_from_isolated_hunt(const ProgramInfo& info, VideoStream& stream,
    ProControllerContext& context, IsolatedHuntLocation location
){
    if (location == IsolatedHuntLocation::CHARGESTONE_CAVE){
        open_map_from_overworld(info, stream, context);
        pbf_press_button(context, BUTTON_R, 400ms, 2400ms);
        pbf_press_button(context, BUTTON_ZL, 160ms, 840ms);
        pbf_move_left_joystick(context, {-1, 0}, 200ms, 800ms);
        pbf_press_button(context, BUTTON_ZR, 160ms, 840ms);
        context.wait_for_all_requests();
        return fly_to_visible_closest_flypoint_cur_zoom_level(info, stream, context, FlyPoint::FAST_TRAVEL);
    }
    stream.log("Central return route is uncalibrated. Supervised return to Zero Gate required; no automatic departure.", COLOR_BLUE);
    return false;
}

}
}
}
