/*  AutoStory Planned Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League plan as built into the program, the Gen 3 damage the
 *  policy reads (expected rolls from @smogon/calc, as PokemonSimulation's
 *  engine computes them), and the planned policy on the console failures
 *  of 2026-11-16 and the Champion loss: PokemonSimulation #111's
 *  test_planned.py cases, case for case.
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryGen3Damage.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"
#include "PokemonFRLG_AutoStoryPlannedRisk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The bag at the League's start on console B, 2026-11-16 (test_planned.py's BAG).
const SlugCounts BAG = {
    {"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
    {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6}, {"full-heal", 2}, {"parlyz-heal", 2},
    {"antidote", 1},
};
const SlugCounts NO_ELIXIRS = []{
    SlugCounts bag = BAG;
    bag["elixir"] = 0;
    bag["max-elixir"] = 0;
    return bag;
}();
const SlugCounts FULL_PP = {{"bite", 25}, {"strength", 15}, {"surf", 15}, {"blizzard", 5}};
//  Our Lv54 Blastoise (console B, 2026-11-16): 171 HP; Speed 117, 128 with Thunder's boost.
constexpr int OUR_MAX_HP = 171;
constexpr int OUR_SPEED = 117;

//  16 rolls from `low` to `high`.
std::vector<int> rolls(int low, int high){
    std::vector<int> out;
    for (int c = 0; c < 16; c++){
        out.push_back(low + (high - low) * c / 15);
    }
    return out;
}

//  Our four moves' damage on the foe out, and their PP.
struct OurHits{
    std::vector<int> bite = {};
    std::vector<int> strength = {};
    std::vector<int> surf = {};
    std::vector<int> blizzard = {};
};

PlannedOwn blastoise(const OurHits& hits, const SlugCounts& pp){
    const auto pp_of = [&](const std::string& move){
        const auto it = pp.find(move);
        return it == pp.end() ? FULL_PP.at(move) : it->second;
    };
    return PlannedOwn{
        .hp = OUR_MAX_HP,
        .max_hp = OUR_MAX_HP,
        .speed = battle_speed(badge_boosted(OUR_SPEED), /*stage=*/0, /*paralyzed=*/false),
        .moves = {
            {.id = "bite", .pp = pp_of("bite"), .damage = hits.bite},
            {.id = "strength", .pp = pp_of("strength"), .damage = hits.strength},
            {.id = "surf", .pp = pp_of("surf"), .damage = hits.surf},
            {.id = "blizzard", .pp = pp_of("blizzard"), .damage = hits.blizzard},
        },
    };
}

const LeagueFightData& fight(std::string_view name){
    const LeagueFightData* data = league_fight_data(league_plan_data(), name);
    if (data == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No {} in the League plan.", name));
    }
    return *data;
}

std::string move_id(const PlannedState& state, const PlannedChoice& choice){
    if (choice.item){
        return *choice.item;
    }
    if (choice.move_slot){
        return state.own.moves[*choice.move_slot].id;
    }
    return "";
}

std::string describe(const PlannedState& state, const PlannedChoice& choice){
    return std::format("{} ({})", choice.row, move_id(state, choice));
}

//  Blastoise at `hits` and `pp`, with stages, at `hp`.
struct OurSetup{
    int spa = 0;
    int speed = 0;
    int accuracy = 0;
    int hp = OUR_MAX_HP;
};
PlannedOwn blastoise_at(const OurHits& hits, const SlugCounts& pp, const OurSetup& setup){
    PlannedOwn own = blastoise(hits, pp);
    own.spa_stage = setup.spa;
    own.speed_stage = setup.speed;
    own.speed = battle_speed(badge_boosted(OUR_SPEED), setup.speed, /*paralyzed=*/false);
    own.accuracy_stage = setup.accuracy;
    own.hp = setup.hp;
    for (PlannedMove& move : own.moves){
        move.accuracy = hit_chance(/*accuracy=*/100, setup.accuracy, /*evasion_stage=*/0);
    }
    return own;
}


