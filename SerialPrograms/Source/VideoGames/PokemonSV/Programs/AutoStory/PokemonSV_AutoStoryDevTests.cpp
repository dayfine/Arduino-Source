/*  AutoStory Developer Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "CommonFramework/Exceptions/UnexpectedBattleException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonTools/StartupChecks/StartProgramChecks.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "ML/Inference/ML_YOLOv5Detector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/DateManip/NintendoSwitch_DateManip.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonSV/PokemonSV_Settings.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_IvJudgeReader.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_DirectionDetector.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "PokemonSV_AutoStory_Segment_00.h"
#include "PokemonSV_AutoStory_Segment_01.h"
#include "PokemonSV_AutoStory_Segment_02.h"
#include "PokemonSV_AutoStory_Segment_03.h"
#include "PokemonSV_AutoStory_Segment_04.h"
#include "PokemonSV_AutoStory_Segment_05.h"
#include "PokemonSV_AutoStory_Segment_06.h"
#include "PokemonSV_AutoStory_Segment_07.h"
#include "PokemonSV_AutoStory_Segment_08.h"
#include "PokemonSV_AutoStory_Segment_09.h"
#include "PokemonSV_AutoStory_Segment_10.h"
#include "PokemonSV_AutoStory_Segment_11.h"
#include "PokemonSV_AutoStory_Segment_12.h"
#include "PokemonSV_AutoStory_Segment_13.h"
#include "PokemonSV_AutoStory_Segment_14.h"
#include "PokemonSV_AutoStory_Segment_15.h"
#include "PokemonSV_AutoStory_Segment_16.h"
#include "PokemonSV_AutoStory_Segment_17.h"
#include "PokemonSV_AutoStory_Segment_18.h"
#include "PokemonSV_AutoStory_Segment_19.h"
#include "PokemonSV_AutoStory_Segment_20.h"
#include "PokemonSV_AutoStory_Segment_21.h"
#include "PokemonSV_AutoStory_Segment_22.h"
#include "PokemonSV_AutoStory_Segment_23.h"
#include "PokemonSV_AutoStory_Segment_24.h"
#include "PokemonSV_AutoStory_Segment_25.h"
#include "PokemonSV_AutoStory_Segment_26.h"
#include "PokemonSV_AutoStory_Segment_27.h"
#include "PokemonSV_AutoStory_Segment_28.h"
#include "PokemonSV_AutoStory_Segment_29.h"
#include "PokemonSV_AutoStory_Segment_30.h"
#include "PokemonSV_AutoStory_Segment_31.h"
#include "PokemonSV_AutoStory_Segment_32.h"
#include "PokemonSV_AutoStory_Segment_33.h"
#include "PokemonSV_AutoStory_Segment_34.h"
#include "PokemonSV_AutoStory_Segment_35.h"
#include "PokemonSV_AutoStory_Segment_36.h"
#include "PokemonSV_AutoStory_Segment_37.h"
#include "PokemonSV_AutoStory_Segment_38.h"
#include "PokemonSV_AutoStory_Segment_39.h"
#include "PokemonSV_AutoStory_Segment_40.h"
#include "PokemonSV_AutoStory.h"
#include <iostream>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;
using namespace ML;


void AutoStory::test_checkpoints(
    SingleSwitchProgramEnvironment& env,
    VideoStream& stream,
    ProControllerContext& context,
    int start, int end, 
    int loop, int start_loop, int end_loop
){
    AutoStoryStats& stats = env.current_stats<AutoStoryStats>();
    // EventNotificationOption& notif_status_update = NOTIFICATION_STATUS_UPDATE;
    // Language language = LANGUAGE;
    // StarterChoice starter_choice = STARTERCHOICE;
    AutoStoryOptions options{
        LANGUAGE,
        STARTERCHOICE,
        NOTIFICATION_STATUS_UPDATE,
        NOTIFICATION_ERROR_RECOVERABLE
    };

    // std::vector<std::function<void()>> checkpoint_list;
    // checkpoint_list.push_back([&](){checkpoint_00(env, context);});
    // checkpoint_list.push_back([&](){checkpoint_01(env, context, notif_status_update, notif_error_recoverable, stats, checkpoint_text, language);});
    // checkpoint_list.push_back([&](){checkpoint_02(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_03(env, context, notif_status_update, notif_error_recoverable, stats, checkpoint_text, language, starter_choice);});
    // checkpoint_list.push_back([&](){checkpoint_04(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_05(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_06(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_07(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_08(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_09(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_10(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_11(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_12(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_13(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_14(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_15(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_16(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_17(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_18(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_19(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_20(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_21(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_22(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_23(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_24(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_25(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_26(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_27(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_28(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_29(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_30(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_31(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_32(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_33(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_34(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_35(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_36(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_37(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_38(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_39(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_40(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_41(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_42(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_43(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_44(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_45(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_46(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_47(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_48(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_49(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_50(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_51(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_52(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_53(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_54(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_55(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_56(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_57(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_58(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_59(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_60(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_61(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_62(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_63(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_64(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_65(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_66(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_67(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_68(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_69(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_70(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_71(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_72(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_73(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_74(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_75(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_76(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_77(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_78(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_79(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_80(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_81(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_82(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_83(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_84(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_85(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_86(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_87(env, context, notif_status_update, notif_error_recoverable, stats, checkpoint_text, language, starter_choice);});
    // checkpoint_list.push_back([&](){checkpoint_88(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_89(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_90(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_91(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_92(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_93(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_94(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_95(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_96(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_97(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_98(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_99(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_100(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_101(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_102(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_103(env, context, notif_status_update, notif_error_recoverable, stats);});
    // checkpoint_list.push_back([&](){checkpoint_104(env, context, notif_status_update, notif_error_recoverable, stats);});
    // // checkpoint_list.push_back([&](){checkpoint_105(env, context, notif_status_update, notif_error_recoverable, stats);});
    
    
    if (end == 0){
        end = start;
    }
    stream.log("test_checkpoints: start: " + std::to_string(start) + ", end:" + std::to_string(end));
    for (int checkpoint = start; checkpoint <= end; checkpoint++){
        if (checkpoint == 0){
            stream.log("checkpoint_0");
            ALL_AUTO_STORY_CHECKPOINT_LIST()[checkpoint]->run_checkpoint(env, context, options, stats);
            // checkpoint_list[checkpoint]();
            continue;
        }
        bool has_minimap = true;
        if (checkpoint < 3){
            has_minimap = false;
        }
        
        const size_t DIGITS = 3;
        std::string number = std::to_string(checkpoint);
        if (number.size() < DIGITS){
            number = std::string(DIGITS - number.size(), '0') + number;
        }        
        if (checkpoint >= start_loop && checkpoint <= end_loop){
            for (int i = 0; i < loop; i++){
                if (i > 0){
                    try{
                        reset_game(env.program_info(), env.console, context);
                        enter_menu_from_overworld(env.program_info(), env.console, context, -1, MenuSide::NONE, has_minimap);
                        // we wait 5 seconds then save, so that the initial conditions are slightly different on each reset.
                        env.log("Wait 5 seconds.");
                        context.wait_for(Milliseconds(5 * 1000));
                    }catch (...){
                        // try one more time
                        reset_game(env.program_info(), env.console, context);
                        enter_menu_from_overworld(env.program_info(), env.console, context, -1, MenuSide::NONE, has_minimap);
                        // we wait 5 seconds then save, so that the initial conditions are slightly different on each reset.
                        env.log("Wait 5 seconds.");
                        context.wait_for(Milliseconds(5 * 1000));                        

                    }
                }
                stream.log("checkpoint_" + number + ": loop " + std::to_string(i));
                // checkpoint_list[checkpoint]();
                ALL_AUTO_STORY_CHECKPOINT_LIST()[checkpoint]->run_checkpoint(env, context, options, stats);
            } 
        }else{
            stream.log("checkpoint_" + number + ".");
            ALL_AUTO_STORY_CHECKPOINT_LIST()[checkpoint]->run_checkpoint(env, context, options, stats);
            // checkpoint_list[checkpoint]();            
        }
       
    }
    
}

void AutoStory::test_code(SingleSwitchProgramEnvironment& env, ProControllerContext& context){


    if (TEST_YOLO_BOX){
        VideoOverlaySet overlays(env.console.overlay());
        YOLOv5Detector yolo_detector(RESOURCE_PATH() + std::string(YOLO_PATH));
        
        // ImageFloatBox target_box =
        get_highest_confident_yolo_box(env, context, overlays, yolo_detector, TARGET_LABEL);

        context.wait_for(Milliseconds(1000));
        return;
    }


    if (TEST_FLYPOINT_LOCATIONS){
        print_flypoint_location(env.program_info(), env.console, context, FLYPOINT_TYPE);
        // print_flypoint_location(env.program_info(), env.console, context, FlyPoint::FAST_TRAVEL);
        return;
    }

    if (TEST_MOVE_CURSOR_OFFSET_FROM_FLYPOINT){
        move_cursor_to_position_offset_from_flypoint(env.program_info(), env.console, context, FLYPOINT_TYPE, {X_OFFSET, Y_OFFSET});

        return;
    }

    if (TEST_CURRENT_DIRECTION){
        DirectionDetector direction;
        // direction.change_direction(env.program_info(), env.console, context, DIR_RADIANS);
        VideoSnapshot snapshot = env.console.video().snapshot();
        env.console.log("current direction: " + std::to_string(direction.get_current_direction(env.console, snapshot)));
        return;
    }

    if (TEST_CHANGE_DIRECTION){
        DirectionDetector direction;
        direction.change_direction(env.program_info(), env.console, context, DIR_RADIANS);
        // VideoSnapshot snapshot = env.console.video().snapshot();
        // env.console.log("current direction: " + std::to_string(direction.get_current_direction(env.console, snapshot)));
        return;
    }    

    if (TEST_PBF_LEFT_JOYSTICK){
        pbf_move_left_joystick(context, {X_MOVE, Y_MOVE}, Milliseconds(HOLD), Milliseconds(RELEASE));
        return;
    } 

    if (TEST_PBF_JOYSTICK2){
        pbf_move_left_joystick(context, {X_MOVE2, Y_MOVE2}, Milliseconds(HOLD2), Milliseconds(RELEASE2));
        return;
    }            

    if (ENABLE_TEST_CHECKPOINTS){
        // test individual checkpoints
        test_checkpoints(env, env.console, context, START_CHECKPOINT, END_CHECKPOINT, LOOP_CHECKPOINT, START_LOOP, END_LOOP);
        GO_HOME_WHEN_DONE.run_end_of_program(context);
        return;
    }


    if (ENABLE_MISC_TEST){
        // walk_forward_while_clear_front_path(env.program_info(), env.console, context, FORWARD_TICKS);

        // overworld_navigation(env.program_info(), env.console, context, 
        //     NavigationStopCondition::STOP_MARKER, NavigationMovementMode::CLEAR_WITH_LETS_GO, 
        //     128, 0, 60, 10, false);

        DirectionDetector direction;

        


        // YOLOv5Detector yolo_detector(DOWNLOADED_RESOURCE_PATH() + "PokemonSV/AreaZero/A0-station-2.onnx");
        // move_camera_yolo(env, context, CameraAxis::Y, yolo_detector, "tree-tera", 0.294444);
        // move_camera_yolo(env, context, CameraAxis::X, yolo_detector, "tree-tera", 0.604688);
        direction.change_direction(env.program_info(), env.console, context, 3.855289);
        pbf_move_left_joystick(context, {0, +1}, 1600ms, 400ms);

        direction.change_direction(env.program_info(), env.console, context, 3.056395);
        pbf_move_left_joystick(context, {0, +1}, 2000ms, 400ms);

        direction.change_direction(env.program_info(), env.console, context, 3.749788);
        pbf_move_left_joystick(context, {0, +1}, 5440ms, 400ms);

        direction.change_direction(env.program_info(), env.console, context, 1.589021);
        pbf_move_left_joystick(context, {0, +1}, 9600ms, 400ms);
        

        return;
    }

    // context.wait_for(Milliseconds(1000000));


}



}
}
}
