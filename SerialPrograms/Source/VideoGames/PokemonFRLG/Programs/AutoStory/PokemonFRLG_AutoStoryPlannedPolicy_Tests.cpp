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

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryGen3Damage.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"

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
        .speed = battle_speed(badge_boosted(OUR_SPEED), 0, false),
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


//  A test whose body is a function: one per sim case.
struct BodyCase{
    std::string name = {};
    std::function<UnitTestResult()> body = {};
};

class Test_PlannedPolicy : public UnitTest{
public:
    explicit Test_PlannedPolicy(BodyCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.body();
    }

private:
    BodyCase m_case;
};


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


UnitTestResult blizzard_reserved_before_lance(){
    //  The old rows' setup is complete: they attacked with Blizzard.
    PlannedState golbat_state{.own = blastoise({.bite = rolls(40, 48), .strength = rolls(30, 36), .surf = rolls(50, 59), .blizzard = rolls(70, 83)}, {}), .foe = golbat(), .foe_party_alive = 4, .bag = BAG};
    golbat_state.own.spa_stage = 3;
    golbat_state.own.speed_stage = 6;
    golbat_state.own.accuracy_stage = 2;
    const PlannedChoice at_golbat = choose_planned(golbat_state, fight("agatha"), 0);
    if (move_id(golbat_state, at_golbat) == "blizzard"){
        return "Golbat: " + describe(golbat_state, at_golbat);
    }
    PlannedState gyarados_state{.own = blastoise({.bite = rolls(40, 48), .strength = rolls(30, 36), .surf = rolls(25, 30), .blizzard = rolls(45, 54)}, {}), .foe = gyarados(), .foe_party_alive = 5, .bag = BAG};
    gyarados_state.own.spa_stage = 3;
    gyarados_state.own.speed_stage = 6;
    gyarados_state.own.accuracy_stage = 2;
    const PlannedChoice at_gyarados = choose_planned(gyarados_state, fight("lance"), 0);
    if (move_id(gyarados_state, at_gyarados) == "blizzard"){
        return "Gyarados: " + describe(gyarados_state, at_gyarados);
    }
    return true;
}

UnitTestResult setup_goal_needs_its_moves_pp(){
    const OurHits hits = {.bite = rolls(40, 48), .strength = rolls(30, 36), .surf = rolls(25, 30), .blizzard = rolls(45, 54)};
    const PlannedState empty{.own = blastoise(hits, {{"blizzard", 0}}), .foe = gyarados(), .foe_party_alive = 5, .bag = NO_ELIXIRS};
    const PlannedChoice without_pp = choose_planned(empty, fight("lance"), 0);
    if (without_pp.item == "x-accuracy" || without_pp.item == "x-special"){
        return "Blizzard at 0 PP: " + describe(empty, without_pp);
    }
    const PlannedState full{.own = blastoise(hits, {}), .foe = gyarados(), .foe_party_alive = 5, .bag = BAG};
    const PlannedChoice with_pp = choose_planned(full, fight("lance"), 0);
    if (with_pp.row != "plan:setup" || with_pp.item != "x-special"){
        return "Blizzard with PP: " + describe(full, with_pp) + "; expected plan:setup (x-special).";
    }
    return true;
}

UnitTestResult dragonite_without_blizzard_takes_bite(){
    PlannedState state{.own = blastoise(ON_DRAGONITE, {{"blizzard", 0}}), .foe = dragonite(), .foe_party_alive = 1, .bag = NO_ELIXIRS};
    state.own.spa_stage = 3;
    state.own.speed_stage = 6;
    state.own.accuracy_stage = 2;
    const PlannedChoice choice = choose_planned(state, fight("lance"), 0);
    if (move_id(state, choice) != "bite"){
        return describe(state, choice) + "; expected Bite.";
    }
    return true;
}

