/*  Story RNG Handoff
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The part of an AutoStory run that hands the game to an RNG program (BDSP
 *  Starter RNG, FRLG Starter RNG and Gift RNG) and takes it back. A game
 *  supplies the program's call; the contract around it is shared:
 *
 *  - A handoff row has a mode: OFF (the story plays it by hand), STOP (the run
 *    ends after the row's handoff save, for the user to run the RNG program)
 *    or RUN (the story calls the RNG program itself and continues).
 *  - The call ends in an RngHandoffResult: no failure means the game stands
 *    where the story goes on; a failure is the reason it does not.
 *  - A failed RUN stops the program. It never retries the row: the program's
 *    own reset loop is the retry, and a reload of the save would lose a
 *    shiny it caught.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRngHandoff_H
#define PokemonAutomation_Pokemon_StoryRngHandoff_H

#include <functional>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


enum class RngHandoffMode{
    OFF,
    STOP,
    RUN,
};

//  The mode for a handoff's two options: RUN takes precedence over STOP.
RngHandoffMode rng_handoff_mode(bool stop, bool run);


//  How an RNG program's call ended, as the story needs it.
struct RngHandoffResult{
    //  Empty: the game stands where the story goes on. Otherwise why it does not.
    std::string failure;
    //  What a success did, for the log: the resets used, the target hit.
    std::string summary;

    bool succeeded() const;
};

//  The call a handoff row runs in place of its steps.
using RngHandoffCall = std::function<RngHandoffResult()>;

//  Runs `call` for the RNG program `program` ("Starter RNG"). A success is
//  logged with its summary and returned as an ok Outcome. A failure throws
//  OperationFailedException with an error report, so that no row retry
//  reruns the program's reset loop.
Outcome run_rng_handoff(Logger& logger, const std::string& program, const RngHandoffCall& call);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
