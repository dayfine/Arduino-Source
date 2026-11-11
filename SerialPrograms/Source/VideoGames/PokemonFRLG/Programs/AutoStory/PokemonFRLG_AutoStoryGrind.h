/*  AutoStory Grind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  GRIND(who, where, level): fight wild battles in a patch until a party
 *  member reaches a level, then heal. Or GRIND(who, where, trainer): until
 *  the battle sim (PokemonFRLG_AutoStoryWinChance.h) says the member beats
 *  the next boss often enough, the level then a cap: between battles, with
 *  the level, moves and stats just read, healed, and the bag's Potions. The table stands the player in the
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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryWinChance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Configuration for an auto-story grinding session.
struct GrindPlan{
    //  The party slot that grinds: the lead (0) only.
    size_t member = 0;
    //  Without a trainer, the level to reach; with one, the cap: the step
    //  stops there if the chance is still short.
    unsigned level = 2;
    //  Wild battles, of any species, before the step gives up.
    size_t max_encounters = 60;
    //  The way the player steps to look for a battle, then back.
    Pokemon::Direction pace = Pokemon::Direction::EAST;
    //  Battles between visits to the nurse; 0: only when hurt.
    size_t heal_every = 0;
    //  The next boss step's trainer, e.g. "TRAINER_RIVAL_CERULEAN_BULBASAUR":
    //  grind until the sim's win chance against it is `min_chance` or
    //  more. Empty: grind to `level`.
    std::string trainer = {};
    double min_chance = MIN_WIN_CHANCE;
};

//  The variant of a GAME step that is a GRIND step.
inline constexpr std::string_view GRIND_VARIANT = "grind";
//  Its args, and a GRIND_CHECK step's: the lead's slot, the level, the
//  encounters and the pace; the nurse's visits, and the trainer to beat
//  with the chance to reach.
inline constexpr Pokemon::StoryArgSpec GRIND_ARGS[] = {
    {"slot", Pokemon::StoryArgType::NUMBER, true},
    {"level", Pokemon::StoryArgType::NUMBER, true},
    {"encounters", Pokemon::StoryArgType::NUMBER, true},
    {"pace", Pokemon::StoryArgType::TEXT, true},
    {"heal_every", Pokemon::StoryArgType::NUMBER, false},
    {"beat", Pokemon::StoryArgType::TEXT, false},
    {"chance", Pokemon::StoryArgType::NUMBER, false},
};

//  `plan` as a GRIND step's args: {"slot": 1, "level": 18, "encounters":
//  60, "pace": "east", "heal_every": 8}, the slot counted from 1, and with
//  a trainer "beat": "TRAINER_RIVAL_CERULEAN_BULBASAUR", "chance": 70, the
//  chance in percent. The step's target is the nurse.
Pokemon::StoryArgs grind_args(const GrindPlan& plan);
//  The plan a GRIND step (grind_step()) runs; nothing if it isn't one.
std::optional<GrindPlan> parse_grind_step(const Pokemon::StoryStep& step);

//  The table's GRIND step: `nurse` is where the player faces the nurse.
Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::Pose nurse);

//  The variant of a GAME step that checks a GRIND step's plan before the
//  walk out to its patch, with grind_args() as its args.
inline constexpr std::string_view GRIND_CHECK_VARIANT = "grind_check";
//  The plan a grind check step (grind_check_step()) checks; nothing if it
//  isn't one.
std::optional<GrindPlan> parse_grind_check_step(const Pokemon::StoryStep& step);

//  The table's check before the walk out to a GRIND step's patch: when the
//  lead already has what `plan` grinds for, the step sets a run flag named
//  `name`, which the walk out, the GRIND step and the way back skip_if.
Pokemon::StoryStep grind_check_step(std::string name, std::string why, const GrindPlan& plan);


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
    //  The sim's chance for `member`, as just read, healed, against
    //  `trainer`; none if the sim lacks the species or the trainer.
    virtual std::optional<WinEstimate> win_chance(const MemberInfo& member, std::string_view trainer) = 0;
};

//  How a grinding session concluded.
enum class GrindEnd{
    //  The level, or the win chance, reached.
    REACHED,
    //  The level cap reached with the win chance still short.
    LEVEL_CAP,
    //  The sim has no chance for the member against the trainer.
    NO_ESTIMATE,
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
const char* grind_end_name(GrindEnd end);

//  The summary result of a grinding session.
struct GrindResult{
    GrindEnd end = GrindEnd::FAILED;
    size_t encounters = 0;
    size_t heals = 0;
    std::optional<unsigned> level = std::nullopt;
    //  The last win chance against the plan's trainer.
    std::optional<double> chance = std::nullopt;
    std::string detail = {};
};

//  From the overworld in the patch: grinds as `plan` says.
GrindResult run_grind(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan);

//  Before the walk out to the patch: whether the member `plan` grinds, as
//  read, already has its level, or with a trainer its win chance. Nothing
//  is fought. 06.03 walked to Route 6's grass and back at Lv29 with the
//  chance against Lt. Surge already 79% (console B, 2026-11-03).
bool grind_reached_already(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan);

//  Formats a log message summarizing the grind result.
std::string grind_message(const GrindPlan& plan, const GrindResult& result);

//  The story's lead as `read` shows it (species, level, moves, stats),
//  each move's PP as `tracked` counts it for a move of that name, else
//  full.
PartyMember lead_as_read(const MemberInfo& read, const PartyMember& tracked);

//  The sim's chance for `lead` with `items` against `trainer`, once the
//  nurse has healed it (full HP and PP).
std::optional<WinEstimate> healed_win_chance(PartyMember lead, const SimItems& items, std::string_view trainer);


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
        Supplies& supplies, StoryChecks checks, HealTrip heal_trip, PaceTrainerBattle trainer_battle
    );

    virtual bool find_encounter(Pokemon::Direction pace) override;
    virtual EncounterSeen read_encounter() override;
    virtual GrindFight fight() override;
    virtual bool needs_heal() override;
    virtual bool heal_trip() override;
    //  With the bag's Potions. The member read becomes the story's lead,
    //  so the boss's own check before its battle plays the same one.
    virtual std::optional<WinEstimate> win_chance(const MemberInfo& member, std::string_view trainer) override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    Supplies& m_supplies;
    StoryChecks m_checks;
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