//  The League's foes as the cases need them (pret's parties; the rolls on
//  our Lv54 Blastoise are the sim's where test_planned.py names them).
PlannedFoe golbat(){
    return {.species = "golbat", .slot = 1, .hp = 161, .max_hp = 161, .speed = 129,
        .moves = {{.id = "wing-attack", .pp = 35, .damage = rolls(25, 30)}}};
}
PlannedFoe gyarados(){
    return {.species = "gyarados", .slot = 0, .hp = 189, .max_hp = 189, .speed = 112,
        .moves = {{.id = "hyper-beam", .pp = 5, .accuracy = 0.9, .damage = rolls(68, 80)}}};
}
PlannedFoe dragonair(){
    return {.species = "dragonair", .slot = 1, .hp = 146, .max_hp = 146, .speed = 96,
        .moves = {
            {.id = "hyper-beam", .pp = 5, .accuracy = 0.9, .damage = rolls(45, 54)},
            {.id = "dragon-rage", .pp = 10, .damage = rolls(40, 40)},
        }};
}
PlannedFoe dragonite(){
    return {.species = "dragonite", .slot = 4, .hp = 197, .max_hp = 197, .speed = 107,
        .moves = {{.id = "hyper-beam", .pp = 5, .accuracy = 0.9, .damage = rolls(70, 83)}}};
}
const OurHits ON_DRAGONITE = {.bite = rolls(22, 26), .strength = rolls(18, 22), .surf = rolls(22, 27), .blizzard = rolls(120, 142)};


const OurHits ON_GOLBAT = {.bite = rolls(40, 48), .strength = rolls(30, 36), .surf = rolls(50, 59), .blizzard = rolls(70, 83)};
const OurHits ON_GYARADOS = {.bite = rolls(40, 48), .strength = rolls(30, 36), .surf = rolls(25, 30), .blizzard = rolls(45, 54)};
//  The old rows' setup, complete.
const OurSetup SET_UP = {.spa = 3, .speed = 6, .accuracy = 2};

PlannedFoe lapras_at_12_percent(){
    return {.species = "lapras", .slot = 4, .hp = 26, .max_hp = 220, .speed = 94,
        .moves = {{.id = "body-slam", .pp = 15, .damage = rolls(30, 36)}}};
}
PlannedFoe onix(){
    return {.species = "onix", .slot = 3, .hp = 118, .max_hp = 118, .speed = 96,
        .moves = {{.id = "earthquake", .pp = 10, .damage = rolls(28, 33)}}};
}
PlannedFoe gengar_at_40(){
    return {.species = "gengar", .slot = 0, .hp = 40, .max_hp = 145, .speed = 126,
        .moves = {{.id = "shadow-ball", .pp = 15, .damage = rolls(30, 36)}}};
}
PlannedFoe machamp_at_79(){
    return {.species = "machamp", .slot = 4, .hp = 79, .max_hp = 183, .speed = 83,
        .moves = {{.id = "cross-chop", .pp = 5, .accuracy = 0.8, .damage = rolls(60, 71)}}};
}
PlannedFoe venusaur(){
    return {.species = "venusaur", .slot = 5, .hp = 193, .max_hp = 193, .speed = 125,
        .moves = {{.id = "razor-leaf", .pp = 25, .accuracy = 0.95, .damage = rolls(60, 71)}}};
}
SlugCounts without(SlugCounts bag, std::string_view item){
    bag[std::string(item)] = 0;
    return bag;
}
//  The PP before the Champion on console B, 2026-11-16, topped up by an Elixir.
const SlugCounts CHAMPION_PP = {{"bite", 25}, {"strength", 15}, {"surf", 15}, {"blizzard", 5}};


struct PolicyInput{
    PlannedState state = {};
    std::string fight = {};
    int streak = 0;
};
//  The row and pick (a move or an item) wanted, and those ruled out.
struct PolicyExpectation{
    std::optional<std::string> row = std::nullopt;
    std::optional<std::string> pick = std::nullopt;
    std::vector<std::string> not_rows = {};
    std::vector<std::string> not_picks = {};
    //  The choice is a move (not an item).
    bool attacks = false;
};
struct PolicyCase{
    std::string name;
    PolicyInput input;
    PolicyExpectation expectation;
};