PlannedState lapras_at_12_percent(){
    PlannedFoe lapras{.species = "lapras", .slot = 4, .hp = 26, .max_hp = 220, .speed = 94,
        .moves = {{.id = "body-slam", .pp = 15, .damage = rolls(30, 36)}}};
    PlannedState state{.own = blastoise({.bite = rolls(20, 24), .strength = rolls(30, 36), .surf = rolls(25, 30), .blizzard = rolls(20, 24)}, {}),
        .foe = lapras, .foe_party_alive = 1, .trainer_item_open = true, .bag = BAG};
    state.own.spa_stage = 3;
    return state;
}

UnitTestResult no_finisher_into_a_full_restore(){
    const PlannedState state = lapras_at_12_percent();
    if (foe_hp_at_our_move(state) != state.foe.max_hp){
        return std::format("Foe HP at our move {}; expected {}.", foe_hp_at_our_move(state), state.foe.max_hp);
    }
    if (planned_danger(state).normal_max != 0){
        return std::format("D {} while the trainer heals; expected 0.", planned_danger(state).normal_max);
    }
    const PlannedChoice choice = choose_planned(state, fight("lorelei"), 0);
    if (choice.row == "sure_ko"){
        return describe(state, choice);
    }
    return true;
}

UnitTestResult pret_trainer_item_slots(){
    struct Row{
        int used;
        int alive;
        bool open;
    };
    //  An Elite Four member's two Full Restores.
    for (const Row& row : {Row{0, 5, true}, Row{1, 5, false}, Row{1, 4, true}, Row{2, 1, false}}){
        if (trainer_item_open(/*carried=*/2, row.used, row.alive) != row.open){
            return std::format("{} used, {} alive: open {}; expected {}.", row.used, row.alive, !row.open, row.open);
        }
    }
    //  The Champion's four: the third opens at three left.
    if (trainer_item_open(/*carried=*/4, /*used=*/2, /*alive=*/4) || !trainer_item_open(/*carried=*/4, /*used=*/2, /*alive=*/3)){
        return "The Champion's third Full Restore should open at three Pokemon left.";
    }
    return true;
}

UnitTestResult no_super_potion_that_does_not_outpace_the_foe(){
    PlannedState state{.own = blastoise({.bite = rolls(30, 36), .strength = rolls(20, 24), .surf = rolls(25, 30), .blizzard = rolls(90, 106)}, {}), .foe = dragonair(), .foe_party_alive = 4, .bag = BAG};
    state.own.hp = 50;
    const PlannedChoice first = choose_planned(state, fight("lance"), 0);
    if (first.row != "ko_heal" || first.item == "super-potion"){
        return describe(state, first) + "; expected ko_heal, not a Super Potion.";
    }
    const PlannedChoice third = choose_planned(state, fight("lance"), /*streak=*/2);
    if (third.row != "ko_heal" || third.item != "full-restore"){
        return "Third medicine turn: " + describe(state, third) + "; expected ko_heal (full-restore).";
    }
    return true;
}

UnitTestResult ko_risk_uses_the_current_foe(){
    //  The second Onix's own hits, not Machamp's row.
    PlannedFoe onix{.species = "onix", .slot = 3, .hp = 118, .max_hp = 118, .speed = 96,
        .moves = {{.id = "earthquake", .pp = 10, .damage = rolls(28, 33)}}};
    PlannedState state{.own = blastoise({.bite = rolls(40, 48), .strength = rolls(20, 24), .surf = rolls(130, 154), .blizzard = rolls(60, 72)}, {}), .foe = onix, .foe_party_alive = 2, .bag = BAG};
    state.own.hp = 60;
    state.own.spa_stage = 2;
    state.own.speed_stage = 1;
    if (planned_danger(state).normal_max >= 60){
        return std::format("D {}; expected under 60.", planned_danger(state).normal_max);
    }
    const PlannedChoice choice = choose_planned(state, fight("bruno"), 0);
    if (!choice.move_slot){
        return describe(state, choice) + "; expected an attack.";
    }
    return true;
}

