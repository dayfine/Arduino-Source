/*  BDSP AutoStory - Wild battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_ReceivePokemonDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Battle.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

// Two grass rows at 240 ms each, with room to spare: the wall stops us.
constexpr Milliseconds CATCH_GRASS_WALK = 900ms;

//  Whether a wild battle has started: its black screen, menu or text.
bool wild_battle_started(VideoStream& stream, ProControllerContext& context){
    BlackScreenWatcher battle_start;
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    const int ret = wait_until(
        stream,
        context,
        1500ms,
        {
            {battle_start},
            {battle_menu},
            {battle_dialog},
        }
    );
    return ret >= 0;
}

//  One walk across the catch grass `direction`, to the wall; whether a
//  battle started.
bool walk_catch_grass(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction){
    route_turn(stream, context, direction);
    pbf_move_left_joystick(context, {0, direction == Pokemon::Direction::NORTH ? 1.0 : -1.0}, CATCH_GRASS_WALK, 200ms);
    context.wait_for_all_requests();
    return wild_battle_started(stream, context);
}

void encounter_one_wild_pokemon(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    pbf_wait(context, 1000ms);

    bool encounter_found = false;
    const auto deadline = std::chrono::steady_clock::now() + 180s;

    // Walk up and down the catch grass until a battle starts. Its three rows
    // sit between tree walls, so each walk ends at a wall, on the same tile
    // every time, after two new grass tiles. Short sideways steps drift and
    // can rock across one tile edge without entering new grass.
    while (!encounter_found && std::chrono::steady_clock::now() < deadline){
        encounter_found = walk_catch_grass(stream, context, Pokemon::Direction::NORTH) ||
            walk_catch_grass(stream, context, Pokemon::Direction::SOUTH);
    }

    if (!encounter_found){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Wild encounter: walking up and down the grass found no battle within three minutes.", stream);
    }
    stream.log("[AutoStory] Wild battle detected!", COLOR_GREEN);

    // Wait until the actual battle menu appears.
    BattleMenuWatcher confirm_menu(BattleType::STANDARD);
    if (wait_until(stream, context, 15s, {{confirm_menu}}) < 0){
        stream.log("[AutoStory] Failed to detect battle menu.", COLOR_RED);
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "[AutoStory] Failed to detect battle menu.", stream);
    }

    stream.log("[AutoStory] Battle menu detected.", COLOR_GREEN);

    context.wait_for_all_requests();
    pbf_wait(context, 500ms);
}

//  With the game's main menu open after a battle: B until it closes, the
//  overworld or a screen without it. Whether it was seen closed.
bool close_menu_after_battle(VideoStream& stream, ProControllerContext& context){
    OverworldWatcher overworld;
    MenuDetector menu_visible;
    for (size_t close = 0; close < 3; close++){
        pbf_press_button(context, BUTTON_B, 80ms, 500ms);
        context.wait_for_all_requests();
        if (wait_until(stream, context, 1s, {{overworld}}) == 0){
            return true;
        }
        BlackScreenWatcher visible(COLOR_YELLOW,
            {0.1, 0.1, 0.8, 0.8},
            100,
            10,
            BlackScreenWatcher::FinderType::GONE,
            500ms);
        if (wait_until(stream, context, 2s, {{visible}}) != 0){
            continue;
        }
        const VideoSnapshot frame = stream.video().snapshot();
        if (frame && !menu_visible.detect(frame)){
            stream.log("[Mine escape] Confirmed game menu closed; returning to navigation.", COLOR_GREEN);
            return true;
        }
    }
    stream.log("[Mine escape] Main menu detected but could not confirm it closed.", COLOR_ORANGE);
    return false;
}

bool mine_confirm_escape_via_menu(VideoStream& stream, ProControllerContext& context, BattleType battle_type){
    MenuWatcher game_menu;
    BattleMenuWatcher battle_menu(battle_type, COLOR_YELLOW);
    for (size_t attempt = 1; attempt <= 3; attempt++){
        // Do not accept the absence-based overworld detector before menu proof.
        const int state = wait_until(stream, context, 300ms, {{game_menu}, {battle_menu}});
        if (state == 1){
            stream.log("[Mine escape] Battle menu still present; retrying Run.");
            return false;
        }
        int detected = state;
        if (state != 0){
            stream.log("[Mine escape] Main menu fallback attempt " + std::to_string(attempt) + "/3.");
            detected = run_until<ProControllerContext>(stream,
                context,
                [](ProControllerContext& controller){
                    pbf_press_button(controller, BUTTON_X, 160ms, 840ms);
                    pbf_wait(controller, 3s);
                    controller.wait_for_all_requests();
                },
                {game_menu, battle_menu});
            if (detected == 1){
                return false;
            }
        }
        if (detected != 0){
            continue;
        }
        stream.log("[Mine escape] Main game menu confirmed; battle has ended.", COLOR_GREEN);
        // Close the confirmed menu before returning control to navigation.
        return close_menu_after_battle(stream, context);
    }
    return false;
}

bool mine_select_run(VideoStream& stream, ProControllerContext& context, BattleType battle_type){
    BattleMenuDetector cursor(battle_type, COLOR_YELLOW);
    size_t matching_frames = 0;
    for (size_t check = 0; check < 16; check++){
        context.wait_for_all_requests();
        const VideoSnapshot frame = stream.video().snapshot();
        if (!frame){
            return false;
        }
        const BattleMenuOption selected = battle_menu_selected_option(cursor, frame);
        stream.log(
            "[Mine escape] Battle cursor row=" + std::to_string(static_cast<int>(selected) + 1) + "; target Run=4.");
        if (selected == BattleMenuOption::RUN){
            if (++matching_frames >= 3){
                stream.log("[Mine escape] Run cursor confirmed.", COLOR_GREEN);
                return true;
            }
            pbf_wait(context, 100ms);
        }else{
            matching_frames = 0;
            if (selected == BattleMenuOption::UNKNOWN){
                // Do not send a direction or A without a known cursor.
                pbf_wait(context, 150ms);
            }else{
                const DpadPosition direction = selected == BattleMenuOption::BATTLE ? DPAD_UP : DPAD_DOWN;
                pbf_press_dpad(context, direction, 80ms, 250ms);
            }
        }
    }
    stream.log("[Mine escape] Could not confirm Run selection; A was not pressed.", COLOR_ORANGE);
    return false;
}

//  Run on the battle menu, until the battle's exit shows or `timeout`
//  runs out. Whether the exit was seen.
bool run_until_escaped(
    VideoStream& stream, ProControllerContext& context, BattleType battle_type,
    std::chrono::seconds timeout, EndBattleWatcher& battle_end
){
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline){
        pbf_press_button(context, BUTTON_B, 40ms, 200ms);
        context.wait_for_all_requests();
        BattleMenuWatcher menu_ready(battle_type, COLOR_YELLOW);
        const int ready = wait_until(stream, context, 3s, {{battle_end}, {menu_ready}});
        if (ready == 0){
            return true;
        }
        if (ready != 1 || !mine_select_run(stream, context, battle_type)){
            continue;
        }
        stream.log("[Mine escape] Run input sent; watching for black-screen exit.");
        const int result = run_until<ProControllerContext>(stream,
            context,
            [](ProControllerContext& controller){
                pbf_press_button(controller, BUTTON_A, 60ms, 0ms);
                pbf_wait(controller, 4s);
                controller.wait_for_all_requests();
            },
            {battle_end});
        if (result == 0){
            return true;
        }
    }
    return false;
}

//  B until the overworld shows; fails with `failure` after two minutes.
void mash_to_overworld(VideoStream& stream, ProControllerContext& context, const std::string& failure){
    // Mash through XP / level-up / dialog screens back to overworld.
    OverworldWatcher overworld;
    const int cleanup_ret = run_until<ProControllerContext>(
        stream, context,
        [](ProControllerContext& ctx){
            pbf_mash_button(ctx, BUTTON_B, 120000ms);
        },
        {{overworld}}
    );
    if (cleanup_ret != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, failure, stream);
    }
}

}

void fight_starly(VideoStream& stream, ProControllerContext& context){
    stream.log("[AutoStory] Starting Starly battle...", COLOR_BLUE);

    // Declare OUTSIDE the loop so accumulated detector state
    // (trigger counts, black-screen transitions) is preserved across frames.
    // STARTER type: player has no Pokeballs, so ball-count areas won't
    // be white. STARTER skips that check and only verifies status bars
    // and the Fight button color.
    BattleMenuWatcher battle_menu(BattleType::STARTER, COLOR_YELLOW);
    EndBattleWatcher  end_battle;

    while (true){
        context.wait_for_all_requests();

        // Phase 1: wait for either battle menu or battle end.
        const int ret = run_until<ProControllerContext>(
            stream, context,
            [](ProControllerContext& ctx){
                pbf_mash_button(ctx, BUTTON_B, 120000ms);
            },
            {{battle_menu}, {end_battle}}
        );
        if (ret == 1){
            stream.log("[AutoStory] Starly battle complete.", COLOR_GREEN);
            mash_to_overworld(stream, context, "Starly battle: Overworld cleanup timed out.");
            return;
        }
        if (ret != 0){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Starly battle timed out.", stream);
        }
        stream.log("[AutoStory] Battle menu detected. Selecting move...", COLOR_BLUE);
        // Select Fight, then move slot 1; loop back and mash B through the
        // animations until the next menu or the end.
        pbf_press_button(context, BUTTON_A, 80ms, 500ms);
        pbf_press_button(context, BUTTON_A, 80ms, 3000ms);
    }
}

void battle_1_pokemon(VideoStream& stream, ProControllerContext& context){
    stream.log("[Training] Finding one wild Pokemon using the catch encounter wiggle.", COLOR_BLUE);
    encounter_one_wild_pokemon(stream, context);
    // The best move on the menu, whatever the starter.
    fight_wild_battle(stream, context, "Training");
    context.wait_for_all_requests();
    stream.log("[Training] One wild battle completed.", COLOR_GREEN);
}

namespace{

//  Throws the first ball in the Bag at the wild Pokemon, up to ten times,
//  until its receipt shows. Returns whether the battle's exit was seen.
bool throw_until_caught(VideoStream& stream, ProControllerContext& context, EndBattleWatcher& battle_ended){
    ReceivePokemonDetector received_pokemon;
    for (int throw_attempt = 0; throw_attempt < 10; throw_attempt++){
        // Open bag from battle menu (X), confirm first slot (ZL).
        pbf_press_button(context, BUTTON_X, 160ms, 840ms);
        context.wait_for_all_requests();
        pbf_press_button(context, BUTTON_ZL, 160ms, 1000ms);
        context.wait_for_all_requests();

        // Wait to see if the battle menu comes back (ball failed)
        // or if the battle ends (caught or fainted).
        BattleMenuWatcher menu_back(BattleType::STANDARD, COLOR_YELLOW);
        const int throw_ret = run_until<ProControllerContext>(
            stream, context,
            [](ProControllerContext& ctx){
                pbf_mash_button(ctx, BUTTON_B, 120000ms);
            },
            {{menu_back}, {received_pokemon}, {battle_ended}}
        );

        if (throw_ret == 1 || (throw_ret == 2 && received_pokemon.received())){
            stream.log("[AutoStory] Pokemon receipt verified.", COLOR_GREEN);
            context.wait_for_all_requests();
            return throw_ret == 2;
        }
        if (throw_ret < 0 || throw_ret == 2){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Catch failed: No verified Pokemon receipt before battle ended or timed out.", stream);
        }
        stream.log(
            "[AutoStory] Ball failed, retrying (attempt " + std::to_string(throw_attempt + 2) + ")...",
            COLOR_ORANGE
        );
    }
    context.wait_for_all_requests();
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Catch failed: Exhausted ten ball attempts.", stream);
}

}

void catch_1_pokemon(
    VideoStream& stream,
    ProControllerContext& context
){
    stream.log("[AutoStory] Starting catch_1_pokemon helper...", COLOR_BLUE);

    encounter_one_wild_pokemon(stream, context);

    // Catch the pokemon.
    stream.log("[AutoStory] Throwing Poke Ball (manual bag open)...", COLOR_BLUE);
    // Preserve exit-transition state from the ball outcome through
    // receipt/experience cleanup; do not recreate it between phases.
    EndBattleWatcher battle_ended;
    const bool exit_seen = throw_until_caught(stream, context, battle_ended);
    // Receipt alone is not an overworld return: clear experience,
    // automatic move learning, dex registration and nickname screens
    // until the same completed-black-transition watcher used in battles fires.
    if (!exit_seen){
        stream.log("[Catch cleanup] Receipt confirmed; waiting for battle exit transition.");
        const int cleanup_ret = run_until<ProControllerContext>(stream, context,
            [](ProControllerContext& ctx){ pbf_mash_button(ctx, BUTTON_B, 120000ms); },
            {{battle_ended}});
        if (cleanup_ret != 0){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Catch cleanup: Battle exit transition did not finish.", stream);
        }
    }
    context.wait_for_all_requests();
    stream.log("[Catch cleanup] Battle exit transition confirmed; checking stable overworld.");
    confirm_battle_overworld(stream, context, "Catch cleanup");
    stream.log("[Catch cleanup] Verified receipt, completed battle exit, and four seconds of clear overworld.", COLOR_GREEN);
}

void escape_wild_battle(VideoStream& stream,
    ProControllerContext& context,
    BattleType battle_type,
    std::chrono::seconds timeout){
    EndBattleWatcher battle_end;
    bool exit_transition = run_until_escaped(stream, context, battle_type, timeout, battle_end);
    if (!exit_transition && wait_until(stream, context, 10s, {{battle_end}}) == 0){
        exit_transition = true;
    }
    if (exit_transition){
        // EndBattleWatcher requires black followed by a visible screen.
        stream.log("[Mine escape] Black-screen exit transition confirmed.", COLOR_GREEN);
        pbf_wait(context, 800ms);
        context.wait_for_all_requests();
        return;
    }
    // Only probe X after normal escape attempts and the final transition wait.
    // Absence of battle UI by itself is not evidence of an overworld return.
    stream.log("[Mine escape] Exit transition missed; trying late main-menu fallback.");
    if (mine_confirm_escape_via_menu(stream, context, battle_type)){
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "escape_wild_battle: Exit transition and late main-menu fallback could not confirm escape.",
        stream);
}

//  STARTER's check of the menu (its Battle button) holds for any party:
//  STANDARD also reads the party's balls, which a lone starter does not
//  match, so Route 201's first wild battle never saw its menu (r34).
void run_from_wild_battle(VideoStream& stream, ProControllerContext& context){
    escape_wild_battle(stream, context, BattleType::STARTER);
}



}
}
}
