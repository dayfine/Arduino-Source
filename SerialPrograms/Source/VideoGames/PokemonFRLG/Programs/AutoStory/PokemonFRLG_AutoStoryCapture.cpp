/*  AutoStory Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

constexpr size_t PARTY_SIZE = 6;
constexpr size_t MOVE_COUNT = 4;

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


std::string_view destination_name(CatchDestination destination){
    switch (destination){
    case CatchDestination::PARTY:   return "party";
    case CatchDestination::PC:      return "PC";
    }
    return "?";
}

std::string_view capture_end_name(CaptureEnd end){
    switch (end){
    case CaptureEnd::CAUGHT:            return "caught";
    case CaptureEnd::OUT_OF_ENCOUNTERS: return "out_of_encounters";
    case CaptureEnd::OUT_OF_BALLS:      return "out_of_balls";
    case CaptureEnd::NO_ENCOUNTER:      return "no_encounter";
    case CaptureEnd::SHINY:             return "shiny";
    case CaptureEnd::PLAYER_FAINTED:    return "player_fainted";
    case CaptureEnd::FAILED:            return "failed";
    }
    return "?";
}


Pokemon::StoryActionArgs capture_args(const CapturePlan& plan){
    Pokemon::StoryActionArgs args{
        {"species", plan.species},
        {"catches", int64_t(plan.catches)},
        {"encounters", int64_t(plan.max_encounters)},
        {"balls", int64_t(plan.max_balls)},
        {"pace", plan.pace},
        {"turns", int64_t(plan.max_weaken_turns)},
    };
    if (plan.weaken_move){
        args.emplace("move", int64_t(*plan.weaken_move + 1));
    }
    return args;
}

std::optional<CapturePlan> capture_plan_of(const Pokemon::StoryActionArgs& args){
    const std::string* species = game_arg<std::string>(args, "species");
    const std::optional<size_t> catches = number_arg(args, "catches", 1, 999);
    const std::optional<size_t> encounters = number_arg(args, "encounters", 1, 9999);
    const std::optional<size_t> balls = number_arg(args, "balls", 1, 999);
    const std::optional<size_t> turns = number_arg(args, "turns", 0, 99);
    const Direction* pace = game_arg<Direction>(args, "pace");
    if (species == nullptr || species->empty() || !catches || !encounters || !balls || !turns || pace == nullptr){
        return std::nullopt;
    }
    CapturePlan plan{
        .species = *species, .catches = *catches, .max_encounters = *encounters, .max_balls = *balls,
        .pace = *pace, .max_weaken_turns = *turns,
    };
    if (args.contains("move")){
        plan.weaken_move = counted_arg(args, "move", MOVE_COUNT);
        if (!plan.weaken_move){
            return std::nullopt;
        }
    }
    return plan;
}

Pokemon::StoryStep capture_step(std::string name, std::string why, const CapturePlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = CAPTURE_ACTION;
    step.args = capture_args(plan);
    return step;
}

Pokemon::StoryStep pc_step(std::string name, std::string why, const PcTransfer& transfer){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = PC_ACTION;
    step.args = pc_args(transfer);
    return step;
}


namespace{

//  What a turn of the target's battle left.
enum class Turn{
    AGAIN,
    //  The battle is over; the capture goes on.
    BATTLE_OVER,
    //  The capture is over: its result is set.
    CAPTURE_OVER,
};

//  One capture under way: its counts, and the battles it plays.
class CaptureRun{
public:
    CaptureRun(Logger& logger, CaptureActions& actions, const CapturePlan& plan, size_t party_size)
        : m_logger(logger)
        , m_actions(actions)
        , m_plan(plan)
        , m_party(party_size)
    {}

    CaptureResult run(){
        m_logger.log(std::format("Capture: {} {}, party of {}.", m_plan.catches, m_plan.species, m_party));
        while (m_result.caught.size() < m_plan.catches){
            if (!next_encounter() || !battle()){
                return m_result;
            }
        }
        m_result.end = CaptureEnd::CAUGHT;
        return m_result;
    }

private:
    //  Ends the capture as `end`. False, for the caller to return.
    bool end(CaptureEnd end, std::string detail = ""){
        m_result.end = end;
        m_result.detail = std::move(detail);
        return false;
    }
    //  RUN, then ends the capture as `end`.
    bool run_and_end(CaptureEnd end, std::string detail = ""){
        if (!m_actions.flee()){
            return this->end(CaptureEnd::FAILED, "couldn't run from " + m_plan.species);
        }
        return this->end(end, std::move(detail));
    }

    //  Into the next wild battle, if the bounds allow one. False if the
    //  capture ends.
    bool next_encounter(){
        if (m_result.encounters >= m_plan.max_encounters){
            return end(CaptureEnd::OUT_OF_ENCOUNTERS);
        }
        if (m_result.balls_thrown >= m_plan.max_balls){
            return end(CaptureEnd::OUT_OF_BALLS);
        }
        if (!m_actions.find_encounter(m_plan.pace)){
            return end(CaptureEnd::NO_ENCOUNTER);
        }
        m_result.encounters++;
        return true;
    }

    //  One wild battle, from its start to the overworld. False if the
    //  capture ends.
    bool battle(){
        const EncounterSeen seen = m_actions.read_encounter();
        if (seen.shiny){
            //  Nothing more is pressed: a person decides.
            m_logger.log("Capture: a shiny! Stopping in its battle.", COLOR_BLUE);
            return end(CaptureEnd::SHINY, seen.species);
        }
        if (seen.species == m_plan.species){
            m_logger.log(std::format("Capture: encounter {}: {}.", m_result.encounters, seen.species));
            return target_battle();
        }
        const std::string species = seen.species.empty() ? "an unread species" : seen.species;
        m_logger.log(std::format("Capture: encounter {}: {}, running.", m_result.encounters, species));
        if (!m_actions.flee()){
            return end(CaptureEnd::FAILED, "couldn't run from " + species);
        }
        return true;
    }

    //  The target's battle, from the battle menu: the weakening move while
    //  its HP bar is green, so the move can't knock it out, then balls.
    //  False if the capture ends.
    bool target_battle(){
        size_t weakened = 0;
        for (size_t turn = 0; turn < CAPTURE_MAX_TURNS; turn++){
            if (m_result.balls_thrown >= m_plan.max_balls){
                return run_and_end(CaptureEnd::OUT_OF_BALLS);
            }
            const bool weaken = m_plan.weaken_move && weakened < m_plan.max_weaken_turns
                && m_actions.read_foe_hp() == FoeHp::GREEN;
            weakened += weaken ? 1 : 0;
            const Turn end = weaken ? attack() : throw_ball();
            if (end != Turn::AGAIN){
                return end == Turn::BATTLE_OVER;
            }
        }
        return run_and_end(CaptureEnd::FAILED, std::format("not caught in {} turns", CAPTURE_MAX_TURNS));
    }

    Turn attack(){
        switch (m_actions.attack(*m_plan.weaken_move)){
        case TurnEnd::MENU:
            return Turn::AGAIN;
        case TurnEnd::FOE_FAINTED:
            m_logger.log("Capture: the target fainted.", COLOR_RED);
            return Turn::BATTLE_OVER;
        case TurnEnd::PLAYER_FAINTED:
            end(CaptureEnd::PLAYER_FAINTED);
            return Turn::CAPTURE_OVER;
        case TurnEnd::FAILED:
            break;
        }
        end(CaptureEnd::FAILED, "the turn didn't end at the battle menu");
        return Turn::CAPTURE_OVER;
    }

    Turn throw_ball(){
        switch (m_actions.throw_ball()){
        case ThrowEnd::CAUGHT:
            m_result.balls_thrown++;
            return caught();
        case ThrowEnd::BROKE_FREE:
            m_result.balls_thrown++;
            return Turn::AGAIN;
        case ThrowEnd::NO_BALLS:
            run_and_end(CaptureEnd::OUT_OF_BALLS, "the bag has no Poke Ball");
            return Turn::CAPTURE_OVER;
        case ThrowEnd::PLAYER_FAINTED:
            end(CaptureEnd::PLAYER_FAINTED);
            return Turn::CAPTURE_OVER;
        case ThrowEnd::FAILED:
            break;
        }
        end(CaptureEnd::FAILED, "the throw didn't end in a catch or at the battle menu");
        return Turn::CAPTURE_OVER;
    }

    //  To the party, or to the PC when the party is full.
    Turn caught(){
        const CatchDestination destination = m_party < PARTY_SIZE ? CatchDestination::PARTY : CatchDestination::PC;
        if (!m_actions.finish_catch(destination)){
            end(CaptureEnd::FAILED, "the overworld didn't come back after the catch");
            return Turn::CAPTURE_OVER;
        }
        m_party += destination == CatchDestination::PARTY ? 1 : 0;
        m_result.caught.push_back(destination);
        m_logger.log(std::format("Capture: caught {}, to the {}.", m_plan.species, destination_name(destination)));
        return Turn::BATTLE_OVER;
    }

    Logger& m_logger;
    CaptureActions& m_actions;
    const CapturePlan& m_plan;
    size_t m_party;
    CaptureResult m_result;
};

} // namespace

CaptureResult run_capture(Logger& logger, CaptureActions& actions, const CapturePlan& plan, size_t party_size){
    return CaptureRun(logger, actions, plan, party_size).run();
}


std::string capture_message(const CapturePlan& plan, const CaptureResult& result){
    const std::string tally = std::format(
        "{} encounters, {} balls thrown, {} of {} caught",
        result.encounters, result.balls_thrown, result.caught.size(), plan.catches
    );
    switch (result.end){
    case CaptureEnd::CAUGHT:{
        std::string where;
        for (CatchDestination destination : result.caught){
            where += (where.empty() ? "" : ", ") + std::string(destination_name(destination));
        }
        return std::format("Capture: caught {} {} ({}): {}.", plan.catches, plan.species, where, tally);
    }
    case CaptureEnd::SHINY:
        return std::format(
            "Capture: a shiny {} in encounter {}. Stopped in its battle without running, for you to decide: "
            "catch it or run by hand, then save.",
            result.detail.empty() ? "Pokemon" : result.detail, result.encounters
        );
    case CaptureEnd::OUT_OF_ENCOUNTERS:
        return std::format("Capture of {}: out of encounters: {}.", plan.species, tally);
    case CaptureEnd::OUT_OF_BALLS:
        return std::format(
            "Capture of {}: out of balls{}: {}.",
            plan.species, result.detail.empty() ? "" : " (" + result.detail + ")", tally
        );
    case CaptureEnd::NO_ENCOUNTER:
        return std::format("Capture of {}: no wild battle started: {}.", plan.species, tally);
    case CaptureEnd::PLAYER_FAINTED:
        return std::format("Capture of {}: the player's Pokemon fainted: {}.", plan.species, tally);
    case CaptureEnd::FAILED:
        return std::format("Capture of {}: {}: {}.", plan.species, result.detail, tally);
    }
    return "Capture: ?";
}


std::optional<Pokemon::Outcome> run_storage_game_step(
    Logger& logger, VideoStream* stream, const Pokemon::StoryStep& step,
    CaptureActions& capture, PcStorageActions& pc
){
    if (step.action == CAPTURE_ACTION){
        const std::optional<CapturePlan> plan = capture_plan_of(step.args);
        if (!plan){
            return Pokemon::outcome_failed("bad_args", step.action);
        }
        const CaptureResult result = run_capture(logger, capture, *plan, pc.read_party().size());
        const std::string message = capture_message(*plan, result);
        logger.log(message, result.end == CaptureEnd::CAUGHT ? COLOR_BLACK : COLOR_RED);
        if (result.end == CaptureEnd::SHINY){
            //  Not a failed Outcome, which would reload the save.
            if (stream != nullptr){
                throw ProgramFinishedException(message, *stream);
            }
            throw ProgramFinishedException(message);
        }
        if (result.end == CaptureEnd::CAUGHT){
            return Pokemon::outcome_ok();
        }
        return Pokemon::outcome_failed(std::string("capture_") + std::string(capture_end_name(result.end)), message);
    }
    if (step.action == PC_ACTION){
        const std::optional<PcTransfer> transfer = pc_transfer_of(step.args);
        if (!transfer){
            return Pokemon::outcome_failed("bad_args", step.action);
        }
        const PcResult result = run_pc_transfer(logger, pc, *transfer);
        switch (result.end){
        case PcEnd::DONE:
        case PcEnd::SKIPPED:
            return Pokemon::outcome_ok();
        case PcEnd::REFUSED:
            return Pokemon::outcome_failed("pc_refused", result.detail);
        case PcEnd::FAILED:
            return Pokemon::outcome_failed("pc_failed", result.detail);
        }
    }
    return std::nullopt;
}


bool pace_until_battle(Logger& logger, PatchSteps& steps, size_t max_steps){
    for (size_t c = 0; c < max_steps; c++){
        if (steps.step()){
            return true;
        }
        //  The flash can be missed (console B, 2026-11-03): the battle's
        //  first text then waits for A, and the D-pad does nothing. Found a
        //  step late, the player stays where the battle started, but the
        //  step after it still counts as taken.
        if (steps.battle_shows()){
            logger.log(std::format("Pacing: a battle shows after step {}; its flash was missed.", c + 1), COLOR_ORANGE);
            return true;
        }
        //  Console B, 2026-11-03 (m63): Camper Shane on Route 24 saw the
        //  player pace in his sight and walked up; his message stayed up
        //  while the rest of the steps paced in vain.
        if (steps.message_shows()){
            const std::string trainer = steps.fight_trainer();
            logger.log(std::format("Pacing: trainer battle during pacing: {}, after step {}.", trainer, c + 1), COLOR_ORANGE);
        }
    }
    if (steps.overworld_shows()){
        return false;
    }
    if (steps.battle_shows()){
        logger.log(std::format("Pacing: a battle shows after the last of {} steps; its flash was missed.", max_steps), COLOR_ORANGE);
        return true;
    }
    logger.log(std::format("Pacing: no battle in {} steps, and the overworld doesn't show.", max_steps), COLOR_RED);
    steps.dump_screen("no-battle-not-overworld");
    return false;
}


ConsolePatchSteps::ConsolePatchSteps(
    ConsoleHandle& console, ProControllerContext& context, Direction pace, PaceSpot& spot,
    const PaceTrainerBattle& trainer_battle
)
    : m_console(console)
    , m_context(context)
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
            step_once(context, way, m_spot.facing);
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
    ConsoleHandle& console, ProControllerContext& context, Language language,
    PaceTrainerBattle trainer_battle
)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_trainer_battle(std::move(trainer_battle))
{}

bool ConsoleCaptureActions::find_encounter(Direction pace){
    PaceSpot spot;
    ConsolePatchSteps steps(m_console, m_context, pace, spot, m_trainer_battle);
    return pace_until_battle(m_console.logger(), steps, PACE_MAX_STEPS);
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