UnitTestResult speed_stages_decide_who_moves_first(){
    PlannedFoe hitmonchan{.species = "hitmonchan", .slot = 1, .hp = 131, .max_hp = 131, .speed = 101};
    PlannedState state{.own = blastoise({}, {}), .foe = hitmonchan};
    const PlannedMove surf{.id = "surf", .pp = 15};
    if (!we_move_first(state, surf)){
        return std::format("At +0 ({} Speed) we should move first.", state.own.speed);
    }
    state.own.speed = battle_speed(badge_boosted(OUR_SPEED), -1, false);
    if (we_move_first(state, surf)){
        return std::format("After Rock Tomb ({} Speed) Hitmonchan should move first.", state.own.speed);
    }
    return true;
}

UnitTestResult evasion_is_no_sure_ko(){
    const double accuracy = hit_chance(/*accuracy=*/100, /*accuracy_stage=*/0, /*evasion_stage=*/3);
    if (accuracy >= 1){
        return "Surf against +3 evasion should be able to miss.";
    }
    PlannedFoe gengar{.species = "gengar", .slot = 0, .hp = 40, .max_hp = 145, .speed = 126,
        .moves = {{.id = "shadow-ball", .pp = 15, .damage = rolls(30, 36)}}};
    PlannedState state{.own = blastoise({.bite = rolls(50, 60), .strength = {}, .surf = rolls(45, 54), .blizzard = rolls(40, 48)}, {}), .foe = gengar, .foe_party_alive = 1, .bag = BAG};
    state.own.speed = battle_speed(badge_boosted(OUR_SPEED), 1, false);
    state.own.spa_stage = 2;
    for (PlannedMove& move : state.own.moves){
        move.accuracy = accuracy;
    }
    const PlannedChoice choice = choose_planned(state, fight("agatha"), 0);
    if (choice.row == "sure_ko"){
        return describe(state, choice);
    }
    return true;
}

UnitTestResult gyarados_hyper_beam_is_in_the_risk(){
    PlannedState state{.own = blastoise({}, {}), .foe = gyarados(), .foe_party_alive = 5, .bag = BAG};
    state.own.hp = 101;
    if (planned_danger(state).normal_max != 80){
        return std::format("D {}; expected Hyper Beam's 80.", planned_danger(state).normal_max);
    }
    return true;
}

UnitTestResult kill_order_keeps_the_foe_out_of_heal_range(){
    //  Machamp 79/183: Surf leaves 4-16.
    PlannedFoe machamp{.species = "machamp", .slot = 4, .hp = 79, .max_hp = 183, .speed = 83,
        .moves = {{.id = "cross-chop", .pp = 5, .accuracy = 0.8, .damage = rolls(60, 71)}}};
    PlannedState state{.own = blastoise({.bite = rolls(25, 30), .strength = rolls(20, 24), .surf = rolls(63, 75), .blizzard = rolls(40, 48)}, {}),
        .foe = machamp, .foe_party_alive = 1, .trainer_item_open = true, .bag = BAG};
    const PlannedChoice choice = choose_planned(state, fight("bruno"), 0);
    if (choice.row != "kill_order" || !choice.move_slot){
        return describe(state, choice) + "; expected kill_order.";
    }
    const PlannedMove& opener = state.own.moves[*choice.move_slot];
    if (79 - opener.damage.back() < 183 / 4){
        return std::format("{} can leave Machamp under 1/4.", opener.id);
    }
    return true;
}

