/*  AutoStory Capture Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game steps a capture takes, injected so its decisions run in tests,
 *  how it ends, and its bounds.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCaptureRun_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCaptureRun_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"
#include "PokemonFRLG_AutoStoryCaptureTurns.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The game steps a capture takes, injected so its decisions run in tests.
class CaptureActions{
public:
    virtual ~CaptureActions() = default;

    //  From the overworld, in the patch: steps `pace` and back until a wild
    //  battle starts. False if none did within its bound.
    virtual bool find_encounter(Pokemon::Direction pace) = 0;
    //  From a wild battle's start: whether it is shiny and, if not, its
    //  species, read at the battle menu. A shiny is left as it shows.
    virtual EncounterSeen read_encounter() = 0;
    //  From the battle menu: RUN, until the overworld is back. False if it
    //  didn't get away.
    virtual bool flee() = 0;
    //  At the battle menu: the wild Pokemon's HP bar.
    virtual FoeHp read_foe_hp() = 0;
    //  From the battle menu: FIGHT, the move `move` (0-3), the turn.
    virtual TurnEnd attack(size_t move) = 0;
    //  From the battle menu: BAG, a Poke Ball, the throw.
    virtual ThrowEnd throw_ball() = 0;
    //  From the catch's first message: no nickname, and the messages to the
    //  overworld, the last saying it went to `destination`. False if the
    //  overworld didn't come back.
    virtual bool finish_catch(CatchDestination destination) = 0;
};

//  How an encounter capture attempt concluded.
enum class CaptureEnd{
    CAUGHT,
    OUT_OF_ENCOUNTERS,
    OUT_OF_BALLS,
    NO_ENCOUNTER,
    //  A shiny showed: stopped in its battle, nothing pressed.
    SHINY,
    PLAYER_FAINTED,
    FAILED,
};
//  The human-readable name of a capture end outcome.
std::string_view capture_end_name(CaptureEnd end);

//  The summary result of a capture attempt.
struct CaptureResult{
    CaptureEnd end = CaptureEnd::FAILED;
    size_t encounters = 0;
    size_t balls_thrown = 0;
    //  Where each catch went, in order.
    std::vector<CatchDestination> caught = {};
    //  SHINY: its species, if read. FAILED: what failed.
    std::string detail = {};
};

//  Turns of one target's battle before the step gives up on it.
constexpr size_t CAPTURE_MAX_TURNS = 20;

//  From the overworld in the patch, with `party_size` Pokemon in the party:
//  catches as `plan` says.
CaptureResult run_capture(Logger& logger, CaptureActions& actions, const CapturePlan& plan, size_t party_size);

//  What the run says when a capture of `plan` ends with `result`.
std::string capture_message(const CapturePlan& plan, const CaptureResult& result);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
