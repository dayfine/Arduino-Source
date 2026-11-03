/*  AutoStory Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_BattleMenuNavigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryCapture.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

constexpr size_t PARTY_SIZE = 6;
constexpr size_t MOVE_COUNT = 4;

std::vector<std::string_view> words_of(std::string_view text){
    std::vector<std::string_view> words;
    while (!text.empty()){
        const size_t space = text.find(' ');
        const std::string_view word = text.substr(0, space);
        if (!word.empty()){
            words.push_back(word);
        }
        if (space == std::string_view::npos){
            break;
        }
        text.remove_prefix(space + 1);
    }
    return words;
}

std::optional<size_t> number_of(std::string_view text){
    if (text.empty() || text.size() > 4){
        return std::nullopt;
    }
    size_t value = 0;
    for (char c : text){
        if (c < '0' || c > '9'){
            return std::nullopt;
        }
        value = value * 10 + size_t(c - '0');
    }
    return value;
}

std::optional<Direction> direction_of(std::string_view text){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        if (text == direction_name(direction)){
            return direction;
        }
    }
    return std::nullopt;
}

Direction opposite(Direction direction){
    switch (direction){
    case Direction::NORTH:  return Direction::SOUTH;
    case Direction::EAST:   return Direction::WEST;
    case Direction::SOUTH:  return Direction::NORTH;
    case Direction::WEST:   return Direction::EAST;
    }
    return direction;
}

}


const char* destination_name(CatchDestination destination){
    switch (destination){
    case CatchDestination::PARTY:   return "party";
    case CatchDestination::PC:      return "PC";
    }
    return "?";
}

const char* capture_end_name(CaptureEnd end){
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


std::string capture_variant(const CapturePlan& plan){
    std::string variant = std::format(
        "{}{} catches={} encounters={} balls={} pace={}",
        CAPTURE_VARIANT_PREFIX, plan.species, plan.catches, plan.max_encounters, plan.max_balls, direction_name(plan.pace)
    );
    if (plan.weaken_move){
        variant += std::format(" move={} turns={}", *plan.weaken_move + 1, plan.max_weaken_turns);
    }
    return variant;
}

std::optional<CapturePlan> parse_capture_variant(std::string_view variant){
    if (!variant.starts_with(CAPTURE_VARIANT_PREFIX)){
        return std::nullopt;
    }
    const std::vector<std::string_view> words = words_of(variant.substr(CAPTURE_VARIANT_PREFIX.size()));
    if (words.empty() || words[0].find('=') != std::string_view::npos){
        return std::nullopt;
    }
    CapturePlan plan;
    plan.species = std::string(words[0]);
    for (size_t c = 1; c < words.size(); c++){
        const size_t equals = words[c].find('=');
        if (equals == std::string_view::npos){
            return std::nullopt;
        }
        const std::string_view key = words[c].substr(0, equals);
        const std::string_view value = words[c].substr(equals + 1);
        if (key == "pace"){
            const std::optional<Direction> pace = direction_of(value);
            if (!pace){
                return std::nullopt;
            }
            plan.pace = *pace;
            continue;
        }
        const std::optional<size_t> number = number_of(value);
        if (!number){
            return std::nullopt;
        }
        if (key == "catches" && *number >= 1){
            plan.catches = *number;
        }else if (key == "encounters" && *number >= 1){
            plan.max_encounters = *number;
        }else if (key == "balls" && *number >= 1){
            plan.max_balls = *number;
        }else if (key == "move" && *number >= 1 && *number <= MOVE_COUNT){
            plan.weaken_move = *number - 1;
        }else if (key == "turns"){
            plan.max_weaken_turns = *number;
        }else{
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
    step.variant = capture_variant(plan);
    return step;
}

Pokemon::StoryStep pc_step(std::string name, std::string why, const PcTransfer& transfer){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = pc_variant(transfer);
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

}

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
    if (step.variant.starts_with(CAPTURE_VARIANT_PREFIX)){
        const std::optional<CapturePlan> plan = parse_capture_variant(step.variant);
        if (!plan){
            return Pokemon::outcome_failed("bad_variant", step.variant);
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
        return Pokemon::outcome_failed(std::string("capture_") + capture_end_name(result.end), message);
    }
    if (step.variant.starts_with(PC_VARIANT_PREFIX)){
        const std::optional<PcTransfer> transfer = parse_pc_variant(step.variant);
        if (!transfer){
            return Pokemon::outcome_failed("bad_variant", step.variant);
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


ConsoleCaptureActions::ConsoleCaptureActions(ConsoleHandle& console, ProControllerContext& context, Language language)
    : m_console(console)
    , m_context(context)
    , m_language(language)
{}

bool ConsoleCaptureActions::find_encounter(Direction pace){
    //  Steps in the patch before giving up: about 10 % of steps in grass
    //  start a battle at Route 4's rates.
    constexpr size_t MAX_STEPS = 200;
    std::optional<Direction> facing;
    Direction way = pace;
    for (size_t c = 0; c < MAX_STEPS; c++){
        BattleFlashWatcher battle(COLOR_RED);
        const int ret = run_until<ProControllerContext>(
            m_console, m_context,
            [&](ProControllerContext& context){
                step_once(context, way, facing);
            },
            {battle}
        );
        if (ret == 0){
            return true;
        }
        facing = way;
        way = opposite(way);
    }
    return false;
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
    m_console.log(std::string("Capture: no nickname; it goes to the ") + destination_name(destination) + ".");
    try{
        //  B answers the nickname NO and moves the messages on.
        mash_until_overworld(m_console, m_context, 30000ms);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Capture: " + e.message(), COLOR_RED);
        return false;
    }
}



}
}
}
