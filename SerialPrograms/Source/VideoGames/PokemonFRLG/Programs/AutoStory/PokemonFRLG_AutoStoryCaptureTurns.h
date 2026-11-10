/*  AutoStory Capture Turns
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a capture's battle shows and how each of its turns and throws ends.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCaptureTurns_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCaptureTurns_H

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
#include "PokemonFRLG_AutoStoryCaptureStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a wild battle's opening showed.
struct EncounterSeen{
    //  The species' slug; empty if it couldn't be read.
    std::string species = {};
    bool shiny = false;
};

//  The wild Pokemon's HP bar.
enum class FoeHp{
    UNKNOWN,
    GREEN,
    YELLOW,
    RED,
};

//  How an attack turn during a capture concluded.
enum class TurnEnd{
    //  The battle menu is back.
    MENU,
    //  The wild Pokemon fainted; the battle is over and the overworld back.
    FOE_FAINTED,
    //  The player's Pokemon fainted.
    PLAYER_FAINTED,
    FAILED,
};

//  The result of throwing a ball during a capture attempt.
enum class ThrowEnd{
    //  Caught: its first message shows.
    CAUGHT,
    //  It broke free; the battle menu is back.
    BROKE_FREE,
    //  The bag has no Poke Ball; the battle menu is back.
    NO_BALLS,
    PLAYER_FAINTED,
    FAILED,
};

//  Where a captured Pokemon was sent.
enum class CatchDestination{
    PARTY,
    PC,
};
//  The human-readable name of a catch destination.
std::string_view destination_name(CatchDestination destination);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
