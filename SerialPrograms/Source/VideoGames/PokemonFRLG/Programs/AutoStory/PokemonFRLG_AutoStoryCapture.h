/*  AutoStory Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step that catches a named species where the player stands, in
 *  grass or a cave: step back and forth until a wild battle starts, read
 *  its species, run from any other, weaken the target only with a move the
 *  table names and only while its HP bar is green, and throw Poke Balls
 *  until it is caught. A catch goes to the party, or to the PC when the
 *  party is full. The step is bounded: it stops after so many encounters
 *  or so many balls thrown, or when the bag has none left.
 *
 *  A shiny, of any species, stops the step where it is: nothing is pressed
 *  after the shiny is seen, not even RUN, so a person decides what to do
 *  with it. The program stops without reloading.
 *
 *  The decisions take their game steps from a CaptureActions, so they run
 *  in tests against a fake game; ConsoleCaptureActions does them on the
 *  console with the battle detectors the shiny hunts use.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCapture_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCapture_H

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
#include "PokemonFRLG_AutoStoryCaptureRun.h"
#include "PokemonFRLG_AutoStoryCaptureTurns.h"
#include "PokemonFRLG_AutoStoryCaptureStep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A capture or PC GAME step (see capture_step(), pc_step()) with these
//  actions, the party read from `pc`: nothing for another action. A capture that ends on a shiny
//  throws ProgramFinishedException, so the segment stops without
//  reloading, with a screenshot from `stream` if given; any other end but
//  CAUGHT is a failed Outcome, as is a PC step that fails or is refused.
std::optional<Pokemon::Outcome> run_storage_game_step(
    Logger& logger, VideoStream* stream, const Pokemon::StoryStep& step,
    CaptureActions& capture, PcStorageActions& pc
);


//  Where pacing has left the player: on the patch's tile, or one tile
//  `pace` from it, as the last step taken left it.
struct PaceSpot{
    bool displaced = false;
    //  The way the player faces after its last step; nothing before one.
    std::optional<Pokemon::Direction> facing = std::nullopt;
};

//  The game steps of pacing in a patch for a wild battle, injected so the
//  search runs in tests.
class PatchSteps{
public:
    virtual ~PatchSteps() = default;

    //  One step, `pace` or back as the last left the player: whether a
    //  battle's flash showed.
    virtual bool step() = 0;
    //  Whether a battle shows: its text box, with an HP box or the menu.
    virtual bool battle_shows() = 0;
    //  Whether the overworld is up, with no text or menu open.
    virtual bool overworld_shows() = 0;
    //  Saves the screen as a debug image, named `label`.
    virtual void dump_screen(std::string_view label) = 0;
};

//  From the overworld, in the patch: steps until a wild battle starts,
//  `max_steps` at most. True once one has.
bool pace_until_battle(Logger& logger, PatchSteps& steps, size_t max_steps);

//  Steps in a patch before pacing gives up: about 10 % of steps in grass
//  start a battle at Route 4's rates.
constexpr size_t PACE_MAX_STEPS = 200;

//  The console's PatchSteps: `pace` and back from `spot`, which each step
//  updates.
class ConsolePatchSteps : public PatchSteps{
public:
    ConsolePatchSteps(ConsoleHandle& console, ProControllerContext& context, Pokemon::Direction pace, PaceSpot& spot);

    virtual bool step() override;
    virtual bool battle_shows() override;
    virtual bool overworld_shows() override;
    virtual void dump_screen(std::string_view label) override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Pokemon::Direction m_pace;
    PaceSpot& m_spot;
};


//  The console's capture steps. Encounters are found as the overworld
//  shiny hunt finds them, a shiny heard as the shiny hunts hear it
//  (handle_encounter()), and runs as they run (flee_battle()). The wild
//  Pokemon's HP bar has no reader yet, so it reads UNKNOWN and nothing is
//  weakened on the console.
class ConsoleCaptureActions : public CaptureActions{
public:
    ConsoleCaptureActions(ConsoleHandle& console, ProControllerContext& context, Language language);

    virtual bool find_encounter(Pokemon::Direction pace) override;
    virtual EncounterSeen read_encounter() override;
    virtual bool flee() override;
    virtual FoeHp read_foe_hp() override;
    virtual TurnEnd attack(size_t move) override;
    virtual ThrowEnd throw_ball() override;
    virtual bool finish_catch(CatchDestination destination) override;

private:
    //  In the bag: to the Poke Balls pocket. False if it wasn't found.
    bool open_ball_pocket();
    //  After the throw: caught, broke free, or neither in time.
    ThrowEnd wait_for_throw();

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
};


//  Register auto-story capture unit tests.
void add_tests_AutoStoryCapture(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
