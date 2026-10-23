/*  Terarium
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Exceptions/UnexpectedBattleException.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "VideoGames/PokemonSV/Inference/Map/PokemonSV_MapFlyPointLabelReader.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "PokemonSV_Terarium.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

void return_to_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    stream.log("Attempting to return to Central Plaza.");
    //Modified version of handle_battles_and_back_to_pokecenter()
    bool returned_to_pokecenter = false;
    bool laggy = false;

    while(!returned_to_pokecenter){
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
                pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
                pbf_move_left_joystick(context, {-1, +1}, 4000ms, 320ms);

                //Now move toward center
                if (laggy){
                    pbf_move_left_joystick(context, {+1, -1}, 2400ms, 320ms); //overshoot by a bit (still works even if not laggy)
                }else{
                    pbf_move_left_joystick(context, {+1, -1}, 2000ms, 320ms); //250 is more accurate but 300 helps with lag
                }
                pbf_press_button(context, BUTTON_ZR, 320ms, 800ms);

                try{
                    //The only pokecenter on the map is Central Plaza
                    fly_to_closest_pokecenter_on_map(info, stream, context);
                    context.wait_for_all_requests();
                    returned_to_pokecenter = true;
                }catch (...){
                    stream.log("Failed to return to Pokecenter. Closing map and retrying.");
                    laggy = true;
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
}

void map_move_cursor_fly(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    double x, double y,
    Milliseconds hold, Milliseconds release,
    std::string location
){
    stream.log("Attempting to fly to " + location + ".");

    for (int i = 0; i < 3; i++){
        try{
            open_map_from_overworld(info, stream, context);
            pbf_move_left_joystick(context, {x, y}, hold, release);
            pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
            fly_to_overworld_from_map(info, stream, context);
            break;
        }
        catch(...){
            stream.log("Failed to fly! Closing map and retrying.");
            press_Bs_to_back_to_overworld(info, stream, context);
            if (i == 2){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Unable to fly to " + location + "!",
                    stream
                );
            }
        }
    }
}

namespace{

//  Fly from Central Plaza with the offsets the table holds. Same inputs as
//  the literals central_to_*() used before.
void central_to(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, TerariumFlyPoint point){
    const TerariumFlyPointInfo& fly_point = terarium_fly_point_info(point);
    const MapCursorMove& move = fly_point.from_central_plaza;
    map_move_cursor_fly(info, stream, context, move.x, move.y, move.hold, 160ms, fly_point.name);
}

}

void central_to_polar_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::POLAR_REST_AREA);
}

void central_to_polar_class1(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::POLAR_CLASSROOM_1);
}

void central_to_polar_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::POLAR_PLAZA);
}

void central_to_coastal_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::COASTAL_PLAZA);
}

void central_to_canyon_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::CANYON_PLAZA);
}

void central_to_savanna_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::SAVANNA_PLAZA);
}

void central_to_canyon_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::CANYON_REST_AREA);
}

void central_to_savanna_class(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::SAVANNA_CLASSROOM);
}

void central_to_chargestone(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    central_to(info, stream, context, TerariumFlyPoint::CHARGESTONE_CAVERN);
}

namespace{

class TerariumFlyActionsConsole : public TerariumFlyActions{
public:
    TerariumFlyActionsConsole(
        const ProgramInfo& info, VideoStream& stream, ProControllerContext& context,
        Language language
    )
        : m_info(info), m_stream(stream), m_context(context)
        , m_language(language)
    {}

    virtual bool open_map_at_central_plaza(size_t attempt) override{
        try{
            open_map_from_overworld(m_info, m_stream, m_context);
        }catch (OperationFailedException&){
            return false;
        }

        //  Same as return_to_plaza(): the top left corner one zoom level out,
        //  then back toward the center, overshooting a bit after a failure.
        pbf_press_button(m_context, BUTTON_ZL, 320ms, 800ms);
        pbf_move_left_joystick(m_context, {-1, +1}, 4000ms, 320ms);
        pbf_move_left_joystick(m_context, {+1, -1}, attempt <= 1 ? 2000ms : 2400ms, 320ms);
        pbf_press_button(m_context, BUTTON_ZR, 320ms, 800ms);

        //  Put the cursor on the Central Plaza icon one zoom level in, like
        //  fly_to_closest_pokecenter_on_map(), then zoom back out to the
        //  level the central_to_*() offsets are for.
        pbf_press_button(m_context, BUTTON_ZR, 320ms, 800ms);
        const bool found = detect_closest_flypoint_and_move_map_cursor_there(
            m_info, m_stream, m_context, FlyPoint::POKECENTER
        );
        pbf_press_button(m_context, BUTTON_ZL, 320ms, 800ms);
        return found;
    }
    virtual void move_cursor(const MapCursorMove& move) override{
        pbf_move_left_joystick(m_context, {move.x, move.y}, move.hold, 160ms);
        pbf_press_button(m_context, BUTTON_ZL, 320ms, 800ms);
    }
    virtual std::string read_fly_point_label() override{
        m_context.wait_for_all_requests();
        MapFlyPointLabelReader reader;
        VideoOverlaySet overlays(m_stream.overlay());
        reader.make_overlays(overlays);
        return reader.read(m_stream.logger(), m_language, m_stream.video().snapshot());
    }
    virtual bool fly_here() override{
        try{
            return fly_to_overworld_from_map(m_info, m_stream, m_context, true);
        }catch (UnexpectedBattleException&){
            throw;
        }catch (OperationFailedException&){
            return false;
        }
    }
    virtual void close_map() override{
        press_Bs_to_back_to_overworld(m_info, m_stream, m_context);
    }

private:
    const ProgramInfo& m_info;
    VideoStream& m_stream;
    ProControllerContext& m_context;
    Language m_language;
};

}

bool fly_to_terarium_point(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    Language language, TerariumFlyPoint destination
){
    if (language != Language::None && !OCR::ocr_language_available(language)){
        stream.log("No OCR data for the game language. Fly point names won't be checked.", COLOR_RED);
        language = Language::None;
    }
    TerariumFlyActionsConsole actions(info, stream, context, language);
    return fly_to_terarium_point(stream.logger(), actions, language, destination);
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
