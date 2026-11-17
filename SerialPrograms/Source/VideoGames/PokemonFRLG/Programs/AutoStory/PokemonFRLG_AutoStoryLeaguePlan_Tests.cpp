/*  AutoStory League Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The budgeted C2 policy's turns (PokemonFRLG_AutoStoryBattlePolicy.h)
 *  in the League rows' battles (20.01 to 20.05).
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryRows20_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The League's budgeted C2 (budgeted-setup) at one turn of a League
//  fight (Bruno's unless named): the foe out and its bar, the lead's HP,
//  status and Surf PP, and the fight's budget. The lead is the one into
//  Bruno (console B, 2026-11-16): Blastoise Lv52, HP 165, Sp. Atk 114,
//  Speed 113 (124 with the badge). Onix's best ordinary hit is 37,
//  Hitmonchan's 66 (mean 28); both are slower; Surf KOs Onix from full,
//  not Hitmonchan (about 48 of 131 at its lowest roll). Lorelei's Dewgong
//  hits up to 30 (mean 13), her Lapras up to 39 (mean 15); Agatha's
//  second Gengar up to 49 and is faster.
struct BudgetedTurnInput{
    std::string_view battle = "bruno";
    size_t foe_index;
    double hp;
    double foe_fill = 1.0;
    std::optional<FightBudget> budget = std::nullopt;
    StatusCondition status = StatusCondition::NONE;
    uint8_t surf_pp = 14;
    //  The foe's evasion stages (Double Team).
    int foe_evasion = 0;
    //  Items in the bag beyond the lead's usual ones.
    BagCounts extra_bag = {};
    //  The turns before this one, at the same state, the policy chose for.
    int turns_before = 0;
};

struct BudgetedTurnExpectation{
    //  The item used, "" for an attack.
    std::string_view item;
    //  For an attack: the move the policy names, "" for none.
    std::optional<std::string_view> move = std::nullopt;
};

struct BudgetedTurnCase{
    std::string name;
    BudgetedTurnInput input;
    BudgetedTurnExpectation expectation;
};

const Pokemon::StoryStep& league_battle(std::string_view name){
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const Pokemon::StoryStep& step : row.steps){
            if (row.id.starts_with("20.") && step.kind == Pokemon::StoryStepKind::BATTLE && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No League battle {}.", name));
}

Supplies league_turn_supplies(const BudgetedTurnInput& input){
    const Pokemon::StoryStep& step = league_battle(input.battle);
    Supplies supplies;
    std::vector<BagRow> bag = {
        {"full-restore", 10}, {"x-special", 10}, {"x-speed", 4}, {"x-accuracy", 3}, {"guard-spec", 2},
        {"elixir", 1}, {"parlyz-heal", 2},
    };
    for (const auto& [item, count] : input.extra_bag){
        bag.push_back({item, count});
    }
    set_from_bag(supplies, bag);
    supplies.lead = PartyMember{
        .species = "blastoise",
        .moves = {{"bite", 25}, {"strength", 15}, {"surf", input.surf_pp}, {"blizzard", 5}},
        .moves_through_level = 52,
        .moves_read = true,
        .level = 52,
        .stats = Stats{.hp = 165, .attack = 99, .defense = 129, .sp_attack = 114, .sp_defense = 144, .speed = 113},
    };
    supplies.foes = Pokemon::story_arg<std::vector<std::string>>(step.args, "foes");
    supplies.policy = battle_policy_of(step.args);
    supplies.budget = input.budget;
    return supplies;
}

class Test_BudgetedTurn : public UnitTest{
public:
    explicit Test_BudgetedTurn(BudgetedTurnCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - League budgeted C2: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const BudgetedTurnInput& input = m_case.input;
        const Supplies supplies = league_turn_supplies(input);
        if (supplies.policy.kind != BattlePolicyKind::BUDGETED_SETUP){
            return std::format("{}: expected budgeted-setup.", input.battle);
        }
        PolicyState state(supplies.policy);
        state.foe_evasion_raised(input.foe_evasion);
        const PolicyView view{
            .hp = input.hp, .max_hp = 165, .status = input.status, .speed = 113,
            .foe_index = input.foe_index, .foe_fill = input.foe_fill,
        };
        for (int turn = 0; turn < input.turns_before; turn++){
            state.choose(supplies, view);
        }
        const PolicyState::Turn turn = state.choose(supplies, view);
        const std::string_view chosen = turn.item ? std::string_view(turn.item->item) : std::string_view();
        if (chosen != m_case.expectation.item){
            return std::format("expected \"{}\", chose \"{}\" ({})", m_case.expectation.item, chosen, state.state_line(supplies, view));
        }
        const std::string_view move = turn.move ? std::string_view(*turn.move) : std::string_view();
        if (m_case.expectation.move && move != *m_case.expectation.move){
            return std::format("expected the move \"{}\", named \"{}\" ({})", *m_case.expectation.move, move, state.state_line(supplies, view));
        }
        return true;
    }

private:
    BudgetedTurnCase m_case;
};

FightBudget bruno_budget(int full_restore, int x_special){
    return FightBudget{.full_restore = full_restore, .x_special = x_special, .x_speed = 2};
}

const BudgetedTurnCase BUDGETED_TURN_CASES[] = {
    {
        .name = "KO risk: 60 HP against Hitmonchan's 66: a Full Restore",
        .input = {.foe_index = 1, .hp = 60},
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "KO risk past the fight's budget: still a Full Restore",
        .input = {.foe_index = 1, .hp = 60, .budget = bruno_budget(/*full_restore=*/0, /*x_special=*/2)},
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "sure KO first: 30 HP, Onix slower and KO'd by Surf: attack",
        .input = {.foe_index = 0, .hp = 30},
        .expectation = {.item = ""},
    },
    {
        .name = "sure KO first: 20 HP, Hitmonchan at a 30% bar: attack",
        .input = {.foe_index = 1, .hp = 20, .foe_fill = 0.3},
        .expectation = {.item = ""},
    },
    {
        .name = "setup within the budget: an X Special",
        .input = {.foe_index = 1, .hp = 165, .budget = bruno_budget(/*full_restore=*/1, /*x_special=*/1)},
        .expectation = {.item = "x-special"},
    },
    {
        .name = "setup capped: no X Special left in the budget: attack",
        .input = {.foe_index = 1, .hp = 165, .budget = bruno_budget(/*full_restore=*/1, /*x_special=*/0)},
        .expectation = {.item = ""},
    },
    {
        .name = "under the setup reserve (80 < 94), no Full Restore in the budget: attack",
        .input = {.foe_index = 1, .hp = 80, .budget = bruno_budget(/*full_restore=*/0, /*x_special=*/2)},
        .expectation = {.item = ""},
    },
    {
        .name = "under the setup reserve, a Full Restore in the budget: the Full Restore",
        .input = {.foe_index = 1, .hp = 80, .budget = bruno_budget(/*full_restore=*/1, /*x_special=*/2)},
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "Surf out of PP: an Elixir",
        .input = {.foe_index = 1, .hp = 165, .surf_pp = 0},
        .expectation = {.item = "elixir"},
    },
    {
        .name = "paralysed above the foe's hit: a Parlyz Heal",
        .input = {.foe_index = 1, .hp = 165, .status = StatusCondition::PARALYSIS},
        .expectation = {.item = "parlyz-heal"},
    },
    {
        .name = "no X Accuracy against Double Team: evasion +2, X Accuracy in the budget: attack",
        .input = {
            .foe_index = 1, .hp = 165,
            .budget = FightBudget{.full_restore = 1, .x_accuracy = 3},
            .foe_evasion = 2,
        },
        .expectation = {.item = ""},
    },
    {
        .name = "Lapras at 24 HP, Super Potions held: a Full Restore over the budget, not a Super Potion",
        .input = {
            .battle = "lorelei", .foe_index = 4, .hp = 24, .budget = FightBudget{},
            .extra_bag = {{"super-potion", 4}},
        },
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "Lapras at 24 HP, Hyper Potions in the budget: a Hyper Potion",
        .input = {
            .battle = "lorelei", .foe_index = 4, .hp = 24, .budget = FightBudget{.hyper_potion = 2},
            .extra_bag = {{"super-potion", 4}, {"hyper-potion", 2}},
        },
        .expectation = {.item = "hyper-potion"},
    },
    {
        .name = "Lapras at 24 HP, Hyper Potions held but not budgeted: a Full Restore",
        .input = {
            .battle = "lorelei", .foe_index = 4, .hp = 24, .budget = FightBudget{},
            .extra_bag = {{"hyper-potion", 2}},
        },
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "paralysed at 38 HP against Lapras' 39: a Full Restore, not a Super Potion or Parlyz Heal",
        .input = {
            .battle = "lorelei", .foe_index = 4, .hp = 38, .budget = FightBudget{},
            .status = StatusCondition::PARALYSIS, .extra_bag = {{"super-potion", 4}},
        },
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "Dewgong's bar unmoved after two Super Potions, 25 HP: a Full Restore, no third",
        .input = {
            .battle = "lorelei", .foe_index = 0, .hp = 25, .budget = FightBudget{},
            .extra_bag = {{"super-potion", 4}}, .turns_before = 2,
        },
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "asleep against Agatha's second Gengar at 110 HP: a Full Heal",
        .input = {
            .battle = "agatha", .foe_index = 4, .hp = 110, .status = StatusCondition::SLEEP,
            .extra_bag = {{"full-heal", 2}},
        },
        .expectation = {.item = "full-heal"},
    },
    {
        .name = "asleep at 40 HP against Gengar's 49: a Full Restore",
        .input = {
            .battle = "agatha", .foe_index = 4, .hp = 40, .status = StatusCondition::SLEEP,
            .extra_bag = {{"full-heal", 2}},
        },
        .expectation = {.item = "full-restore"},
    },
    {
        .name = "Hitmonchan at a 4% bar: Strength, not Surf",
        .input = {.foe_index = 1, .hp = 165, .foe_fill = 0.04},
        .expectation = {.item = "", .move = "strength"},
    },
    {
        .name = "Hitmonchan at a 30% bar: Strength doesn't KO, no move named",
        .input = {.foe_index = 1, .hp = 165, .foe_fill = 0.3},
        .expectation = {.item = "", .move = ""},
    },
};

} // namespace


void add_tests_AutoStoryLeaguePlan(UnitTestDatabase& database){
    for (const BudgetedTurnCase& item : BUDGETED_TURN_CASES){
        database.add<Test_BudgetedTurn>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
