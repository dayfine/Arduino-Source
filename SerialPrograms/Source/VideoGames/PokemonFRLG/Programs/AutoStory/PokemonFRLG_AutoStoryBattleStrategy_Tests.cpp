/*  AutoStory Battle Strategy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The hard battles from Lavender to the Champion, each from the lead
 *  Pokeguy's Squirtle route brought to it (PokemonSimulation
 *  docs/progression/pokeguy-states.md: Blastoise, its raw stats, HP, moves
 *  and PP, and its bag), played in the sim three ways from the same seed:
 *  - before: own_turn() with the items it knows (Potions, Awakenings,
 *    Antidotes);
 *  - heals: the plan with the whole bag but no setup goals;
 *  - plan: the plan with strategies.json's goals.
 *  The sim has no abilities (Torrent, Intimidate) or held items (Black
 *  Glasses), so its chances are not PokemonSimulation's.
 *
 */

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattleStrategy.h"
#include "PokemonFRLG_AutoStoryWinChance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Battles per estimate, and the seed every estimate starts from.
constexpr size_t BATTLES = 400;
constexpr uint32_t SEED = 1;

//  A hard battle and the lead Pokeguy brought to it.
struct HardBattle{
    std::string_view name;
    std::string_view trainer;
    uint8_t level = 0;
    uint16_t hp = 0;
    Stats stats = {};
    std::array<std::pair<std::string_view, uint8_t>, 4> moves = {};
    SimItems items = {};
    bool soul_badge = false;
    bool volcano_badge = false;
};

