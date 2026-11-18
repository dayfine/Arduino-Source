/*  AutoStory Grind Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A GRIND step's battles and how it ends, with its game steps injected so
 *  its decisions run in tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGrindRun_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGrindRun_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryGrindStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The outcome of an individual grinding battle.
enum class GrindFight{
    //  The battle ended with the overworld back and the lead standing,
    //  won or run from.
    OVER,
    //  The lead fainted: a blackout.
    LOST,
    FAILED,
};

//  The game steps a GRIND step takes, injected so its decisions run in
//  tests.
class GrindActions{
public:
    virtual ~GrindActions() = default;

    //  From the overworld, in the patch: steps `pace` and back until a wild
    //  battle starts. False if none did.
    virtual bool find_encounter(Pokemon::Direction pace) = 0;
    //  From a wild battle's start: whether it is shiny, and its species.
    virtual EncounterSeen read_encounter() = 0;
    //  From the battle menu: the battle handler, to the overworld.
    virtual GrindFight fight() = 0;
    //  Whether the lead is hurt, has a status or is low on PP in its
    //  attacks, as the battles left it.
    virtual bool needs_heal() = 0;
    //  From the patch: to the nurse, heal, back to the patch. False if it
    //  didn't get there and back.
    virtual bool heal_trip() = 0;
};

//  How a grinding session concluded.
enum class GrindEnd{
    //  The level reached.
    REACHED,
    OUT_OF_ENCOUNTERS,
    NO_ENCOUNTER,
    //  A shiny showed: stopped in its battle, nothing pressed.
    SHINY,
    PLAYER_FAINTED,
    //  The member's level couldn't be read: no battle fought.
    UNREAD,
    REFUSED,
    FAILED,
};
//  The human-readable name of a grind end outcome.
std::string_view grind_end_name(GrindEnd end);

//  The summary result of a grinding session.
struct GrindResult{
    GrindEnd end = GrindEnd::FAILED;
    size_t encounters = 0;
    size_t heals = 0;
    std::optional<unsigned> level = std::nullopt;
    std::string detail = {};
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