UnitTestResult max_elixir_is_held_until_the_champion(){
    PlannedState state{.own = blastoise(ON_DRAGONITE, {{"blizzard", 0}, {"bite", 0}, {"surf", 0}}), .foe = dragonite(), .foe_party_alive = 1, .bag = BAG};
    state.bag["elixir"] = 0;
    const PlannedChoice choice = choose_planned(state, fight("lance"), 0);
    if (choice.item == "max-elixir"){
        return "In Lance's fight: " + describe(state, choice);
    }
    PlannedCarry carry{.hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .full_pp = FULL_PP, .bag = state.bag};
    carry.pp["blizzard"] = 0;
    carry.pp["surf"] = 7;
    planned_between(carry, fight("lance"));
    if (carry.bag["max-elixir"] != 1){
        return "The Max Elixir was used before Lance.";
    }
    planned_between(carry, fight("champion"));
    if (carry.bag["max-elixir"] != 0 || carry.pp["blizzard"] != 5){
        return std::format("Before the Champion: {} Max Elixirs, Blizzard {}; expected 0 and 5.", carry.bag["max-elixir"], carry.pp["blizzard"]);
    }
    return true;
}

UnitTestResult champion_venusaur_takes_blizzard(){
    //  Console B, 2026-11-16: entry 89/171, Surf 7, Blizzard 0; Venusaur took Strength.
    PlannedCarry carry{.hp = 89, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .full_pp = FULL_PP, .bag = BAG};
    carry.pp["blizzard"] = 0;
    carry.pp["surf"] = 7;
    const std::vector<std::string> used = planned_between(carry, fight("champion"));
    if (used.empty() || carry.hp != OUR_MAX_HP || carry.pp["blizzard"] != 5){
        return std::format("Between: {} items, HP {}, Blizzard {}; expected some, 171 and 5.", used.size(), carry.hp, carry.pp["blizzard"]);
    }
    PlannedFoe venusaur{.species = "venusaur", .slot = 5, .hp = 193, .max_hp = 193, .speed = 125,
        .moves = {{.id = "razor-leaf", .pp = 25, .accuracy = 0.95, .damage = rolls(60, 71)}}};
    PlannedState state{.own = blastoise({.bite = rolls(30, 36), .strength = rolls(25, 30), .surf = rolls(20, 24), .blizzard = rolls(95, 112)}, carry.pp),
        .foe = venusaur, .foe_party_alive = 1, .bag = carry.bag};
    const PlannedChoice choice = choose_planned(state, fight("champion"), 0);
    if (move_id(state, choice) != "blizzard"){
        return describe(state, choice) + "; expected Blizzard.";
    }
    return true;
}

UnitTestResult league_plan_is_built_in(){
    const LeaguePlanData& plan = league_plan_data();
    if (plan.fights.size() != 5 || plan.fights.back().name != "champion" || plan.fights.back().foes.size() != 6){
        return std::format("{} fights; expected the Elite Four and the Champion's six.", plan.fights.size());
    }
    const LeagueFightData& lance = fight("lance");
    if (spendable(lance, BAG, "max-elixir") != 0 || spendable(lance, BAG, "full-restore") != 6){
        return "Lance's reserve should hold the Max Elixir and 2 Full Restores.";
    }
    const LeagueFoePlan* second_onix = league_foe_plan(fight("bruno"), 3, "onix");
    if (second_onix == nullptr || !second_onix->setup.empty()){
        return "Bruno's second Onix should be found by its slot, with no setup.";
    }
    const LeagueFoePlan* moved = league_foe_plan(fight("lance"), 0, "dragonite");
    if (moved == nullptr || moved->moves.front() != "blizzard"){
        return "A foe at another slot should be found by species.";
    }
    if (league_fight_name("TRAINER_CHAMPION_FIRST_BULBASAUR") != "champion" || league_fight_name("TRAINER_LEADER_BROCK")){
        return "League fight names by trainer.";
    }
    return true;
}

UnitTestResult league_plan_rejects_unknown_items(){
    try{
        parse_league_plan(R"({"format": "frlg-league-plan/1", "fights": {"lorelei": {"foes": [], "reserve": {"items": {"rarecandy": 1}}}}})");
    }catch (const ParseException&){
        return true;
    }
    return "An unknown item should not parse.";
}


struct DamageCase{
    std::string name = {};
    Gen3Hit input = {};
    std::vector<int> expectation = {};
};

