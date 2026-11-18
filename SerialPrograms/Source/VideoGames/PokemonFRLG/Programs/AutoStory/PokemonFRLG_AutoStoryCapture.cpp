/*  AutoStory Capture Logic
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryPacing.h"

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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
