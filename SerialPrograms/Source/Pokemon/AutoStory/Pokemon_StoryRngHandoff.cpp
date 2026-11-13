/*  Story RNG Handoff
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryRngHandoff.h"

namespace PokemonAutomation{
namespace Pokemon{


RngHandoffMode rng_handoff_mode(bool stop, bool run){
    if (run){
        return RngHandoffMode::RUN;
    }
    return stop ? RngHandoffMode::STOP : RngHandoffMode::OFF;
}

bool RngHandoffResult::succeeded() const{
    return failure.empty();
}

std::optional<std::string> run_rng_handoff_or_user_turn(
    Logger& logger, const std::string& program, const RngHandoffCall& call
){
    const RngHandoffResult result = call();
    if (!result.succeeded()){
        throw_and_log<OperationFailedException>(logger, ErrorReportMode::SEND_ERROR_REPORT, result.failure);
    }
    if (!result.user_turn.empty()){
        logger.log(program + " stopped for the user: " + result.user_turn, COLOR_BLUE);
        return result.user_turn;
    }
    logger.log(program + " succeeded: " + result.summary, COLOR_BLUE);
    return std::nullopt;
}

Outcome run_rng_handoff(Logger& logger, const std::string& program, const RngHandoffCall& call){
    const std::optional<std::string> user_turn = run_rng_handoff_or_user_turn(logger, program, call);
    if (user_turn){
        //  A story action has no stop to give the user's turn to.
        throw_and_log<OperationFailedException>(logger, ErrorReportMode::SEND_ERROR_REPORT, *user_turn);
    }
    return outcome_ok();
}


} // namespace Pokemon
} // namespace PokemonAutomation