class Test_Gen3Damage : public UnitTest{
public:
    explicit Test_Gen3Damage(DamageCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedPolicy - Gen 3 damage: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<int> got = gen3_damage_rolls(m_case.input);
        if (got != m_case.expectation){
            return std::format("Rolls {}..{} ({}); expected {}..{}.",
                got.empty() ? 0 : got.front(), got.empty() ? 0 : got.back(), got.size(),
                m_case.expectation.front(), m_case.expectation.back());
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
        .input = {.level = 56, .attack = 150, .defense = 156, .power = 150},
        .expectation = {60, 61, 61, 62, 63, 63, 64, 65, 66, 66, 67, 68, 68, 69, 70, 71},
    },
    {
        .name = "the same on -1 Defense",
        .input = {.level = 56, .attack = 150, .defense = 156, .power = 150, .defense_stage = -1},
        .expectation = {89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 105},
    },
    {
        .name = "Lv54 Surf at +2 on Dragonite, STAB, resisted",
        .input = {.level = 54, .attack = 125, .defense = 157, .power = 95, .attack_stage = 2, .stab = true, .effectiveness_first = 0.5},
        .expectation = {45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 53},
    },
    {
        .name = "Lv54 Blizzard on Dragonite, 4x",
        .input = {.level = 54, .attack = 125, .defense = 157, .power = 120, .effectiveness_first = 2, .effectiveness_second = 2},
        .expectation = {153, 154, 156, 158, 160, 162, 163, 165, 167, 169, 171, 172, 174, 176, 178, 180},
    },
    {
        .name = "a critical hit ignores -1 Attack and +2 Defense",
        .input = {.level = 56, .attack = 170, .defense = 156, .power = 100, .attack_stage = -1, .defense_stage = 2, .stab = true, .critical = true},
        .expectation = {137, 139, 140, 142, 144, 145, 147, 149, 150, 152, 153, 155, 157, 158, 160, 162},
    },
};

const BodyCase BODY_CASES[] = {
    {.name = "League plan built in", .body = league_plan_is_built_in},
    {.name = "League plan: unknown item", .body = league_plan_rejects_unknown_items},
    {.name = "1. Blizzard reserved before Lance, not spent on Golbat or Gyarados", .body = blizzard_reserved_before_lance},
    {.name = "2. a setup goal needs its move's PP", .body = setup_goal_needs_its_moves_pp},
    {.name = "3. Dragonite without Blizzard takes Bite", .body = dragonite_without_blizzard_takes_bite},
    {.name = "4. no finisher into a trainer's Full Restore", .body = no_finisher_into_a_full_restore},
    {.name = "4. pret's trainer item slots", .body = pret_trainer_item_slots},
    {.name = "5. no Super Potion that doesn't outpace the foe", .body = no_super_potion_that_does_not_outpace_the_foe},
    {.name = "6. KO risk from the current foe", .body = ko_risk_uses_the_current_foe},
    {.name = "7. Speed stages decide who moves first", .body = speed_stages_decide_who_moves_first},
    {.name = "8. evasion is no sure KO", .body = evasion_is_no_sure_ko},
    {.name = "9. Gyarados's Hyper Beam is in the risk", .body = gyarados_hyper_beam_is_in_the_risk},
    {.name = "kill order keeps the foe out of heal range", .body = kill_order_keeps_the_foe_out_of_heal_range},
    {.name = "the Max Elixir is held until the Champion", .body = max_elixir_is_held_until_the_champion},
    {.name = "Champion loss: Venusaur takes Blizzard", .body = champion_venusaur_takes_blizzard},
};

} // namespace


void add_tests_AutoStoryPlannedPolicy(UnitTestDatabase& database){
    for (const DamageCase& test_case : DAMAGE_CASES){
        database.add<Test_Gen3Damage>(test_case);
    }
    for (const BodyCase& test_case : BODY_CASES){
        database.add<Test_PlannedPolicy>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
