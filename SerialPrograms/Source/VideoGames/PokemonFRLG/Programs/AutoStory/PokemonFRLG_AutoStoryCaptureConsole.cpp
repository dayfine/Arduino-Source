/*  AutoStory Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_BattleMenuNavigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BikeSpinPresses.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_AutoStoryPacing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;

namespace{

Direction opposite(Direction direction){
    switch (direction){
    case Direction::NORTH:  return Direction::SOUTH;
    case Direction::EAST:   return Direction::WEST;
    case Direction::SOUTH:  return Direction::NORTH;
    case Direction::WEST:   return Direction::EAST;
    }
    return direction;
}

} // namespace


std::optional<std::optional<Pokemon::StoryFailure>> run_storage_game_step(
    std::vector<size_t>& protected_slots,
    Logger& logger, VideoStream* stream, const StoryStep& step,
    CaptureActions& capture, PcStorageActions& pc
){
    if (step.action == CAPTURE_ACTION){
        const std::optional<CapturePlan> plan = capture_plan_of(step.args);
        if (!plan){
            return frlg_failure("bad_args", step.action);
        }
        const CaptureResult result = run_capture(logger, capture, *plan, pc.read_party().size());
        const std::string message = capture_message(*plan, result);
        logger.log(message, result.end == CaptureEnd::CAUGHT ? COLOR_BLACK : COLOR_RED);
        if (result.end == CaptureEnd::SHINY){
            //  Not a failed StoryFailure, which would reload the save.
            if (stream != nullptr){
                throw ProgramFinishedException(message, *stream);
            }
            throw ProgramFinishedException(message);
        }
        if (result.end == CaptureEnd::CAUGHT){
            return std::optional<Pokemon::StoryFailure>(std::nullopt);
        }
        return frlg_failure(std::string("capture_") + std::string(capture_end_name(result.end)), message);
    }
    if (step.action == PC_ACTION){
        const std::optional<PcTransfer> transfer = pc_transfer_of(step.args);
        if (!transfer){
            return frlg_failure("bad_args", step.action);
        }
        const PcResult result = run_pc_transfer(logger, pc, *transfer);
        if ((result.end == PcEnd::DONE || result.end == PcEnd::SKIPPED) && transfer->protect && result.party_slot
            && std::ranges::find(protected_slots, *result.party_slot) == protected_slots.end()
        ){
            protected_slots.push_back(*result.party_slot);
            logger.log(std::format("PC: party slot {} rides along; it is never sent into a battle.", *result.party_slot + 1));
        }
        switch (result.end){
        case PcEnd::DONE:
        case PcEnd::SKIPPED:
            return std::optional<Pokemon::StoryFailure>(std::nullopt);
        case PcEnd::REFUSED:
            return frlg_failure("pc_refused", result.detail);
        case PcEnd::FAILED:
            return frlg_failure("pc_failed", result.detail);
        }
    }
    return std::nullopt;
}




OverworldWalkSteps::BikeSelectCalls console_bike_select(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    OverworldWalkSteps::BikeSelectCalls calls;
    calls.press_select = [&console, &context]{
        console.log("Pacing: SELECT (MINUS) off the Bicycle.");
        pbf_press_button(context, BUTTON_MINUS, MovementMechanics::BIKE_SELECT_PRESS, MovementMechanics::BIKE_SELECT_SETTLE);
    };
    calls.riding = [&console, &context, player]{
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        return screen && OverworldWalkSteps::screen_shows_riding(*screen.frame, player);
    };
    return calls;
}

ConsolePatchSteps::ConsolePatchSteps(
    ConsoleHandle& console, ProControllerContext& context, PlayerGender player, Direction pace, PaceSpot& spot,
    const PaceTrainerBattle& trainer_battle
)
    : m_console(console)
    , m_context(context)
    , m_player(player)
    , m_pace(pace)
    , m_spot(spot)
    , m_trainer_battle(trainer_battle)
{}

bool ConsolePatchSteps::step(){
    const Direction way = m_spot.displaced ? opposite(m_pace) : m_pace;
    BattleFlashWatcher battle(COLOR_RED);
    const int ret = run_until<ProControllerContext>(
        m_console, m_context,
        [&](ProControllerContext& context){
            step_once(m_console, context, way, m_spot.facing, m_player);
        },
        {battle}
    );
    m_spot = {.displaced = !m_spot.displaced, .facing = way};
    return ret == 0;
}

bool ConsolePatchSteps::battle_shows(){
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_console.video().snapshot();
    return screen && BattleScreenDetector(COLOR_RED).detect(screen);
}

bool ConsolePatchSteps::overworld_shows(){
    return PokemonFRLG::overworld_shows(m_console, m_context);
}

bool ConsolePatchSteps::message_shows(){
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_console.video().snapshot();
    return screen && WhiteDialogDetector(COLOR_RED).detect(screen);
}

std::string ConsolePatchSteps::fight_trainer(){
    m_trainer_battle();
    //  Back on the patch's tile, facing as the walk back left it.
    m_spot = {};
    return "a trainer";
}

void ConsolePatchSteps::dump_screen(std::string_view label){
    m_context.wait_for_all_requests();
    dump_debug_image(m_console, "AutoStory/pace", std::string(label), m_console.video().snapshot());
}


ConsoleCaptureActions::ConsoleCaptureActions(
    ConsoleHandle& console, ProControllerContext& context, Language language, PlayerGender player,
    PaceTrainerBattle trainer_battle
)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_player(player)
    , m_trainer_battle(std::move(trainer_battle))
{}

bool ConsoleCaptureActions::find_encounter(Direction pace){
    ConsolePatchSteps steps(m_console, m_context, m_player, pace, m_spot, m_trainer_battle);
    return find_paced_encounter(m_console.logger(), console_bike_select(m_console, m_context, m_player), steps);
}

EncounterSeen ConsoleCaptureActions::read_encounter(){
    EncounterSeen seen;
    //  The shiny sound plays as the wild Pokemon slides in; the lead is
    //  sent out only if none was heard.
    seen.shiny = handle_encounter(m_console, m_context, true);
    if (seen.shiny){
        return seen;
    }
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_console.video().snapshot();
    seen.species = WildEncounterReader(COLOR_RED).read_encounter(m_console.logger(), m_language, screen, {}).name;
    return seen;
}

bool ConsoleCaptureActions::flee(){
    try{
        flee_battle(m_console, m_context);
        //  The pacing counts its taps: one lost to the battle's exit
        //  would put it a tile off.
        wait_out_battle_exit(m_console, m_context);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Capture: " + e.message(), COLOR_RED);
        return false;
    }
}

FoeHp ConsoleCaptureActions::read_foe_hp(){
    //  No reader for the wild Pokemon's HP bar yet.
    return FoeHp::UNKNOWN;
}

TurnEnd ConsoleCaptureActions::attack(size_t move){
    //  Only called with the HP bar read green, which the console can't read
    //  yet (read_foe_hp()): not run on the console.
    if (!move_cursor_to_option(m_console, m_context, BattleMenuOption::FIGHT)){
        return TurnEnd::FAILED;
    }
    pbf_press_button(m_context, BUTTON_A, 160ms, 640ms);
    if (!move_cursor_to_option(m_console, m_context, BattleMoveOption(move))){
        return TurnEnd::FAILED;
    }
    pbf_press_button(m_context, BUTTON_A, 160ms, 640ms);
    BattleMenuWatcher menu(COLOR_RED);
    const int ret = run_until<ProControllerContext>(
        m_console, m_context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 30000ms);
        },
        {menu}
    );
    return ret == 0 ? TurnEnd::MENU : TurnEnd::FAILED;
}

ThrowEnd ConsoleCaptureActions::throw_ball(){
    if (!move_cursor_to_option(m_console, m_context, BattleMenuOption::BAG)){
        return ThrowEnd::FAILED;
    }
    pbf_press_button(m_context, BUTTON_A, 160ms, 1200ms);
    m_context.wait_for_all_requests();
    if (!open_ball_pocket()){
        m_console.log("Capture: no Poke Ball found in the bag.", COLOR_RED);
        pbf_mash_button(m_context, BUTTON_B, 2000ms);
        m_context.wait_for_all_requests();
        return ThrowEnd::NO_BALLS;
    }
    //  The top row, USE, the throw.
    pbf_press_button(m_context, BUTTON_A, 160ms, 640ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 640ms);
    m_context.wait_for_all_requests();
    return wait_for_throw();
}

bool ConsoleCaptureActions::open_ball_pocket(){
    //  The bag opens on the pocket used last: Items, Key Items and Poke
    //  Balls, RIGHT from one to the next.
    for (size_t c = 0; c < 3; c++){
        VideoSnapshot screen = m_console.video().snapshot();
        const std::vector<BagRow> rows = read_bag_pocket(m_console.logger(), m_language, screen);
        if (!rows.empty() && rows.front().item == "poke-ball"){
            return true;
        }
        pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 640ms);
        m_context.wait_for_all_requests();
    }
    return false;
}

ThrowEnd ConsoleCaptureActions::wait_for_throw(){
    //  Either the battle menu (it broke free) or, after "Gotcha!" and the
    //  Pokedex page, "Give a nickname?". B moves the messages on; one that
    //  answers the nickname NO leads to the overworld.
    const WallClock deadline = current_time() + 60000ms;
    for (size_t polls = 1; current_time() < deadline; polls++){
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_console.video().snapshot();
        if (screen && yes_no_cursor(screen)){
            return ThrowEnd::CAUGHT;
        }
        if (screen && BattleMenuDetector(COLOR_RED).detect(screen)){
            return ThrowEnd::BROKE_FREE;
        }
        if (polls % 4 != 0){
            pbf_wait(m_context, 500ms);
            continue;
        }
        if (overworld_shows(m_console, m_context)){
            return ThrowEnd::CAUGHT;
        }
        pbf_press_button(m_context, BUTTON_B, 160ms, 340ms);
    }
    return ThrowEnd::FAILED;
}

bool ConsoleCaptureActions::finish_catch(CatchDestination destination){
    m_console.log(std::string("Capture: no nickname; it goes to the ") + std::string(destination_name(destination)) + ".");
    try{
        //  B answers the nickname NO and moves the messages on.
        mash_until_overworld(m_console, m_context, 30000ms);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Capture: " + e.message(), COLOR_RED);
        return false;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
