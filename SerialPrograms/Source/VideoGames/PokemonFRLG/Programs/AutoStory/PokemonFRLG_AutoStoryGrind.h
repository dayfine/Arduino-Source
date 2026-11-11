/*  AutoStory Grind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  GRIND(who, where, level): fight wild battles in a patch until a party
 *  member reaches a level, then heal. The table stands the player in the
 *  patch (as for a capture) and names the Pokemon Center's nurse as the
 *  step's target: between rounds, and at the end, the step walks there,
 *  heals and walks back to where it started.
 *
 *  Each battle is the story's battle handler as it is (fight_battle(),
 *  WILD_FIGHT: it fights while the lead is healthy and runs otherwise);
 *  move-learn prompts, evolutions and switching are its business. So the
 *  member that grinds must lead the party: the step refuses another slot.
 *  It heals when the battle handler says the lead is hurt or has a status,
 *  when its attacks are low on PP, or after `heal_every` battles; the
 *  nurse restores PP as well. It is bounded by encounters, and a
 *  shiny stops it as it stops a capture: nothing pressed.
 *
 *  The member is read with a MemberReader before the first battle and
 *  after each win.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGrind_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGrind_H

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
#include "PokemonFRLG_AutoStoryGrindRun.h"
#include "PokemonFRLG_AutoStoryGrindStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  From the overworld in the patch: grinds as `plan` says.
GrindResult run_grind(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan);

//  Before the walk out to the patch: whether the member `plan` grinds, as
//  read, already has its level. Nothing is fought. 00.24 walked to Route
//  6's grass and back at Lv29 (console B, 2026-11-03).
bool grind_reached_already(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan);

//  Formats a log message summarizing the grind result.
std::string grind_message(const GrindPlan& plan, const GrindResult& result);


//  Where pacing left the player, for the trip to the nurse.
using GrindSpot = PaceSpot;

//  The console's GRIND steps: encounters found by pacing as a capture
//  does, the shiny heard as a capture hears it, battles by fight_battle(),
//  the trip to the nurse and back by the caller (the story's routes and
//  nurse), told where pacing left the player.
class ConsoleGrindActions : public GrindActions{
public:
    using HealTrip = std::function<bool(const GrindSpot& spot)>;

    ConsoleGrindActions(
        ConsoleHandle& console, ProControllerContext& context, Language language,
        Supplies& supplies, Pokemon::StoryCheckLevel checks, HealTrip heal_trip, PaceTrainerBattle trainer_battle
    );

    virtual bool find_encounter(Pokemon::Direction pace) override;
    virtual EncounterSeen read_encounter() override;
    virtual GrindFight fight() override;
    virtual bool needs_heal() override;
    virtual bool heal_trip() override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    Supplies& m_supplies;
    Pokemon::StoryCheckLevel m_checks;
    ConsoleCaptureActions m_encounters;
    HealTrip m_heal_trip;
    PaceTrainerBattle m_trainer_battle;
    GrindSpot m_spot;
};


//  Register auto-story grind unit tests.
void add_tests_AutoStoryGrind(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
