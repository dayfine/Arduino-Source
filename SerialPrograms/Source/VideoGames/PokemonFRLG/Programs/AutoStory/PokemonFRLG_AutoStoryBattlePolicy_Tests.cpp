/*  AutoStory Battle Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PokeGuy's battle policy (PokemonFRLG_AutoStoryBattlePolicy.h) on
 *  scripted battle states, Koga's among them, and the plan steps that
 *  name it.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Koga's foes as the plan carries them: Koffing, Muk, Koffing, Weezing
//  against a Blastoise Lv39 (HP 122, Speed 81, 89 with the Thunder Badge).
const std::vector<FoeThreat> KOGA_FOES = {
    {.speed = 30, .likely = 73, .mean = 26, .max = 166, .normal_max = 84, .lowers_accuracy = true, .explodes = true},
    {.speed = 48, .likely = 28, .mean = 8, .max = 63, .normal_max = 33},
    {.speed = 30, .likely = 73, .mean = 26, .max = 166, .normal_max = 84, .lowers_accuracy = true, .explodes = true},
    {.speed = 56, .likely = 33, .mean = 13, .max = 75, .normal_max = 39, .lowers_accuracy = true},
};

BattlePolicy koga_policy(){
    return BattlePolicy{
        .kind = BattlePolicyKind::GOAL_BASED_SETUP,
        .goals = {.spa = 1, .accuracy = 0, .outspeed = true, .guard_accuracy = false},
        .foes = KOGA_FOES,
    };
}

PolicyView koga_view(double hp, size_t foe_index = 0, StatusCondition status = StatusCondition::NONE){
    return PolicyView{.hp = hp, .max_hp = 122, .status = status, .speed = 81, .foe_index = foe_index};
}

Supplies koga_bag(){
    Supplies supplies;
    set_from_bag(supplies, {
        {"antidote", 1}, {"full-restore", 1}, {"super-potion", 5}, {"potion", 5},
        {"x-special", 19}, {"x-speed", 6}, {"x-accuracy", 5}, {"guard-spec", 2},
    });
    return supplies;
}

//  What the policy uses, or "" for an attack; the item is used, too.
std::string use(PolicyState& state, Supplies& supplies, const PolicyView& view){
    const std::optional<PolicyItem> item = state.choose(supplies, view);
    if (!item){
        return "";
    }
    state.item_used(item->item);
    return item->item;
}

UnitTestResult test_koga_opening(){
    PolicyState state(koga_policy());
    Supplies supplies = koga_bag();
    //  Faster than every Fuchsia foe with the Thunder Badge (89 against 56):
    //  no X Speed. One X Special for Sp. Atk +1, then Surf.
    const std::string first = use(state, supplies, koga_view(122));
    if (first != "x-special"){
        return std::format("Expected X Special at turn 1, got \"{}\".", first);
    }
    if (state.stages().spa != 1){
        return "Expected Sp. Atk +1 after the X Special.";
    }
    const std::string second = use(state, supplies, koga_view(122));
    if (!second.empty()){
        return std::format("Expected an attack at turn 2, got \"{}\".", second);
    }
    return true;
}

UnitTestResult test_x_speed_when_slower(){
    BattlePolicy policy = koga_policy();
    policy.foes[3].speed = 120;
    PolicyState state(policy);
    Supplies supplies = koga_bag();
    //  The foe out is slow, but Weezing comes later and is faster: the goal
    //  is every foe left.
    if (use(state, supplies, koga_view(122)) != "x-special"){
        return "Expected X Special first.";
    }
    const std::string next = use(state, supplies, koga_view(122));
    if (next != "x-speed"){
        return std::format("Expected X Speed against a faster foe left, got \"{}\".", next);
    }
    if (!use(state, supplies, koga_view(122)).empty() || state.stages().speed != 1){
        return "Expected an attack at Speed +1 (89 x1.5 = 133 over 120).";
    }
    //  With Weezing out, it is the only foe left to outspeed.
    PolicyState later(policy);
    later.item_used("x-special");
    if (use(later, supplies, koga_view(122, 3)) != "x-speed"){
        return "Expected X Speed against the Weezing that is faster.";
    }
    //  Past it, no foe is faster: the goal is met.
    PolicyState done(koga_policy());
    done.item_used("x-special");
    if (!use(done, supplies, koga_view(122, 1)).empty()){
        return "Expected an attack against Muk, faster than no foe left.";
    }
    return true;
}

UnitTestResult test_poison_antidote(){
    PolicyState state(koga_policy());
    Supplies supplies = koga_bag();
    //  Poison takes 15 a turn: 100 HP is over Koffing's likely 73 + 15.
    const std::string cured = use(state, supplies, koga_view(100, 0, StatusCondition::POISON));
    if (cured != "antidote"){
        return std::format("Expected the Antidote at 100/122 poisoned, got \"{}\".", cured);
    }
    //  Without one: a heal, not a cure.
    PolicyState other(koga_policy());
    Supplies none = koga_bag();
    none.antidotes = 0;
    const std::string healed = use(other, none, koga_view(100, 0, StatusCondition::POISON));
    if (healed == "antidote"){
        return "Expected no Antidote from a bag with none.";
    }
    return true;
}

UnitTestResult test_heal_before_self_destruct(){
    PolicyState state(koga_policy());
    state.item_used("x-special");
    Supplies supplies = koga_bag();
    //  80 HP is under Koffing's biggest ordinary hit (84): a heal first.
    const std::string heal = use(state, supplies, koga_view(80));
    if (heal != "super-potion" && heal != "full-restore"){
        return std::format("Expected a heal at 80/122 against Koffing, got \"{}\".", heal);
    }
    //  The same HP against Muk (33) is safe: attack.
    PolicyState muk(koga_policy());
    muk.item_used("x-special");
    const std::string attack = use(muk, supplies, koga_view(80, 1));
    if (!attack.empty()){
        return std::format("Expected an attack at 80/122 against Muk, got \"{}\".", attack);
    }
    return true;
}

UnitTestResult test_low_hp_heals(){
    PolicyState state(koga_policy());
    state.item_used("x-special");
    Supplies supplies = koga_bag();
    const std::string heal = use(state, supplies, koga_view(34));
    if (heal != "super-potion" && heal != "full-restore"){
        return std::format("Expected a heal at 34/122, got \"{}\".", heal);
    }
    Supplies empty;
    set_from_bag(empty, {{"x-special", 3}});
    PolicyState dry(koga_policy());
    dry.item_used("x-special");
    if (!use(dry, empty, koga_view(34)).empty()){
        return "Expected an attack with no medicine in the bag.";
    }
    return true;
}

UnitTestResult test_guard_spec_then_accuracy(){
    BattlePolicy policy = koga_policy();
    policy.goals = {.spa = 0, .accuracy = 2, .outspeed = false, .guard_accuracy = true};
    PolicyState state(policy);
    Supplies supplies = koga_bag();
    const std::string guard = use(state, supplies, koga_view(122));
    if (guard != "guard-spec" || state.stages().mist_turns <= 0){
        return std::format("Expected Guard Spec first against Koffing, got \"{}\".", guard);
    }
    for (int c = 0; c < 2; c++){
        const std::string accuracy = use(state, supplies, koga_view(122));
        if (accuracy != "x-accuracy"){
            return std::format("Expected X Accuracy {}, got \"{}\".", c + 1, accuracy);
        }
    }
    if (state.stages().accuracy != 2 || !use(state, supplies, koga_view(122)).empty()){
        return "Expected an attack at accuracy +2.";
    }
    //  Muk has no such move: no Guard Spec.
    PolicyState muk(policy);
    if (use(muk, supplies, koga_view(122, 1)) != "x-accuracy"){
        return "Expected X Accuracy first against Muk.";
    }
    return true;
}

UnitTestResult test_foe_evasion_raises_accuracy_goal(){
    BattlePolicy policy = koga_policy();
    policy.goals = {.spa = 0, .accuracy = 0, .outspeed = false, .guard_accuracy = false};
    PolicyState state(policy);
    Supplies supplies = koga_bag();
    if (!use(state, supplies, koga_view(122)).empty()){
        return "Expected an attack with accuracy 0 of 0.";
    }
    state.foe_evasion_raised(2);
    if (use(state, supplies, koga_view(122)) != "x-accuracy"){
        return "Expected X Accuracy against evasion +2.";
    }
    return true;
}

UnitTestResult test_torrent_power(){
    BattlePolicy policy;
    policy.kind = BattlePolicyKind::GOAL_BASED_SETUP;
    policy.goals = {.spa = 2, .torrent_power = true};
    Supplies supplies;
    set_from_bag(supplies, {{"x-special", 10}});
    PolicyState full(policy);
    int used = 0;
    while (!use(full, supplies, PolicyView{.hp = 100, .max_hp = 100, .speed = 81}).empty() && used < 10){
        used++;
    }
    //  Torrent off: +2 would lose 1.5 to Torrent; +4 (3.0) makes it up.
    if (used != 4){
        return std::format("Expected 4 X Specials while Torrent is off, used {}.", used);
    }
    PolicyState low(policy);
    used = 0;
    while (!use(low, supplies, PolicyView{.hp = 30, .max_hp = 100, .speed = 81}).empty() && used < 10){
        used++;
    }
    if (used != 2){
        return std::format("Expected 2 X Specials with Torrent on, used {}.", used);
    }
    return true;
}

UnitTestResult test_solo_recovery_no_setup(){
    BattlePolicy policy = koga_policy();
    policy.kind = BattlePolicyKind::SOLO_RECOVERY;
    PolicyState state(policy);
    Supplies supplies = koga_bag();
    if (!use(state, supplies, koga_view(122)).empty()){
        return "Expected no X item under solo-recovery.";
    }
    if (use(state, supplies, koga_view(100, 0, StatusCondition::POISON)) != "antidote"){
        return "Expected the Antidote under solo-recovery.";
    }
    PolicyState none{BattlePolicy{}};
    if (!use(none, supplies, koga_view(10)).empty()){
        return "Expected nothing from a step with no policy.";
    }
    return true;
}

UnitTestResult test_sleep_wakes_before_setup(){
    PolicyState state(koga_policy());
    Supplies supplies = koga_bag();
    set_from_bag(supplies, {{"awakening", 3}, {"x-special", 5}});
    const std::string woke = use(state, supplies, koga_view(122, 1, StatusCondition::SLEEP));
    if (woke != "awakening"){
        return std::format("Expected an Awakening asleep, got \"{}\".", woke);
    }
    return true;
}

UnitTestResult test_foe_rows(){
    if (foe_threats_of({{1, 2, 3, 4, 5, 0}})){
        return "Expected a row of 6 to be refused.";
    }
    const std::optional<std::vector<FoeThreat>> rows = foe_threats_of({{30, 73, 26, 166, 84, 1, 1}});
    if (!rows || rows->size() != 1 || (*rows)[0].speed != 30 || !(*rows)[0].lowers_accuracy || !(*rows)[0].explodes){
        return "Expected the row to read.";
    }
    if (battle_policy_kind("goal-based-setup") != BattlePolicyKind::GOAL_BASED_SETUP || battle_policy_kind("x")){
        return "Expected the policy words to read.";
    }
    return true;
}


const StoryStep& battle_step(std::string_view id, std::string_view name){
    for (const Pokemon::StoryCheckpoint& row : story_checkpoints()){
        if (row.id != id){
            continue;
        }
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::BATTLE && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No battle {} in {}.", name, id));
}

template <typename Type>
std::optional<Type> arg_of(const StoryStep& step, std::string_view key){
    const auto found = step.args.find(key);
    if (found == step.args.end()){
        return std::nullopt;
    }
    if (const auto* value = std::get_if<Type>(&found->second)){
        return *value;
    }
    return std::nullopt;
}

//  The step names the policy, its goals, and one threat row per foe.
UnitTestResult check_policy_step(std::string_view id, std::string_view name, const SetupGoals& goals){
    const int64_t spa = goals.spa;
    const int64_t accuracy = goals.accuracy;
    const bool outspeed = goals.outspeed;
    const bool guard = goals.guard_accuracy;
    const bool torrent = goals.torrent_power;
    const StoryStep& step = battle_step(id, name);
    if (arg_of<std::string>(step, "policy") != "goal-based-setup"){
        return std::format("{} {}: expected policy goal-based-setup.", id, name);
    }
    if (arg_of<int64_t>(step, "setup_spa") != spa || arg_of<bool>(step, "setup_outspeed").value_or(false) != outspeed
        || arg_of<bool>(step, "setup_guard_accuracy").value_or(false) != guard
        || arg_of<bool>(step, "setup_torrent_power").value_or(false) != torrent
    ){
        return std::format("{} {}: the setup goals differ from the doc's.", id, name);
    }
    if (arg_of<int64_t>(step, "setup_accuracy").value_or(0) != accuracy){
        return std::format("{} {}: expected setup_accuracy {}.", id, name, accuracy);
    }
    const auto foes = arg_of<std::vector<std::string>>(step, "foes");
    const auto rows = arg_of<std::vector<std::vector<int64_t>>>(step, "foe_threats");
    if (!foes || !rows || !foe_threats_of(*rows) || rows->size() != foes->size()){
        return std::format("{} {}: expected one valid foe_threats row per foe.", id, name);
    }
    return true;
}

UnitTestResult test_plan_koga(){
    const UnitTestResult result = check_policy_step("12.02", "koga", {.spa = 1, .outspeed = true});
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    const StoryStep& step = battle_step("12.02", "koga");
    if (arg_of<std::vector<std::vector<int64_t>>>(step, "foe_threats") != std::vector<std::vector<int64_t>>{
        {30, 73, 26, 166, 84, 1, 1}, {48, 28, 8, 63, 33, 0, 0}, {30, 73, 26, 166, 84, 1, 1}, {56, 33, 13, 75, 39, 1, 0},
    }){
        return "Expected Koga's foe rows from the doc's stats.";
    }
    return true;
}

UnitTestResult test_plan_tower_rival(){
    return check_policy_step("10.03", "rival_tower", {.spa = 2, .outspeed = true, .guard_accuracy = true, .torrent_power = true});
}

UnitTestResult test_plan_erika(){
    return check_policy_step("14.04", "erika", {.spa = 1, .accuracy = 2, .outspeed = true, .guard_accuracy = true});
}


struct AutoStoryBattlePolicyInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryBattlePolicyExpectation{
    std::string_view description;
};

struct AutoStoryBattlePolicyCase{
    AutoStoryBattlePolicyInput input;
    AutoStoryBattlePolicyExpectation expectation;
};

class Test_AutoStoryBattlePolicy : public UnitTest{
public:
    explicit Test_AutoStoryBattlePolicy(AutoStoryBattlePolicyCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - BattlePolicy: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryBattlePolicyCase m_case;
};

const std::vector<AutoStoryBattlePolicyCase> AUTO_STORY_BATTLE_POLICY_CASES = {
    {
        .input = {.name = "Koga: X Special, no X Speed when faster, then Surf", .body = test_koga_opening},
        .expectation = {.description = "one X Special, then an attack"},
    },
    {
        .input = {.name = "X Speed against a faster foe left", .body = test_x_speed_when_slower},
        .expectation = {.description = "X Speed while a foe left is faster"},
    },
    {
        .input = {.name = "poison: the Antidote while the HP outlasts a turn", .body = test_poison_antidote},
        .expectation = {.description = "the Antidote, none from an empty bag"},
    },
    {
        .input = {.name = "a heal before Self-Destruct, not against Muk", .body = test_heal_before_self_destruct},
        .expectation = {.description = "a heal under the foe's largest hit"},
    },
    {
        .input = {.name = "low HP heals; no medicine attacks", .body = test_low_hp_heals},
        .expectation = {.description = "a heal at low HP, an attack with none"},
    },
    {
        .input = {.name = "Guard Spec, then X Accuracy; none against a foe without the move", .body = test_guard_spec_then_accuracy},
        .expectation = {.description = "Guard Spec first, then the accuracy goal"},
    },
    {
        .input = {.name = "the foe's evasion raises the accuracy goal", .body = test_foe_evasion_raises_accuracy_goal},
        .expectation = {.description = "X Accuracy against evasion"},
    },
    {
        .input = {.name = "torrent_power: a higher Sp. Atk goal while Torrent is off", .body = test_torrent_power},
        .expectation = {.description = "4 X Specials off, 2 on"},
    },
    {
        .input = {.name = "solo-recovery cures and heals without setup", .body = test_solo_recovery_no_setup},
        .expectation = {.description = "no X item under B"},
    },
    {
        .input = {.name = "asleep: an Awakening first", .body = test_sleep_wakes_before_setup},
        .expectation = {.description = "an Awakening"},
    },
    {
        .input = {.name = "the step's rows and policy words read", .body = test_foe_rows},
        .expectation = {.description = "rows of 7 read, others refused"},
    },
    {
        .input = {.name = "plan: 12.02 koga names the policy, goals and foes", .body = test_plan_koga},
        .expectation = {.description = "the policy, goals and 4 foe rows"},
    },
    {
        .input = {.name = "plan: 14.04 erika names the policy, goals and foes", .body = test_plan_erika},
        .expectation = {.description = "the policy, goals and 3 foe rows"},
    },
    {
        .input = {.name = "plan: 10.03 rival_tower names the policy, goals and foes", .body = test_plan_tower_rival},
        .expectation = {.description = "the policy, goals and 5 foe rows"},
    },
};

} // namespace


void add_tests_AutoStoryBattlePolicy(UnitTestDatabase& database){
    for (const AutoStoryBattlePolicyCase& test_case : AUTO_STORY_BATTLE_POLICY_CASES){
        database.add<Test_AutoStoryBattlePolicy>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
