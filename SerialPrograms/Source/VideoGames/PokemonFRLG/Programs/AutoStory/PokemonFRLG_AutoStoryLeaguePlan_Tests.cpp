/*  AutoStory League Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League planner (PokemonFRLG_AutoStoryLeaguePlan.h) on the bag of
 *  2026-11-16, a smaller and a larger one; the minimum check; the items
 *  between the fights (PokemonFRLG_AutoStoryLeagueSteps.h); and the
 *  budgeted C2 policy's turns (PokemonFRLG_AutoStoryBattlePolicy.h).
 *
 */

#include <array>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryLeaguePlan.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The bag at the League's start on console B, 2026-11-16.
const BagCounts BAG_NOW = {
    {"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
    {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6}, {"full-heal", 2}, {"parlyz-heal", 2},
};


struct LeaguePlanInput{
    BagCounts bag;
};

struct LeaguePlanExpectation{
    //  Lorelei, Bruno, Agatha, Lance, the Champion.
    std::array<FightBudget, 5> budgets;
    FightBudget spare;
};

struct LeaguePlanCase{
    std::string name;
    LeaguePlanInput input;
    LeaguePlanExpectation expectation;
};

class Test_LeaguePlan : public UnitTest{
public:
    explicit Test_LeaguePlan(LeaguePlanCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - League plan: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LeaguePlan plan = plan_league(m_case.input.bag);
        if (plan.fights.size() != m_case.expectation.budgets.size()){
            return std::format("expected {} fights, planned {}", m_case.expectation.budgets.size(), plan.fights.size());
        }
        for (size_t c = 0; c < plan.fights.size(); c++){
            if (plan.fights[c].budget != m_case.expectation.budgets[c]){
                return std::format(
                    "{}: expected {}, planned {}",
                    plan.fights[c].name, budget_text(m_case.expectation.budgets[c]), budget_text(plan.fights[c].budget)
                );
            }
        }
        if (plan.spare != m_case.expectation.spare){
            return std::format("spare: expected {}, planned {}", budget_text(m_case.expectation.spare), budget_text(plan.spare));
        }
        return true;
    }

private:
    LeaguePlanCase m_case;
};

FightBudget budget(int full_restore, int x_special, int x_speed, int x_accuracy, int guard_spec, int elixir){
    return FightBudget{
        .full_restore = full_restore, .x_special = x_special, .x_speed = x_speed,
        .x_accuracy = x_accuracy, .guard_spec = guard_spec, .elixir = elixir,
    };
}

//  `base` with its Hyper (and Max) Potions and Full Heals.
FightBudget with_heals(FightBudget base, int hyper_potion, int full_heal){
    base.hyper_potion = hyper_potion;
    base.full_heal = full_heal;
    return base;
}

const LeaguePlanCase LEAGUE_PLAN_CASES[] = {
    {
        .name = "the bag of 2026-11-16",
        .input = {.bag = BAG_NOW},
        //  Full Restores by the sim's medians, the Champion's reserve of 3
        //  taken from Lorelei; 15 X Specials against 17 wanted, the
        //  Champion's 4 first.
        .expectation = {
            .budgets = {
                budget(/*full_restore=*/0, /*x_special=*/3, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/2, /*x_special=*/3, /*x_speed=*/2, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                with_heals(budget(/*full_restore=*/0, /*x_special=*/2, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0), /*hyper_potion=*/0, /*full_heal=*/1),
                budget(/*full_restore=*/3, /*x_special=*/3, /*x_speed=*/1, /*x_accuracy=*/1, /*guard_spec=*/0, /*elixir=*/1),
                with_heals(budget(/*full_restore=*/3, /*x_special=*/4, /*x_speed=*/2, /*x_accuracy=*/2, /*guard_spec=*/1, /*elixir=*/1), /*hyper_potion=*/0, /*full_heal=*/1),
            },
            .spare = budget(/*full_restore=*/0, /*x_special=*/0, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/1, /*elixir=*/0),
        },
    },
    {
        .name = "a smaller bag",
        .input = {.bag = {
            {"full-restore", 4}, {"x-special", 10}, {"x-speed", 3}, {"x-accuracy", 2}, {"guard-spec", 1}, {"elixir", 1},
        }},
        //  The Champion's Full Restore reserve is half the bag, 2.
        .expectation = {
            .budgets = {
                budget(/*full_restore=*/0, /*x_special=*/2, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/0, /*x_special=*/2, /*x_speed=*/1, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/0, /*x_special=*/1, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/2, /*x_special=*/1, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/2, /*x_special=*/4, /*x_speed=*/2, /*x_accuracy=*/2, /*guard_spec=*/1, /*elixir=*/1),
            },
            .spare = {},
        },
    },
    {
        .name = "a larger bag (the note's buy)",
        .input = {.bag = {
            {"full-restore", 12}, {"x-special", 18}, {"x-speed", 9}, {"x-accuracy", 3}, {"guard-spec", 2},
            {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6},
        }},
        //  Every X Special wanted, one spare; Lance and the Champion take
        //  the extra Full Restores.
        .expectation = {
            .budgets = {
                budget(/*full_restore=*/1, /*x_special=*/4, /*x_speed=*/1, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/2, /*x_special=*/4, /*x_speed=*/4, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/0, /*x_special=*/2, /*x_speed=*/1, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/5, /*x_special=*/3, /*x_speed=*/1, /*x_accuracy=*/1, /*guard_spec=*/0, /*elixir=*/1),
                budget(/*full_restore=*/4, /*x_special=*/4, /*x_speed=*/2, /*x_accuracy=*/2, /*guard_spec=*/1, /*elixir=*/1),
            },
            .spare = budget(/*full_restore=*/0, /*x_special=*/1, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/1, /*elixir=*/0),
        },
    },
    {
        .name = "the bag after the 2026-11-16 15:35 run, no restock",
        .input = {.bag = {
            {"full-restore", 7}, {"x-special", 10}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
            {"max-elixir", 1}, {"full-heal", 2},
        }},
        //  The Champion's 4 X Specials first; its Full Heal first, the
        //  other to Agatha (Hypnosis).
        .expectation = {
            .budgets = {
                budget(/*full_restore=*/0, /*x_special=*/2, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                budget(/*full_restore=*/1, /*x_special=*/2, /*x_speed=*/2, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0),
                with_heals(budget(/*full_restore=*/0, /*x_special=*/1, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/0, /*elixir=*/0), /*hyper_potion=*/0, /*full_heal=*/1),
                budget(/*full_restore=*/3, /*x_special=*/1, /*x_speed=*/1, /*x_accuracy=*/1, /*guard_spec=*/0, /*elixir=*/0),
                with_heals(budget(/*full_restore=*/3, /*x_special=*/4, /*x_speed=*/2, /*x_accuracy=*/2, /*guard_spec=*/1, /*elixir=*/1), /*hyper_potion=*/0, /*full_heal=*/1),
            },
            .spare = budget(/*full_restore=*/0, /*x_special=*/0, /*x_speed=*/0, /*x_accuracy=*/0, /*guard_spec=*/1, /*elixir=*/0),
        },
    },
    {
        .name = "Hyper Potions and Full Heals for every fight",
        .input = {.bag = {{"hyper-potion", 6}, {"max-potion", 2}, {"full-heal", 6}}},
        //  Two of each against Lorelei's Lapras; a Max Potion counts as a Hyper Potion.
        .expectation = {
            .budgets = {
                with_heals(FightBudget{}, /*hyper_potion=*/2, /*full_heal=*/2),
                with_heals(FightBudget{}, /*hyper_potion=*/1, /*full_heal=*/0),
                with_heals(FightBudget{}, /*hyper_potion=*/1, /*full_heal=*/2),
                with_heals(FightBudget{}, /*hyper_potion=*/2, /*full_heal=*/1),
                with_heals(FightBudget{}, /*hyper_potion=*/2, /*full_heal=*/1),
            },
            .spare = {},
        },
    },
};


struct LeagueMinimumInput{
    BagCounts bag;
};

struct LeagueMinimumExpectation{
    //  league_shortfalls()'s list, none when the bag is enough.
    std::optional<std::string> shortfalls;
};

struct LeagueMinimumCase{
    std::string name;
    LeagueMinimumInput input;
    LeagueMinimumExpectation expectation;
};

class Test_LeagueMinimum : public UnitTest{
public:
    explicit Test_LeagueMinimum(LeagueMinimumCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - League minimum: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<std::string> found = league_shortfalls(m_case.input.bag);
        if (found != m_case.expectation.shortfalls){
            return std::format(
                "expected {}, read {}",
                m_case.expectation.shortfalls.value_or("nothing short"), found.value_or("nothing short")
            );
        }
        return true;
    }

private:
    LeagueMinimumCase m_case;
};

const LeagueMinimumCase LEAGUE_MINIMUM_CASES[] = {
    {
        .name = "the bag of 2026-11-16 lacks only Full Heals",
        .input = {.bag = BAG_NOW},
        .expectation = {.shortfalls = "full-heal 2 of 6"},
    },
    {
        .name = "one Full Restore and the Max Elixir short",
        .input = {.bag = {
            {"full-restore", 7}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
            {"elixir", 1}, {"super-potion", 6}, {"full-heal", 6},
        }},
        .expectation = {.shortfalls = "full-restore 7 of 8, max-elixir 0 of 1"},
    },
    {
        .name = "more than the minimum is enough",
        .input = {.bag = {
            {"full-restore", 12}, {"x-special", 18}, {"x-speed", 9}, {"x-accuracy", 3}, {"guard-spec", 2},
            {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 10}, {"full-heal", 8},
        }},
        .expectation = {.shortfalls = std::nullopt},
    },
};


struct BetweenFightsInput{
    BetweenFightView lead;
    BagCounts bag;
    //  The next fight, by its plan row's name.
    std::string next;
};

struct BetweenFightsExpectation{
    BetweenFightAction action;
};

struct BetweenFightsCase{
    std::string name;
    BetweenFightsInput input;
    BetweenFightsExpectation expectation;
};

std::string action_text(const BetweenFightAction& action){
    switch (action.move){
    case BetweenFightMove::READY:
        return "ready";
    case BetweenFightMove::USE_ITEM:
        return std::format("the {}", action.item.value_or("?"));
    case BetweenFightMove::READ_HP:
        return "read the HP";
    case BetweenFightMove::HP_UNKNOWN:
        return "HP unknown";
    }
    return "?";
}

class Test_BetweenFights : public UnitTest{
public:
    explicit Test_BetweenFights(BetweenFightsCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - League between fights: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LeaguePlan plan = plan_league(BAG_NOW);
        const LeagueFightPlan* next = nullptr;
        for (const LeagueFightPlan& fight : plan.fights){
            if (fight.name == m_case.input.next){
                next = &fight;
            }
        }
        if (next == nullptr){
            return "no fight " + m_case.input.next;
        }
        const BetweenFightAction action = between_fight_action(m_case.input.lead, m_case.input.bag, *next);
        if (action != m_case.expectation.action){
            return std::format("expected {}, chose {}", action_text(m_case.expectation.action), action_text(action));
        }
        return true;
    }

private:
    BetweenFightsCase m_case;
};

BetweenFightView lead_at(std::optional<int> hp, int max_hp, int surf_pp = 15, int blizzard_pp = 5){
    return BetweenFightView{
        .hp = hp, .max_hp = max_hp, .hp_reread = false, .cure = "",
        .surf_pp = surf_pp, .surf_max_pp = 15, .blizzard_pp = blizzard_pp, .blizzard_max_pp = 5,
    };
}

//  `lead` after its HP was read again (READ_HP).
BetweenFightView reread(BetweenFightView lead){
    lead.hp_reread = true;
    return lead;
}

BetweenFightView with_cure(BetweenFightView lead, std::string cure){
    lead.cure = std::move(cure);
    return lead;
}

BetweenFightAction use(std::string item){
    return BetweenFightAction{.move = BetweenFightMove::USE_ITEM, .item = std::move(item)};
}

const BetweenFightAction READY{.move = BetweenFightMove::READY};

const BetweenFightsCase BETWEEN_FIGHTS_CASES[] = {
    {
        .name = "into Bruno at 117/165: a Super Potion up to 80%",
        .input = {.lead = lead_at(/*hp=*/117, /*max_hp=*/165), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = use("super-potion")},
    },
    {
        .name = "into Bruno at 140/165: nothing",
        .input = {.lead = lead_at(/*hp=*/140, /*max_hp=*/165), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = READY},
    },
    {
        .name = "into Agatha at 48/165: nothing, above 27%",
        .input = {.lead = lead_at(/*hp=*/48, /*max_hp=*/165), .bag = BAG_NOW, .next = "agatha"},
        .expectation = {.action = READY},
    },
    {
        .name = "into Lance at 48/168: a Super Potion, not a Full Restore",
        .input = {.lead = lead_at(/*hp=*/48, /*max_hp=*/168), .bag = BAG_NOW, .next = "lance"},
        .expectation = {.action = use("super-potion")},
    },
    {
        .name = "into Lance at 98/168, no Super Potion left: a Full Restore",
        .input = {.lead = lead_at(/*hp=*/98, /*max_hp=*/168), .bag = {{"full-restore", 5}}, .next = "lance"},
        .expectation = {.action = use("full-restore")},
    },
    {
        .name = "into Lance at 48/168, no Super Potion, Hyper Potions held: a Hyper Potion, not a Full Restore",
        .input = {.lead = lead_at(/*hp=*/48, /*max_hp=*/168), .bag = {{"hyper-potion", 2}, {"full-restore", 5}}, .next = "lance"},
        .expectation = {.action = use("hyper-potion")},
    },
    {
        .name = "paralysed: a Parlyz Heal first",
        .input = {
            .lead = with_cure(lead_at(/*hp=*/60, /*max_hp=*/168), "parlyz-heal"),
            .bag = BAG_NOW, .next = "lance",
        },
        .expectation = {.action = use("parlyz-heal")},
    },
    {
        .name = "into the Champion with Surf 3, Blizzard 0: the Elixir",
        .input = {.lead = lead_at(/*hp=*/171, /*max_hp=*/171, /*surf_pp=*/3, /*blizzard_pp=*/0), .bag = BAG_NOW, .next = "champion"},
        .expectation = {.action = use("elixir")},
    },
    {
        .name = "into Lance with Blizzard 0 and no Elixir: the Max Elixir",
        .input = {
            .lead = lead_at(/*hp=*/168, /*max_hp=*/168, /*surf_pp=*/9, /*blizzard_pp=*/0),
            .bag = {{"max-elixir", 1}}, .next = "lance",
        },
        .expectation = {.action = use("max-elixir")},
    },
    {
        .name = "HP unknown: read it again, no Full Restore",
        .input = {.lead = lead_at(/*hp=*/std::nullopt, /*max_hp=*/165), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = {.move = BetweenFightMove::READ_HP}},
    },
    {
        .name = "read again at 117/165: a Super Potion up to Bruno's 80%",
        .input = {.lead = reread(lead_at(/*hp=*/117, /*max_hp=*/165)), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = use("super-potion")},
    },
    {
        .name = "read again at 140/165: ready for Bruno",
        .input = {.lead = reread(lead_at(/*hp=*/140, /*max_hp=*/165)), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = READY},
    },
    {
        .name = "still unknown after reading it again: stop, no Full Restore",
        .input = {.lead = reread(lead_at(/*hp=*/std::nullopt, /*max_hp=*/165)), .bag = BAG_NOW, .next = "bruno"},
        .expectation = {.action = {.move = BetweenFightMove::HP_UNKNOWN}},
    },
    {
        .name = "poisoned, no Antidote or Full Heal: a Full Restore",
        .input = {
            .lead = with_cure(lead_at(/*hp=*/150, /*max_hp=*/168), "antidote"),
            .bag = {{"full-restore", 5}, {"super-potion", 6}}, .next = "lance",
        },
        .expectation = {.action = use("full-restore")},
    },
    {
        .name = "poisoned with HP unknown: the Antidote before reading the HP",
        .input = {
            .lead = with_cure(lead_at(/*hp=*/std::nullopt, /*max_hp=*/168), "antidote"),
            .bag = {{"antidote", 1}, {"full-restore", 5}}, .next = "lance",
        },
        .expectation = {.action = use("antidote")},
    },
};



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
            if (row.id == "20.01" && step.kind == Pokemon::StoryStepKind::BATTLE && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No battle {} in 20.01.", name));
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
            return std::format("20.01 {}: expected budgeted-setup.", input.battle);
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
    for (const LeaguePlanCase& item : LEAGUE_PLAN_CASES){
        database.add<Test_LeaguePlan>(item);
    }
    for (const LeagueMinimumCase& item : LEAGUE_MINIMUM_CASES){
        database.add<Test_LeagueMinimum>(item);
    }
    for (const BetweenFightsCase& item : BETWEEN_FIGHTS_CASES){
        database.add<Test_BetweenFights>(item);
    }
    for (const BudgetedTurnCase& item : BUDGETED_TURN_CASES){
        database.add<Test_BudgetedTurn>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