class Test_PlannedChoice : public UnitTest{
public:
    explicit Test_PlannedChoice(PolicyCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PolicyInput& input = m_case.input;
        const PolicyExpectation& want = m_case.expectation;
        const PlannedChoice choice = choose_planned(input.state, fight(input.fight), input.streak);
        const std::string pick = move_id(input.state, choice);
        const bool ruled_out = std::ranges::find(want.not_rows, choice.row) != want.not_rows.end()
            || std::ranges::find(want.not_picks, pick) != want.not_picks.end();
        if ((want.row && choice.row != *want.row) || (want.pick && pick != *want.pick) || ruled_out
            || (want.attacks && !choice.move_slot)){
            return std::format("Chose {}.", describe(input.state, choice));
        }
        return true;
    }

private:
    PolicyCase m_case;
};

const PolicyCase POLICY_CASES[] = {
    {
        .name = "1. Blizzard reserved before Lance: not on Golbat",
        .input = {.state = {.own = blastoise_at(ON_GOLBAT, {}, SET_UP), .foe = golbat(), .foe_party_alive = 4, .bag = BAG}, .fight = "agatha"},
        .expectation = {.not_picks = {"blizzard"}},
    },
    {
        .name = "1. Blizzard reserved before Lance: not on Gyarados",
        .input = {.state = {.own = blastoise_at(ON_GYARADOS, {}, SET_UP), .foe = gyarados(), .foe_party_alive = 5, .bag = BAG}, .fight = "lance"},
        .expectation = {.not_picks = {"blizzard"}},
    },
    {
        .name = "2. no setup for Blizzard at 0 PP",
        .input = {.state = {.own = blastoise(ON_GYARADOS, {{"blizzard", 0}}), .foe = gyarados(), .foe_party_alive = 5, .bag = NO_ELIXIRS}, .fight = "lance"},
        .expectation = {.not_picks = {"x-accuracy", "x-special"}},
    },
    {
        .name = "2. setup for Blizzard with PP",
        .input = {.state = {.own = blastoise(ON_GYARADOS, {}), .foe = gyarados(), .foe_party_alive = 5, .bag = BAG}, .fight = "lance"},
        .expectation = {.row = "plan:setup", .pick = "x-special"},
    },
    {
        .name = "3. Dragonite without Blizzard takes Bite",
        .input = {.state = {.own = blastoise_at(ON_DRAGONITE, {{"blizzard", 0}}, SET_UP), .foe = dragonite(), .foe_party_alive = 1, .bag = NO_ELIXIRS}, .fight = "lance"},
        .expectation = {.pick = "bite"},
    },
    {
        .name = "4. no finisher into a trainer's Full Restore",
        .input = {.state = {.own = blastoise_at({.bite = rolls(20, 24), .strength = rolls(30, 36), .surf = rolls(25, 30), .blizzard = rolls(20, 24)}, {}, {.spa = 3}),
            .foe = lapras_at_12_percent(), .foe_party_alive = 1, .trainer_item_open = true, .bag = BAG}, .fight = "lorelei"},
        .expectation = {.not_rows = {"sure_ko"}},
    },
    {
        .name = "5. no Super Potion that doesn't outpace the foe",
        .input = {.state = {.own = blastoise_at({.bite = rolls(30, 36), .strength = rolls(20, 24), .surf = rolls(25, 30), .blizzard = rolls(90, 106)}, {}, {.hp = 50}),
            .foe = dragonair(), .foe_party_alive = 4, .bag = BAG}, .fight = "lance"},
        .expectation = {.row = "ko_heal", .not_picks = {"super-potion"}},
    },
    {
        .name = "5. the third medicine turn: a Full Restore",
        .input = {.state = {.own = blastoise_at({.bite = rolls(30, 36), .strength = rolls(20, 24), .surf = rolls(25, 30), .blizzard = rolls(90, 106)}, {}, {.hp = 50}),
            .foe = dragonair(), .foe_party_alive = 4, .bag = BAG}, .fight = "lance", .streak = 2},
        .expectation = {.row = "ko_heal", .pick = "full-restore"},
    },
    {
        .name = "6. KO risk from the current foe: the second Onix's hits",
        .input = {.state = {.own = blastoise_at({.bite = rolls(40, 48), .strength = rolls(20, 24), .surf = rolls(130, 154), .blizzard = rolls(60, 72)}, {}, {.spa = 2, .speed = 1, .hp = 60}),
            .foe = onix(), .foe_party_alive = 2, .bag = BAG}, .fight = "bruno"},
        .expectation = {.attacks = true},
    },
    {
        .name = "8. evasion is no sure KO",
        .input = {.state = {.own = blastoise_at({.bite = rolls(50, 60), .strength = {}, .surf = rolls(45, 54), .blizzard = rolls(40, 48)}, {}, {.spa = 2, .speed = 1, .accuracy = -3}),
            .foe = gengar_at_40(), .foe_party_alive = 1, .bag = BAG}, .fight = "agatha"},
        .expectation = {.not_rows = {"sure_ko"}},
    },
    {
        .name = "kill order keeps Machamp out of heal range",
        .input = {.state = {.own = blastoise({.bite = rolls(25, 30), .strength = rolls(20, 24), .surf = rolls(63, 75), .blizzard = rolls(40, 48)}, {}),
            .foe = machamp_at_79(), .foe_party_alive = 1, .trainer_item_open = true, .bag = BAG}, .fight = "bruno"},
        .expectation = {.row = "kill_order", .pick = "bite"},
    },
    {
        .name = "the Max Elixir is held in Lance's fight",
        .input = {.state = {.own = blastoise(ON_DRAGONITE, {{"blizzard", 0}, {"bite", 0}, {"surf", 0}}), .foe = dragonite(), .foe_party_alive = 1, .bag = without(BAG, "elixir")},
            .fight = "lance"},
        .expectation = {.not_picks = {"max-elixir"}},
    },
    {
        .name = "Champion loss: Venusaur takes Blizzard",
        .input = {.state = {.own = blastoise({.bite = rolls(30, 36), .strength = rolls(25, 30), .surf = rolls(20, 24), .blizzard = rolls(95, 112)}, CHAMPION_PP),
            .foe = venusaur(), .foe_party_alive = 1, .bag = without(BAG, "elixir")}, .fight = "champion"},
        .expectation = {.pick = "blizzard"},
    },
    {
        .name = "Champion Arcanine Extreme Speed at 18 HP: ko_heal instead of sure_ko",
        .input = {.state = {
            .own = blastoise_at({.bite = rolls(30, 36), .strength = rolls(25, 30), .surf = rolls(220, 260), .blizzard = rolls(95, 112)}, CHAMPION_PP, {.spa = 2, .speed = 1, .accuracy = 2, .hp = 18}),
            .foe = {.species = "arcanine", .slot = 4, .hp = 199, .max_hp = 199, .speed = 139, .moves = {{.id = "extreme-speed", .pp = 5, .accuracy = 1.0, .damage = rolls(39, 46), .priority = 1}}},
            .foe_party_alive = 1, .bag = BAG}, .fight = "champion"},
        .expectation = {.row = "ko_heal"},
    },
    {
        .name = "Champion Arcanine Extreme Speed at 121 HP: sure_ko Surf",
        .input = {.state = {
            .own = blastoise_at({.bite = rolls(30, 36), .strength = rolls(25, 30), .surf = rolls(220, 260), .blizzard = rolls(95, 112)}, CHAMPION_PP, {.spa = 2, .speed = 1, .accuracy = 2, .hp = 121}),
            .foe = {.species = "arcanine", .slot = 4, .hp = 199, .max_hp = 199, .speed = 139, .moves = {{.id = "extreme-speed", .pp = 5, .accuracy = 1.0, .damage = rolls(39, 46), .priority = 1}}},
            .foe_party_alive = 1, .bag = BAG}, .fight = "champion"},
        .expectation = {.row = "sure_ko", .pick = "surf"},
    },
};