const HardBattle HARD_BATTLES[] = {
    {
        .name = "hideout-giovanni", .trainer = "TRAINER_BOSS_GIOVANNI", .level = 36, .hp = 38,
        .stats = {.hp = 114, .attack = 74, .defense = 76, .sp_attack = 85, .sp_defense = 88, .speed = 74},
        .moves = {{{"water-gun", 20}, {"mega-kick", 1}, {"bite", 12}, {"water-pulse", 8}}},
        .items = {.potions = 5, .awakenings = 3, .antidotes = 1, .super_potions = 5, .full_restores = 0, .x_specials = 0, .x_speeds = 0, .x_accuracies = 0},
        .soul_badge = false, .volcano_badge = false,
    },
    {
        .name = "tower-rival", .trainer = "TRAINER_RIVAL_POKEMON_TOWER_BULBASAUR", .level = 36, .hp = 38,
        .stats = {.hp = 114, .attack = 74, .defense = 76, .sp_attack = 85, .sp_defense = 88, .speed = 74},
        .moves = {{{"water-gun", 18}, {"mega-kick", 1}, {"bite", 12}, {"water-pulse", 6}}},
        .items = {.potions = 5, .awakenings = 3, .antidotes = 1, .super_potions = 5, .full_restores = 0, .x_specials = 21, .x_speeds = 6, .x_accuracies = 5},
        .soul_badge = false, .volcano_badge = false,
    },
    {
        .name = "koga", .trainer = "TRAINER_LEADER_KOGA", .level = 39, .hp = 34,
        .stats = {.hp = 122, .attack = 81, .defense = 82, .sp_attack = 93, .sp_defense = 95, .speed = 81},
        .moves = {{{"mega-kick", 1}, {"bite", 6}, {"water-pulse", 2}, {"surf", 11}}},
        .items = {.potions = 5, .awakenings = 3, .antidotes = 1, .super_potions = 5, .full_restores = 1, .x_specials = 19, .x_speeds = 6, .x_accuracies = 5},
        .soul_badge = false, .volcano_badge = false,
    },
    {
        .name = "blaine", .trainer = "TRAINER_LEADER_BLAINE", .level = 40, .hp = 24,
        .stats = {.hp = 126, .attack = 83, .defense = 85, .sp_attack = 95, .sp_defense = 98, .speed = 83},
        .moves = {{{"mega-kick", 1}, {"bite", 5}, {"water-pulse", 2}, {"surf", 5}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 5, .full_restores = 1, .x_specials = 19, .x_speeds = 6, .x_accuracies = 5},
        .soul_badge = true, .volcano_badge = false,
    },
    {
        .name = "erika", .trainer = "TRAINER_LEADER_ERIKA", .level = 44, .hp = 35,
        .stats = {.hp = 137, .attack = 91, .defense = 93, .sp_attack = 104, .sp_defense = 107, .speed = 92},
        .moves = {{{"bite", 2}, {"water-pulse", 2}, {"surf", 1}, {"blizzard", 5}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 5, .full_restores = 1, .x_specials = 19, .x_speeds = 6, .x_accuracies = 5},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "silph-rival", .trainer = "TRAINER_RIVAL_SILPH_BULBASAUR", .level = 44, .hp = 85,
        .stats = {.hp = 137, .attack = 91, .defense = 93, .sp_attack = 104, .sp_defense = 107, .speed = 92},
        .moves = {{{"bite", 25}, {"water-pulse", 20}, {"surf", 15}, {"blizzard", 5}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 1, .x_specials = 19, .x_speeds = 6, .x_accuracies = 5},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "silph-giovanni", .trainer = "TRAINER_BOSS_GIOVANNI_2", .level = 46, .hp = 33,
        .stats = {.hp = 143, .attack = 96, .defense = 98, .sp_attack = 110, .sp_defense = 112, .speed = 96},
        .moves = {{{"bite", 23}, {"water-pulse", 17}, {"surf", 14}, {"blizzard", 3}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 1, .x_specials = 17, .x_speeds = 6, .x_accuracies = 4},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "sabrina", .trainer = "TRAINER_LEADER_SABRINA", .level = 46, .hp = 33,
        .stats = {.hp = 143, .attack = 96, .defense = 98, .sp_attack = 110, .sp_defense = 112, .speed = 96},
        .moves = {{{"bite", 23}, {"water-pulse", 16}, {"surf", 11}, {"blizzard", 3}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 1, .x_specials = 17, .x_speeds = 6, .x_accuracies = 4},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "giovanni-gym", .trainer = "TRAINER_LEADER_GIOVANNI", .level = 48, .hp = 28,
        .stats = {.hp = 149, .attack = 101, .defense = 101, .sp_attack = 115, .sp_defense = 117, .speed = 100},
        .moves = {{{"bite", 23}, {"water-pulse", 11}, {"surf", 4}, {"blizzard", 3}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 1, .x_specials = 17, .x_speeds = 5, .x_accuracies = 4},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "route22-rival", .trainer = "TRAINER_RIVAL_ROUTE22_LATE_BULBASAUR", .level = 49, .hp = 153,
        .stats = {.hp = 153, .attack = 103, .defense = 105, .sp_attack = 117, .sp_defense = 119, .speed = 102},
        .moves = {{{"bite", 23}, {"water-pulse", 6}, {"surf", 4}, {"blizzard", 3}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 0, .x_specials = 17, .x_speeds = 5, .x_accuracies = 4},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "lorelei", .trainer = "TRAINER_ELITE_FOUR_LORELEI", .level = 51, .hp = 159,
        .stats = {.hp = 159, .attack = 107, .defense = 108, .sp_attack = 123, .sp_defense = 124, .speed = 107},
        .moves = {{{"bite", 25}, {"surf", 15}, {"blizzard", 5}, {"strength", 15}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 13, .x_specials = 15, .x_speeds = 4, .x_accuracies = 3},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "bruno", .trainer = "TRAINER_ELITE_FOUR_BRUNO", .level = 52, .hp = 130,
        .stats = {.hp = 162, .attack = 109, .defense = 111, .sp_attack = 126, .sp_defense = 126, .speed = 109},
        .moves = {{{"bite", 19}, {"surf", 14}, {"blizzard", 5}, {"strength", 15}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 12, .x_specials = 11, .x_speeds = 4, .x_accuracies = 3},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "agatha", .trainer = "TRAINER_ELITE_FOUR_AGATHA", .level = 53, .hp = 45,
        .stats = {.hp = 165, .attack = 112, .defense = 113, .sp_attack = 128, .sp_defense = 129, .speed = 111},
        .moves = {{{"bite", 19}, {"surf", 9}, {"blizzard", 5}, {"strength", 15}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 12, .x_specials = 9, .x_speeds = 3, .x_accuracies = 3},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "lance", .trainer = "TRAINER_ELITE_FOUR_LANCE", .level = 54, .hp = 168,
        .stats = {.hp = 168, .attack = 114, .defense = 116, .sp_attack = 130, .sp_defense = 131, .speed = 113},
        .moves = {{{"bite", 19}, {"surf", 4}, {"blizzard", 5}, {"strength", 15}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 11, .x_specials = 8, .x_speeds = 2, .x_accuracies = 3},
        .soul_badge = true, .volcano_badge = true,
    },
    {
        .name = "champion", .trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .level = 55, .hp = 171,
        .stats = {.hp = 171, .attack = 117, .defense = 118, .sp_attack = 134, .sp_defense = 134, .speed = 115},
        .moves = {{{"bite", 25}, {"surf", 13}, {"blizzard", 5}, {"strength", 15}}},
        .items = {.potions = 4, .awakenings = 3, .antidotes = 1, .super_potions = 4, .full_restores = 9, .x_specials = 7, .x_speeds = 2, .x_accuracies = 2},
        .soul_badge = true, .volcano_badge = true,
    },
};

SimBattler lead(const HardBattle& battle){
    SimBattler own;
    own.species = "blastoise";
    own.body = combatant("blastoise", battle.level, OWN_GENES).value();
    own.body.stats = battle.stats;
    own.body.boulder_badge = true;
    own.body.soul_badge = battle.soul_badge;
    own.body.volcano_badge = battle.volcano_badge;
    own.hp = battle.hp;
    for (const auto& [move, pp] : battle.moves){
        own.moves.push_back(SimMove{.move = std::string(move), .pp = pp});
    }
    return own;
}

//  The items own_turn() uses, the rest left out.
SimItems known_items(const SimItems& items){
    return SimItems{.potions = items.potions, .awakenings = items.awakenings, .antidotes = items.antidotes};
}

struct Rates{
    double before = 0;
    double heals = 0;
    double plan = 0;
};

Rates rates(const HardBattle& battle, uint16_t hp){
    SimBattler own = lead(battle);
    own.hp = hp;
    const SimTrainer trainer = sim_trainer(battle.trainer).value();
    return Rates{
        .before = estimate_win_chance(own, known_items(battle.items), trainer, BATTLES, SEED).chance,
        .heals = estimate_win_chance(own, battle.items, trainer, BATTLES, SEED).chance,
        .plan = estimate_win_chance(own, battle.items, trainer, BATTLES, SEED, setup_goals(battle.trainer)).chance,
    };
}

//  Each hard battle at its recorded HP and at full HP: the plan wins at
//  least as often as before, give or take the sampling (5 points at 400
//  battles), and more often over all of them.
UnitTestResult test_plan_win_rates(Logger& logger){
    double before_sum = 0;
    double plan_sum = 0;
    std::string worse;
    logger.log("battle, HP: before / heals / plan");
    for (const HardBattle& battle : HARD_BATTLES){
        for (uint16_t hp : {battle.hp, battle.stats.hp}){
            const Rates r = rates(battle, hp);
            logger.log(std::format(
                "{}, {}/{}: {:.1f}% / {:.1f}% / {:.1f}%",
                battle.name, hp, battle.stats.hp, 100 * r.before, 100 * r.heals, 100 * r.plan
            ));
            before_sum += r.before;
            plan_sum += r.plan;
            if (r.plan < r.before - 0.05){
                worse += std::format(" {} at {} HP ({:.1f}% < {:.1f}%);", battle.name, hp, 100 * r.plan, 100 * r.before);
            }
        }
    }
    if (!worse.empty()){
        return "The plan lost more often than before:" + worse;
    }
    if (plan_sum <= before_sum){
        return std::format("Expected the plan to win more over all, got {:.2f} <= {:.2f}.", plan_sum, before_sum);
    }
    return true;
}

const HardBattle& hard_battle(std::string_view name){
    for (const HardBattle& battle : HARD_BATTLES){
        if (battle.name == name){
            return battle;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No hard battle " + std::string(name) + ".");
}

//  strategies.json's goals: Bruno two X Specials and outspeeding, Lance
//  three and two X Accuracies; none for a trainer it doesn't name.
UnitTestResult test_goals_from_data(Logger&){
    const SetupGoals bruno = setup_goals("TRAINER_ELITE_FOUR_BRUNO");
    const SetupGoals lance = setup_goals("TRAINER_ELITE_FOUR_LANCE");
    if (bruno.sp_attack != 2 || !bruno.outspeed || bruno.accuracy != 0 || bruno.attack != 0){
        return "Bruno: expected +2 Sp. Atk and outspeeding.";
    }
    if (lance.sp_attack != 3 || lance.accuracy != 2){
        return "Lance: expected +3 Sp. Atk and +2 accuracy.";
    }
    if (setup_goals("TRAINER_LEADER_BROCK").any() || setup_goals("TRAINER_BOSS_GIOVANNI").any()){
        return "Expected no goals for Brock or the hideout's Giovanni.";
    }
    return true;
}

//  Lorelei's Dewgong against the full-HP Blastoise: X Special first (goal
//  +4), and after four, X Speed while Dewgong's speed isn't beaten. With
//  no X items left, own_turn()'s attack. At 20/159 HP with a Full Restore
//  left, the Full Restore before more setup.
UnitTestResult test_setup_order(Logger&){
    const HardBattle& battle = hard_battle("lorelei");
    const SimTrainer trainer = sim_trainer(battle.trainer).value();
    const SetupGoals goals = setup_goals(battle.trainer);
    const SimBattler& dewgong = trainer.party[0];
    const std::vector<SimBattler> to_come(trainer.party.begin() + 1, trainer.party.end());
    SimBattler own = lead(battle);
    PlannedTurn turn = planned_turn(own, battle.items, dewgong, trainer, trainer.party.size(), to_come, goals);
    if (turn.item != PlanItem::X_SPECIAL){
        return "At full HP: expected an X Special.";
    }
    own.body.stages[static_cast<size_t>(Stage::SP_ATTACK)] = 4;
    turn = planned_turn(own, battle.items, dewgong, trainer, trainer.party.size(), to_come, goals);
    const std::optional<PlanItem> speed = next_setup_item(own, battle.items, dewgong, to_come, goals);
    if (turn.item != speed){
        return "At +4 Sp. Atk: expected the next setup item, " + std::string(speed ? plan_item_slug(*speed) : "none") + ".";
    }
    SimItems no_x = battle.items;
    no_x.x_specials = no_x.x_speeds = no_x.x_accuracies = 0;
    turn = planned_turn(own, no_x, dewgong, trainer, trainer.party.size(), to_come, goals);
    if (turn.item || !turn.turn.slot){
        return "With no X items: expected an attack.";
    }
    own.body.stages[static_cast<size_t>(Stage::SP_ATTACK)] = 0;
    own.hp = 20;
    turn = planned_turn(own, battle.items, dewgong, trainer, trainer.party.size(), to_come, goals);
    if (!turn.item || plan_item_heal(*turn.item, own) == 0){
        return std::format("At 20 HP: expected a heal, got {}.", turn.item ? plan_item_slug(*turn.item) : "no plan item");
    }
    return true;
}


struct AutoStoryBattleStrategyInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct AutoStoryBattleStrategyExpectation{
    const char* description;
};

struct AutoStoryBattleStrategyTestCase{
    AutoStoryBattleStrategyInput input;
    AutoStoryBattleStrategyExpectation expectation;
};

class Test_AutoStoryBattleStrategy : public UnitTest{
public:
    explicit Test_AutoStoryBattleStrategy(AutoStoryBattleStrategyTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryBattleStrategy - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_scenario.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryBattleStrategyTestCase m_scenario;
};

const AutoStoryBattleStrategyTestCase AUTO_STORY_BATTLE_STRATEGY_CASES[] = {
    {
        .input = {.name = "setup goals from strategies.json", .body = test_goals_from_data},
        .expectation = {.description = "reads each trainer's goals, none for one not listed"},
    },
    {
        .input = {.name = "setup order and the heal before it", .body = test_setup_order},
        .expectation = {.description = "X Special, then X Speed, then the attack; a heal first at low HP"},
    },
    {
        .input = {.name = "hard battles: the plan against before", .body = test_plan_win_rates},
        .expectation = {.description = "the plan wins at least as often as own_turn() in every hard battle"},
    },
};

} // namespace


void add_tests_AutoStoryBattleStrategy(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_BATTLE_STRATEGY_CASES){
        database.add<Test_AutoStoryBattleStrategy>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
