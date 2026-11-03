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

//  The prefix of a GAME step's variant that names a GRIND step.
inline constexpr std::string_view GRIND_VARIANT_PREFIX = "grind ";

//  `plan` as a variant: "grind slot=1 level=18 encounters=60 pace=east
//  heal_every=8", the slot counted from 1, and with a trainer
//  " beat=TRAINER_RIVAL_CERULEAN_BULBASAUR chance=70", the chance in
//  percent. The step's target is the nurse.
std::string grind_variant(const GrindPlan& plan);
std::optional<GrindPlan> parse_grind_variant(std::string_view variant);

//  The table's GRIND step: `nurse` is where the player faces the nurse.
Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::Pose nurse);


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
const char* grind_end_name(GrindEnd end);

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

std::string grind_message(const GrindPlan& plan, const GrindResult& result);

//  The story's lead as `read` shows it (species, level, moves, stats),
//  each move's PP as `tracked` counts it for a move of that name, else
//  full.
PartyMember lead_as_read(const MemberInfo& read, const PartyMember& tracked);

//  The sim's chance for `lead` with `potions` against `trainer`, once the
//  nurse has healed it (full HP and PP).
std::optional<WinEstimate> healed_win_chance(PartyMember lead, int potions, std::string_view trainer);


//  Where pacing left the player, for the trip to the nurse: on the patch's
//  tile, or one tile `pace` from it, as the last step taken left it.
struct GrindSpot{
    bool displaced = false;
    //  The way the player faces after its last step; nothing before one.
    std::optional<Pokemon::Direction> facing = std::nullopt;
};

//  The console's GRIND steps: encounters found by pacing as a capture
//  does, the shiny heard as a capture hears it, battles by fight_battle(),
//  the trip to the nurse and back by the caller (the story's routes and
//  nurse), told where pacing left the player.
class ConsoleGrindActions : public GrindActions{
public:
    using HealTrip = std::function<bool(const GrindSpot& spot)>;

    ConsoleGrindActions(
        ConsoleHandle& console, ProControllerContext& context, Language language,
        Supplies& supplies, StoryChecks checks, HealTrip heal_trip
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
    GrindSpot m_spot;
};


void add_tests_AutoStoryGrind(UnitTestDatabase& database);



}
}
}
#endif