struct RiskInput{
    PlannedState state = {};
    //  The move we_move_first() is asked about.
    PlannedMove move = {};
};
struct RiskExpectation{
    int danger = 0;
    int foe_hp_at_our_move = 0;
    bool we_move_first = false;
};
struct RiskCase{
    std::string name;
    RiskInput input;
    RiskExpectation expectation;
};

class Test_PlannedRisk : public UnitTest{
public:
    explicit Test_PlannedRisk(RiskCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - risk: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlannedState& state = m_case.input.state;
        const int danger = planned_danger(state).normal_max;
        const int foe_hp = foe_hp_at_our_move(state);
        const bool first = we_move_first(state, m_case.input.move);
        const RiskExpectation& want = m_case.expectation;
        if (danger != want.danger || foe_hp != want.foe_hp_at_our_move || first != want.we_move_first){
            return std::format("D {}, foe HP at our move {}, we move first {}.", danger, foe_hp, first);
        }
        return true;
    }

private:
    RiskCase m_case;
};

const PlannedMove SURF = {.id = "surf", .pp = 15};

const RiskCase RISK_CASES[] = {
    {
        .name = "4. the trainer's Full Restore lands first: no risk, a full foe",
        .input = {.state = {.own = blastoise({}, {}), .foe = lapras_at_12_percent(), .foe_party_alive = 1, .trainer_item_open = true}, .move = SURF},
        .expectation = {.danger = 0, .foe_hp_at_our_move = 220, .we_move_first = true},
    },
    {
        .name = "6. the second Onix's Earthquake",
        .input = {.state = {.own = blastoise({}, {}), .foe = onix(), .foe_party_alive = 2}, .move = SURF},
        .expectation = {.danger = 33, .foe_hp_at_our_move = 118, .we_move_first = true},
    },
    {
        .name = "7. at +0 we outspeed Hitmonchan",
        .input = {.state = {.own = blastoise({}, {}), .foe = {.species = "hitmonchan", .slot = 1, .hp = 131, .max_hp = 131, .speed = 101}}, .move = SURF},
        .expectation = {.danger = 0, .foe_hp_at_our_move = 131, .we_move_first = true},
    },
    {
        .name = "7. after Rock Tomb Hitmonchan moves first",
        .input = {.state = {.own = blastoise_at({}, {}, {.speed = -1}), .foe = {.species = "hitmonchan", .slot = 1, .hp = 131, .max_hp = 131, .speed = 101}}, .move = SURF},
        .expectation = {.danger = 0, .foe_hp_at_our_move = 131, .we_move_first = false},
    },
    {
        .name = "9. Gyarados's Hyper Beam is in the risk",
        .input = {.state = {.own = blastoise({}, {}), .foe = gyarados(), .foe_party_alive = 5}, .move = SURF},
        .expectation = {.danger = 80, .foe_hp_at_our_move = 189, .we_move_first = true},
    },
};


