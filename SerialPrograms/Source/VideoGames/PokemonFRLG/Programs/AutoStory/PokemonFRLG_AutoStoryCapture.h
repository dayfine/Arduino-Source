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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a capture step catches, and its bounds.
struct CapturePlan{
    //  The species' slug, e.g. "rattata".
    std::string species = {};
    //  How many to catch.
    size_t catches = 1;
    //  Wild battles, of any species, before the step gives up.
    size_t max_encounters = 20;
    //  Poke Balls thrown before the step gives up.
    size_t max_balls = 5;
    //  The way the player steps to look for a battle, then back: the table
    //  stands the player in the patch with a free tile that way.
    Pokemon::Direction pace = Pokemon::Direction::EAST;
    //  The FIGHT list's move (0-3) to weaken the target with: one that
    //  can't knock it out from green HP. None: only balls.
    std::optional<size_t> weaken_move = std::nullopt;
    //  The most turns spent weakening each target.
    size_t max_weaken_turns = 2;
};

//  The prefix of a GAME step's variant that names a capture step.
inline constexpr std::string_view CAPTURE_VARIANT_PREFIX = "capture ";

//  `plan` as a GAME step's variant, e.g. "capture rattata catches=1
//  encounters=20 balls=5 pace=east", with " move=1 turns=2" when it
//  weakens (the move counted from 1).
std::string capture_variant(const CapturePlan& plan);
//  A variant from capture_variant(), whose keys after the species may come
//  in any order or be left out (their defaults); nothing if `variant` isn't
//  one or has a key or value it doesn't know.
std::optional<CapturePlan> parse_capture_variant(std::string_view variant);

//  The table's step that runs `plan`.
Pokemon::StoryStep capture_step(std::string name, std::string why, const CapturePlan& plan);
//  The table's step that runs `transfer` at the PC the player faces.
Pokemon::StoryStep pc_step(std::string name, std::string why, const PcTransfer& transfer);


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

enum class TurnEnd{
    //  The battle menu is back.
    MENU,
    //  The wild Pokemon fainted; the battle is over and the overworld back.
    FOE_FAINTED,
    //  The player's Pokemon fainted.
    PLAYER_FAINTED,
    FAILED,
};

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

enum class CatchDestination{
    PARTY,
    PC,
};
const char* destination_name(CatchDestination destination);

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
const char* capture_end_name(CaptureEnd end);

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


//  A capture or PC GAME step (see capture_step(), pc_step()) with these
//  actions, the party read from `pc`: nothing for another variant. A capture that ends on a shiny
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


void add_tests_AutoStoryCapture(UnitTestDatabase& database);



}
}
}
#endif
