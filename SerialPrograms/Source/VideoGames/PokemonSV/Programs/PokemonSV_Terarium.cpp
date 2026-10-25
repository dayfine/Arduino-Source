/*  Terarium
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_PlaceNameBannerReader.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "PokemonSV_TerariumPlazaReturn.h"
#include "PokemonSV_Terarium.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

//The map reopens at the zoom it was left at (4 levels, 3 by default). The corner
//moves below were measured one level out from the default, so get there from any zoom:
//all the way out, then one level in.
static void zoom_map_to_corner_level(ProControllerContext& context){
    for (int i = 0; i < 3; i++){
        pbf_press_button(context, BUTTON_ZL, 320ms, 480ms);
    }
    pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);
}

//The zoom the map opens at after a flight (level 3 of 4), from any zoom.
static void zoom_map_to_default_level(ProControllerContext& context){
    for (int i = 0; i < 3; i++){
        pbf_press_button(context, BUTTON_ZL, 320ms, 480ms);
    }
    pbf_press_button(context, BUTTON_ZR, 320ms, 480ms);
    pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);
}


namespace{

// What one fly toward Central Plaza did.
enum class PlazaFly{
    Flew,
    DidNotFly,          //  No Pokecenter to fly to, or the fly was interrupted.
    StoppedAtBattle,    //  Only when stop_at_battle is set.
};

// Flies to the closest Pokecenter on the map, once. Nothing here checks where
// the player lands: the closest-Pokecenter search can fall back to any fly
// point. Returns DidNotFly if the fly failed or a battle interrupted it (the
// battle is fled); the caller decides whether to try again, so one return
// can't loop for minutes. With stop_at_battle set, it instead stops at a wild
// battle's menu and returns StoppedAtBattle.
PlazaFly fly_toward_plaza(
    const ProgramInfo& info, VideoStream& stream, ProControllerContext& context,
    bool laggy, bool stop_at_battle
){
    stream.log("Attempting to return to Central Plaza.");
    //Modified version of handle_battles_and_back_to_pokecenter()
    bool returned_to_pokecenter = false;

    {
        EncounterWatcher encounter_watcher(stream, COLOR_RED);
        int ret = run_until<ProControllerContext>(
            stream, context,
            [&](ProControllerContext& context){
                //Exit any dialogs (ex. Cyrano upgrading BBQs)
                OverworldWatcher overworld(stream.logger(), COLOR_RED);
                int ret_overworld = run_until<ProControllerContext>(
                    stream, context,
                    [&](ProControllerContext& context){
                        pbf_mash_button(context, BUTTON_B, 80000ms);
                    },
                    {overworld}
                );
                if (ret_overworld == 0){
                    stream.log("Overworld detected.");
                }
                context.wait_for_all_requests();

                try{
                    open_map_from_overworld(info, stream, context);
                }catch (...){
                    jump_off_wall_until_map_open(info, stream, context);
                }

                //Move cursor to top left corner - even works when at Entrance fly point
                zoom_map_to_corner_level(context);
                pbf_move_left_joystick(context, {-1, +1}, 4000ms, 320ms);

                //Now move toward center
                if (laggy){
                    pbf_move_left_joystick(context, {+1, -1}, 2400ms, 320ms); //overshoot by a bit (still works even if not laggy)
                }else{
                    pbf_move_left_joystick(context, {+1, -1}, 2000ms, 320ms); //250 is more accurate but 300 helps with lag
                }
                pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);

                try{
                    //The only pokecenter on the map is Central Plaza. Each failed search
                    //costs ~13 s, and repeating the same one rarely helps (bbq-m5 11:20:
                    //17 identical "no Pokecenter" tries), so give up after 3.
                    fly_to_closest_pokecenter_on_map(info, stream, context, 3);
                    context.wait_for_all_requests();
                    returned_to_pokecenter = true;
                }catch (...){
                    //The next try mashes B back to the overworld first.
                    stream.log("Failed to return to Pokecenter.", COLOR_RED);
                }
                context.wait_for_all_requests();
            },
            {
                static_cast<VisualInferenceCallback&>(encounter_watcher),
                static_cast<AudioInferenceCallback&>(encounter_watcher),
            }
        );
        if (ret == 0){
            stream.log("Battle menu detected.");
            encounter_watcher.throw_if_no_sound();

            bool is_shiny = (bool)encounter_watcher.shiny_screenshot();
            if (is_shiny){
                stream.log("Shiny detected!");
                pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
                throw ProgramFinishedException();
            }else if (stop_at_battle){
                stream.log("Detected battle. Stopping at the battle menu.");
                return PlazaFly::StoppedAtBattle;
            }else{
                stream.log("Detected battle. Running from battle.");
                try{
                    //Smoke Ball or Flying type required due to Arena Trap
                    NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                    battle_menu.move_to_slot(stream, context, 3);
                    pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                }catch (...){
                    stream.log("Unable to flee.");
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Unable to flee!",
                        stream
                    );
                }
            }
        }
    }
    context.wait_for_all_requests();
    return returned_to_pokecenter ? PlazaFly::Flew : PlazaFly::DidNotFly;
}

// Thrown out of PlazaReturnConsoleActions::fly_to_plaza() to leave the
// return-and-check loop when a fly stops at a wild battle.
struct StoppedAtBattle{};

class PlazaReturnConsoleActions : public PlazaReturnActions{
public:
    PlazaReturnConsoleActions(
        const ProgramInfo& info, VideoStream& stream, ProControllerContext& context,
        Language language, bool stop_at_battle
    )
        : m_info(info)
        , m_stream(stream)
        , m_context(context)
        , m_language(language)
        , m_stop_at_battle(stop_at_battle)
    {}

    virtual bool fly_to_plaza(bool overshoot) override{
        switch (fly_toward_plaza(m_info, m_stream, m_context, overshoot, m_stop_at_battle)){
        case PlazaFly::StoppedAtBattle:
            throw StoppedAtBattle();
        case PlazaFly::Flew:
            return true;
        default:
            return false;
        }
    }

    // The banner fades in shortly after the overworld is back and stays for
    // a few seconds. Read until it matches or the window ends.
    virtual std::string read_landing_banner(const std::function<bool(const std::string& text)>& matches) override{
        PlaceNameBannerReader reader;
        VideoOverlaySet overlays(m_stream.overlay());
        reader.make_overlays(overlays);

        std::string last;
        const WallClock deadline = current_time() + std::chrono::seconds(8);
        while (current_time() < deadline){
            m_context.wait_for_all_requests();
            VideoSnapshot screen = m_stream.video().snapshot();
            for (std::string& text : reader.read(m_stream.logger(), m_language, screen)){
                if (matches(text)){
                    return text;
                }
                if (!text.empty()){
                    last = std::move(text);
                }
            }
            m_context.wait_for(std::chrono::milliseconds(250));
        }
        return last;
    }

private:
    const ProgramInfo& m_info;
    VideoStream& m_stream;
    ProControllerContext& m_context;
    const Language m_language;
    const bool m_stop_at_battle;
};

//  Set by stay_away_from_plaza(): the player is not at Central Plaza.
bool away_from_plaza = false;
Language away_language = Language::None;

//  Returns true if it stopped at a wild battle's menu (only when stop_at_battle
//  is set). Otherwise it flees each battle and returns false once at the plaza.
bool return_to_plaza_impl(
    const ProgramInfo& info, VideoStream& stream, ProControllerContext& context,
    Language language, bool stop_at_battle
){
    away_from_plaza = false;
    const bool ocr_available = OCR::ocr_language_available(language);
    if (!ocr_available){
        stream.log("No OCR data for the game language. Central Plaza arrivals won't be checked.", COLOR_ORANGE);
    }
    const std::string expected_banner = central_plaza_banner_to_check(language, ocr_available);

    PlazaReturnConsoleActions actions(info, stream, context, language, stop_at_battle);
    PlazaReturnResult result;
    try{
        result = return_to_plaza_checked(stream.logger(), actions, expected_banner, 3);
    }catch (StoppedAtBattle&){
        return true;
    }
    if (result == PlazaReturnResult::FAILED){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Unable to return to Central Plaza: no landing showed its name.",
            stream
        );
    }
    return false;
}

}

void return_to_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, Language language){
    return_to_plaza_impl(info, stream, context, language, false);
}

bool return_to_plaza_unless_battle(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, Language language){
    return return_to_plaza_impl(info, stream, context, language, true);
}

void stay_away_from_plaza(Language language){
    away_from_plaza = true;
    away_language = language;
}
void ensure_at_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    if (away_from_plaza){
        stream.log("Away from Central Plaza. Returning before a step that starts there.");
        return_to_plaza_impl(info, stream, context, away_language, false);
    }
}
void reset_plaza_position(){
    away_from_plaza = false;
}

namespace{

//  Log the place name shown on landing, to check where a flight went.
void log_landing_banner(VideoStream& stream, ProControllerContext& context, Language language, const std::string& location){
    if (!OCR::ocr_language_available(language)){
        return;
    }
    PlaceNameBannerReader reader;
    const WallClock deadline = current_time() + std::chrono::seconds(5);
    while (current_time() < deadline){
        context.wait_for_all_requests();
        VideoSnapshot screen = stream.video().snapshot();
        for (const std::string& text : reader.read(stream.logger(), language, screen)){
            if (!text.empty()){
                stream.log("Landed for " + location + ", banner: \"" + text + "\".");
                return;
            }
        }
        context.wait_for(std::chrono::milliseconds(250));
    }
    stream.log("Landed for " + location + ", no banner read.");
}

//  Fly from anywhere to a central_to_*() destination. The map opens centered on the
//  player, so first put the cursor on Central Plaza the way fly_toward_plaza() finds it
//  (corner, then toward the center, then onto the Pokecenter icon), then move as from
//  Central Plaza. Returns false, with the map possibly still open, if it did not fly.
bool fly_from_anywhere(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    double x, double y,
    Milliseconds hold, Milliseconds release
){
    open_map_from_overworld(info, stream, context);
    zoom_map_to_corner_level(context);
    pbf_move_left_joystick(context, {-1, +1}, 4000ms, 320ms);
    pbf_move_left_joystick(context, {+1, -1}, 2000ms, 320ms);
    pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);
    pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);
    if (!detect_closest_flypoint_and_move_map_cursor_there(info, stream, context, FlyPoint::POKECENTER, 0.29)){
        return false;
    }
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    pbf_move_left_joystick(context, {x, y}, hold, release);
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    return fly_to_overworld_from_map(info, stream, context, true);
}

}

void map_move_cursor_fly(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    double x, double y,
    Milliseconds hold, Milliseconds release,
    std::string location
){
    stream.log("Attempting to fly to " + location + ".");

    if (away_from_plaza){
        stream.log("Away from Central Plaza. Flying to " + location + " directly.");
        bool flew = false;
        try{
            flew = fly_from_anywhere(info, stream, context, x, y, hold, release);
        }catch (OperationFailedException&){}
        if (flew){
            away_from_plaza = false;
            log_landing_banner(stream, context, away_language, location);
            return;
        }
        stream.log("Direct flight failed. Returning to Central Plaza first.", COLOR_ORANGE);
        press_Bs_to_back_to_overworld(info, stream, context);
        return_to_plaza_impl(info, stream, context, away_language, false);
    }

    for (int i = 0; i < 3; i++){
        try{
            open_map_from_overworld(info, stream, context);
            //A failed attempt closes the map one level out, without the flight that
            //would reset its zoom.
            if (i > 0){
                zoom_map_to_default_level(context);
            }
            pbf_move_left_joystick(context, {x, y}, hold, release);
            pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
            fly_to_overworld_from_map(info, stream, context);
            break;
        }
        catch(...){
            stream.log("Failed to fly! Closing map and retrying.");
            press_Bs_to_back_to_overworld(info, stream, context);
            if (i == 2){
                throw_and_log<TerariumFlyFailedException>(
                    stream.logger(),
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Unable to fly to " + location + "!",
                    stream
                );
            }
        }
    }
}

void central_to_polar_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -0.414, +1, 1840ms, 160ms, "Polar Rest Area");
}

void central_to_polar_class1(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -1, +0.844, 1200ms, 160ms, "Polar Classroom 1");
}

void central_to_polar_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -0.844, +0.805, 1960ms, 160ms, "Polar Plaza");
}

void central_to_coastal_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, +0.409, +1, 1680ms, 160ms, "Coastal Plaza");
}

void central_to_canyon_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -1, -1, 1720ms, 160ms, "Canyon Plaza");
}

void central_to_savanna_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, +0.291, -1, 1440ms, 160ms, "Savanna Plaza");
}

void central_to_canyon_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -1, -0.094, 1280ms, 160ms, "Canyon Rest Area");
}

void central_to_savanna_class(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, +1, -0.724, 1120ms, 160ms, "Savanna Classroom");
}

void central_to_chargestone(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    map_move_cursor_fly(info, stream, context, -1, -0.055, 1040ms, 160ms, "Chargestone Cavern");
}

void jump_glide_fly(
    VideoStream& stream, ProControllerContext& context,
    bool inverted_flight,
    Milliseconds hold_up,
    Milliseconds flight_wait,
    Milliseconds drop_time
){
    stream.log("Jump, glide, fly.");

    ssf_press_button(context, BUTTON_B, 0ms, 800ms);
    ssf_press_button(context, BUTTON_B, 0ms, 160ms, 80ms);
    ssf_press_button(context, BUTTON_B, 0ms, 160ms);
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_LCLICK, 400ms, 0ms);


    if (inverted_flight){
        pbf_move_left_joystick(context, {0, -1}, hold_up, 2000ms);
    }else{
        pbf_move_left_joystick(context, {0, +1}, hold_up, 2000ms);
    }

    pbf_wait(context, flight_wait);
    context.wait_for_all_requests();

    pbf_press_button(context, BUTTON_B, 160ms, 400ms);
    pbf_wait(context, drop_time);
    context.wait_for_all_requests();
}

}
}
}