struct ItemSlotInput{
    int carried = 0;
    int used = 0;
    int alive = 0;
};
struct ItemSlotExpectation{
    bool open = false;
};
struct ItemSlotCase{
    std::string name;
    ItemSlotInput input;
    ItemSlotExpectation expectation;
};

class Test_TrainerItemSlots : public UnitTest{
public:
    explicit Test_TrainerItemSlots(ItemSlotCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - pret's trainer item slots: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ItemSlotInput& input = m_case.input;
        if (trainer_item_open(input.carried, input.used, input.alive) != m_case.expectation.open){
            return std::format("Open {}.", !m_case.expectation.open);
        }
        return true;
    }

private:
    ItemSlotCase m_case;
};

const ItemSlotCase ITEM_SLOT_CASES[] = {
    {.name = "an Elite Four member's first", .input = {.carried = 2, .used = 0, .alive = 5}, .expectation = {.open = true}},
    {.name = "the second, five left", .input = {.carried = 2, .used = 1, .alive = 5}, .expectation = {.open = false}},
    {.name = "the second, four left", .input = {.carried = 2, .used = 1, .alive = 4}, .expectation = {.open = true}},
    {.name = "both used", .input = {.carried = 2, .used = 2, .alive = 1}, .expectation = {.open = false}},
    {.name = "the Champion's third, four left", .input = {.carried = 4, .used = 2, .alive = 4}, .expectation = {.open = false}},
    {.name = "the Champion's third, three left", .input = {.carried = 4, .used = 2, .alive = 3}, .expectation = {.open = true}},
};


struct BetweenInput{
    PlannedCarry carry = {};
    //  The fights topped up for, in order.
    std::vector<std::string> fights = {};
};
struct BetweenExpectation{
    int hp = 0;
    int blizzard_pp = 0;
    int max_elixirs = 0;
};
struct BetweenCase{
    std::string name;
    BetweenInput input;
    BetweenExpectation expectation;
};

