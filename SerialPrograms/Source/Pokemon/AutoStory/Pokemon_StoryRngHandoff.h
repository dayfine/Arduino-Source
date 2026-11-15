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
 *    where the story goes on; a failure is the reason it does not. A game
 *    whose program can also end where the user takes over (FRLG: the target
 *    hit but not shiny, the attempts spent) gives that end a user_turn, which
 *    is a stop and not an error. What the story runs after a continuation
 *    (FRLG: the rows after a shiny) is the game's, built from the call.
 *  - A failed RUN stops the program. It never retries the row: the program's
 *    own reset loop is the retry, and a reload of the save would lose a
 *    shiny it caught.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRngHandoff_H
#define PokemonAutomation_Pokemon_StoryRngHandoff_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a handoff row does with its RNG program.
enum class RngHandoffMode{
    OFF,
    STOP,
    RUN,
};

//  A handoff's two options.
struct RngHandoffOptions{
    bool stop = false;
    bool run = false;
};

//  The mode for a handoff's two options: RUN takes precedence over STOP.
RngHandoffMode rng_handoff_mode(const RngHandoffOptions& options);


//  How an RNG program's call ended, as the story needs it.
struct RngHandoffResult{
    //  Empty: the game stands where the story goes on. Otherwise why it does not.
    std::string failure;
    //  What a success did, for the log: the resets used, the target hit.
    std::string summary;
    //  Not empty (and no failure): the program ended where the user takes
    //  over, with this for them. The run stops without an error report.
    std::string user_turn;

    bool succeeded() const;
};

//  The call a handoff row runs in place of its steps.
using RngHandoffCall = std::function<RngHandoffResult()>;

//  Runs `call` for the RNG program `program` ("Starter RNG"). A success is
//  logged with its summary and returned as an ok Outcome. A failure throws
//  OperationFailedException with an error report, so that no row retry
//  reruns the program's reset loop.
Outcome run_rng_handoff(Logger& logger, const std::string& program, const RngHandoffCall& call);

//  As run_rng_handoff(), for a program whose call may end in the user's turn:
//  returns that message, which is logged, and the run stops there. Returns
//  nothing when the game stands where the story goes on.
std::optional<std::string> run_rng_handoff_or_user_turn(
    Logger& logger, const std::string& program, const RngHandoffCall& call
);

//  A run that stops for the user to run an RNG program, as the game's program
//  reports it: `message` (what to do) and then `lines` (the RNG plan's
//  attempts), each logged and passed to `show` (the overlay's log). Returns
//  the whole text, one line each, for the program's finish notification.
std::string report_rng_handoff_stop(
    Logger& logger, const std::string& message, const std::vector<std::string>& lines,
    const std::function<void(const std::string&)>& show
);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