class Test_PlannedBetween : public UnitTest{
public:
    explicit Test_PlannedBetween(BetweenCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - between: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        PlannedCarry carry = m_case.input.carry;
        for (const std::string& next : m_case.input.fights){
            planned_between(carry, fight(next));
        }
        const BetweenExpectation& want = m_case.expectation;
        const int blizzard = count_of(carry.pp, "blizzard");
        const int max_elixirs = count_of(carry.bag, "max-elixir");
        if (carry.hp != want.hp || blizzard != want.blizzard_pp || max_elixirs != want.max_elixirs){
            return std::format("HP {}, Blizzard {}, Max Elixirs {}.", carry.hp, blizzard, max_elixirs);
        }
        return true;
    }

private:
    BetweenCase m_case;
};

//  Console B, 2026-11-16: Surf 7, Blizzard 0.
const SlugCounts SPENT_PP = {{"bite", 25}, {"strength", 15}, {"surf", 7}, {"blizzard", 0}};

const BetweenCase BETWEEN_CASES[] = {
    {
        .name = "the Max Elixir is held before Lance",
        .input = {.carry = {.hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP, .pp = SPENT_PP, .full_pp = FULL_PP, .bag = without(BAG, "elixir")}, .fights = {"lance"}},
        .expectation = {.hp = OUR_MAX_HP, .blizzard_pp = 0, .max_elixirs = 1},
    },
    {
        .name = "the Max Elixir is used before the Champion",
        .input = {.carry = {.hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP, .pp = SPENT_PP, .full_pp = FULL_PP, .bag = without(BAG, "elixir")}, .fights = {"lance", "champion"}},
        .expectation = {.hp = OUR_MAX_HP, .blizzard_pp = 5, .max_elixirs = 0},
    },
    {
        .name = "Champion loss: entry 89/171 topped up, Blizzard restored",
        .input = {.carry = {.hp = 89, .max_hp = OUR_MAX_HP, .pp = SPENT_PP, .full_pp = FULL_PP, .bag = BAG}, .fights = {"champion"}},
        .expectation = {.hp = OUR_MAX_HP, .blizzard_pp = 5, .max_elixirs = 1},
    },
};


struct DamageInput{
    Gen3Hit hit = {};
};
struct DamageExpectation{
    std::vector<int> rolls = {};
};
struct DamageCase{
    std::string name;
    DamageInput input;
    DamageExpectation expectation;
};

class Test_Gen3Damage : public UnitTest{
public:
    explicit Test_Gen3Damage(DamageCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - Gen 3 damage: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<int> got = gen3_damage_rolls(m_case.input.hit);
        const std::vector<int>& want = m_case.expectation.rolls;
        if (got != want){
            return std::format("Rolls {}..{} ({}); expected {}..{}.",
                got.empty() ? 0 : got.front(), got.empty() ? 0 : got.back(), got.size(), want.front(), want.back());
        }
        return true;
    }

private:
    DamageCase m_case;
};

//  Expected rolls: @smogon/calc 0.10 calculate() for Gen 3 with these raw stats.
const DamageCase DAMAGE_CASES[] = {
    {
        .name = "Lv56 Hyper Beam, 150 Attack on 156 Defense",
        .input = {.hit = {.level = 56, .attack = 150, .defense = 156, .power = 150}},
        .expectation = {.rolls = {60, 61, 61, 62, 63, 63, 64, 65, 66, 66, 67, 68, 68, 69, 70, 71}},
    },
    {
        .name = "the same on -1 Defense",
        .input = {.hit = {.level = 56, .attack = 150, .defense = 156, .power = 150, .defense_stage = -1}},
        .expectation = {.rolls = {89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 105}},
    },
    {
        .name = "Lv54 Surf at +2 on Dragonite, STAB, resisted",
        .input = {.hit = {.level = 54, .attack = 125, .defense = 157, .power = 95, .attack_stage = 2, .stab = true, .effectiveness_first = 0.5}},
        .expectation = {.rolls = {45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 53}},
    },
    {
        .name = "Lv54 Blizzard on Dragonite, 4x",
        .input = {.hit = {.level = 54, .attack = 125, .defense = 157, .power = 120, .effectiveness_first = 2, .effectiveness_second = 2}},
        .expectation = {.rolls = {153, 154, 156, 158, 160, 162, 163, 165, 167, 169, 171, 172, 174, 176, 178, 180}},
    },
    {
        .name = "a critical hit ignores -1 Attack and +2 Defense",
        .input = {.hit = {.level = 56, .attack = 170, .defense = 156, .power = 100, .attack_stage = -1, .defense_stage = 2, .stab = true, .critical = true}},
        .expectation = {.rolls = {137, 139, 140, 142, 144, 145, 147, 149, 150, 152, 153, 155, 157, 158, 160, 162}},
    },
};


struct RecoveryInput{
    std::string item = {};
    int surf_pp = 0;
    int blizzard_pp = 0;
};
struct RecoveryExpectation{
    int surf_pp = 0;
    int blizzard_pp = 0;
};
struct RecoveryCase{
    std::string name;
    RecoveryInput input;
    RecoveryExpectation expectation;
};

class Test_RecoveryItem : public UnitTest{
public:
    explicit Test_RecoveryItem(RecoveryCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        Supplies supplies;
        supplies.lead.moves = {{.move = "surf", .pp = static_cast<uint8_t>(m_case.input.surf_pp)}, {.move = "blizzard", .pp = static_cast<uint8_t>(m_case.input.blizzard_pp)}};
        took_recovery_item(supplies, m_case.input.item);
        const int surf = supplies.lead.moves[0].pp;
        const int blizzard = supplies.lead.moves[1].pp;
        if (surf != m_case.expectation.surf_pp || blizzard != m_case.expectation.blizzard_pp){
            return std::format("Surf {}, Blizzard {}.", surf, blizzard);
        }
        return true;
    }

private:
    RecoveryCase m_case;
};

//  Console B, 2026-11-16: the PP read after the Max Elixir before the
//  Champion still said Blizzard 0/5; took_recovery_item() restored PP for
//  an Elixir only.
const RecoveryCase RECOVERY_CASES[] = {
    {
        .name = "Champion loss: the Max Elixir restores the lead's PP",
        .input = {.item = "max-elixir", .surf_pp = 7, .blizzard_pp = 0},
        .expectation = {.surf_pp = 15, .blizzard_pp = 5},
    },
};


struct FoePlanInput{
    std::string fight = {};
    size_t slot = 0;
    std::string species = {};
};
struct FoePlanExpectation{
    std::string first_move = {};
    size_t setup_goals = 0;
};
struct FoePlanCase{
    std::string name;
    FoePlanInput input;
    FoePlanExpectation expectation;
};

class Test_LeagueFoePlan : public UnitTest{
public:
    explicit Test_LeagueFoePlan(FoePlanCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - League plan built in: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const FoePlanInput& input = m_case.input;
        const LeagueFoePlan* foe = league_foe_plan(fight(input.fight), input.slot, input.species);
        if (foe == nullptr){
            return "Not found.";
        }
        const std::string first = foe->moves.empty() ? "" : foe->moves.front();
        if (first != m_case.expectation.first_move || foe->setup.size() != m_case.expectation.setup_goals){
            return std::format("First move {}, {} setup goals.", first, foe->setup.size());
        }
        return true;
    }

private:
    FoePlanCase m_case;
};

const FoePlanCase FOE_PLAN_CASES[] = {
    {
        .name = "Bruno's second Onix, by its slot",
        .input = {.fight = "bruno", .slot = 3, .species = "onix"},
        .expectation = {.first_move = "surf", .setup_goals = 0},
    },
    {
        .name = "Dragonite at another slot, by species",
        .input = {.fight = "lance", .slot = 0, .species = "dragonite"},
        .expectation = {.first_move = "blizzard", .setup_goals = 0},
    },
};

} // namespace


void add_tests_AutoStoryPlannedPolicy(UnitTestDatabase& database){
    for (const DamageCase& test_case : DAMAGE_CASES){
        database.add<Test_Gen3Damage>(test_case);
    }
    for (const PolicyCase& test_case : POLICY_CASES){
        database.add<Test_PlannedChoice>(test_case);
    }
    for (const RiskCase& test_case : RISK_CASES){
        database.add<Test_PlannedRisk>(test_case);
    }
    for (const ItemSlotCase& test_case : ITEM_SLOT_CASES){
        database.add<Test_TrainerItemSlots>(test_case);
    }
    for (const BetweenCase& test_case : BETWEEN_CASES){
        database.add<Test_PlannedBetween>(test_case);
    }
    for (const RecoveryCase& test_case : RECOVERY_CASES){
        database.add<Test_RecoveryItem>(test_case);
    }
    for (const FoePlanCase& test_case : FOE_PLAN_CASES){
        database.add<Test_LeagueFoePlan>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
