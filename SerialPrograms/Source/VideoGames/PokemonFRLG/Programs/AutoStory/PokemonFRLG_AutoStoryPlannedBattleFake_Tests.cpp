/*  AutoStory Planned Battle Fake Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  League fights on a fake battle engine: each case scripts the foe's side
 *  turn by turn (its move, the damage on us, our Speed stages it changes,
 *  a status or confusion it gives us, Roar or Whirlwind) and what our
 *  moves do to it. The engine feeds PlannedBattle the per-turn inputs the
 *  real readers give it (our HP bar and status, the foe out and its HP bar,
 *  the battle texts, the bag), applies the choice, and plays the trainer's
 *  Full Restore below a quarter of the foe's HP. It ends the try as the
 *  plan says: a Roar or Whirlwind in a fight that resets on one, Bruno's
 *  early reset after Onix #1, a win or a loss.
 *
 */

#include <algorithm>
#include <cctype>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int MAX_TURNS = 30;
constexpr std::string_view LEAD = "blastoise";

//  Our Lv54 Blastoise (PokemonSimulation chain.py LV54) with the League's
//  moves and the PP it enters Lorelei with.
PartyMember blastoise(){
    return PartyMember{
        .species = std::string(LEAD),
        .moves = {{"bite", 25}, {"surf", 15}, {"blizzard", 5}, {"strength", 15}},
        .level = 54,
        .stats = Stats{.hp = 171, .attack = 104, .defense = 134, .sp_attack = 118, .sp_defense = 149, .speed = 117},
    };
}

//  The bag the League starts with (PokemonSimulation BAG_NOW).
SlugCounts league_bag(){
    return {
        {"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
        {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6}, {"full-heal", 2}, {"parlyz-heal", 2}, {"antidote", 1},
    };
}

//  The battle text's letters, as read_battle_text() gives them.
std::string letters(std::string_view text){
    std::string out;
    for (char c : text){
        if (std::isalpha(static_cast<unsigned char>(c))){
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}


//  One turn of the foe's.
struct FakeFoeTurn{
    std::string move = {};
    int damage = 0;
    //  Our Speed stages it changes (Rock Tomb: -1).
    int our_speed = 0;
    StatusCondition status = StatusCondition::NONE;
    bool confuses = false;
    //  Roar or Whirlwind: our lead is dragged out.
    bool drags_out = false;
};

//  A foe of the case, in the order they come out.
struct FakeFoe{
    size_t slot = 0;
    //  Its HP when it comes out; none for full.
    std::optional<int> hp = std::nullopt;
    //  Our moves' damage on it; a move not here does none.
    std::map<std::string, int, std::less<>> hits = {};
    //  Its turns; none once they run out.
    std::vector<FakeFoeTurn> turns = {};
};

enum class FakeEnd{
    WON,
    LOST,
    RESET,
    OUT_OF_TURNS,
};

std::string_view end_name(FakeEnd end){
    switch (end){
    case FakeEnd::WON:
        return "won";
    case FakeEnd::LOST:
        return "lost";
    case FakeEnd::RESET:
        return "reset";
    case FakeEnd::OUT_OF_TURNS:
        return "out of turns";
    }
    return "";
}

//  What a fake fight did.
struct FakeResult{
    //  Each turn's choice: the item, or the move.
    std::vector<std::string> actions = {};
    FakeEnd end = FakeEnd::OUT_OF_TURNS;
    bool passes = false;
};

//  The fight `trainer` from our side (`hp`, X items already used) against
//  the scripted `foes`.
class FakeBattle{
public:
    FakeBattle(std::string trainer, int hp, std::vector<FakeFoe> foes)
        : m_trainer_name(std::move(trainer))
        , m_hp(hp)
        , m_foes(std::move(foes))
    {
        const std::optional<std::string> name = league_fight_name(m_trainer_name);
        m_fight = name ? league_fight_data(league_plan_data(), *name) : nullptr;
        const auto trainer_data = league_foes_data().trainers.find(m_trainer_name);
        std::optional<PlannedBattle> battle = planned_battle(m_trainer_name);
        if (m_fight == nullptr || trainer_data == league_foes_data().trainers.end() || !battle){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No League fight {}.", m_trainer_name));
        }
        m_trainer = &trainer_data->second;
        m_battle.emplace(std::move(*battle));
        m_trainer_restores = count_of(m_trainer->items, "full-restore");
        m_supplies.lead = blastoise();
        for (const auto& [item, count] : league_bag()){
            m_supplies.other_counts[item] = count;
        }
    }

    //  X items used before the case's first turn.
    void used_before(const std::vector<std::string>& items){
        for (const std::string& item : items){
            m_battle->item_used(item);
            m_speed += item == "x-speed" ? 1 : 0;
        }
    }

    //  Up to `turns` turns.
    FakeResult run(int turns){
        FakeResult result;
        result.end = play(result.actions, turns);
        result.passes = league_try_passes(*m_fight, LeagueTry{
            .won = result.end == FakeEnd::WON,
            .roared = m_roared,
            .early_reset = m_early_reset,
            .used = m_used,
        });
        return result;
    }

private:
    const LeagueFoeData& foe_data() const{
        return m_trainer->party.at(m_foes[m_foe].slot);
    }

    void send_out(size_t index){
        m_foe = index;
        m_foe_turn = 0;
        m_foe_hp = m_foes[index].hp.value_or(foe_data().stats.hp);
    }

    FakeEnd play(std::vector<std::string>& actions, int turns){
        send_out(0);
        for (int turn = 0; turn < turns; turn++){
            const PolicyView view{
                .hp = static_cast<double>(m_hp),
                .max_hp = static_cast<double>(m_supplies.lead.stats->hp),
                .status = m_status,
                .speed = m_supplies.lead.stats->speed,
                .foe_index = m_foes[m_foe].slot,
                .foe_fill = static_cast<double>(m_foe_hp) / foe_data().stats.hp,
                .battle_texts = std::move(m_texts),
            };
            m_texts.clear();
            m_supplies.foe_out = foe_data().species;
            const PlannedChoice choice = m_battle->choose(m_supplies, view).choice;
            const bool foe_healed = trainer_heals();
            actions.push_back(our_turn(choice));
            if (m_foe_hp <= 0){
                if (std::optional<FakeEnd> end = foe_fainted()){
                    return *end;
                }
                continue;
            }
            if (!foe_healed){
                if (std::optional<FakeEnd> end = foe_turn()){
                    return *end;
                }
            }
        }
        return FakeEnd::OUT_OF_TURNS;
    }

    //  pret: the trainer's AI uses a Full Restore before any move when its
    //  Pokemon is under a quarter of its HP.
    bool trainer_heals(){
        const int max_hp = foe_data().stats.hp;
        if (m_trainer_restores <= 0 || m_foe_hp <= 0 || m_foe_hp >= max_hp / 4){
            return false;
        }
        m_trainer_restores--;
        m_foe_hp = max_hp;
        m_texts.push_back(letters(std::format("{} used FULL RESTORE!", m_fight->name)));
        return true;
    }

    std::string our_turn(const PlannedChoice& choice){
        if (choice.item){
            use_item(*choice.item);
            return *choice.item;
        }
        if (!choice.move_slot || *choice.move_slot >= m_supplies.lead.moves.size()){
            return "struggle";
        }
        KnownMove& move = m_supplies.lead.moves[*choice.move_slot];
        move.pp--;
        const auto hit = m_foes[m_foe].hits.find(move.move);
        m_foe_hp -= hit == m_foes[m_foe].hits.end() ? 0 : hit->second;
        return move.move;
    }

    void use_item(const std::string& item){
        m_supplies.other_counts[item]--;
        m_used[item]++;
        const int max_hp = m_supplies.lead.stats->hp;
        if (item == "full-restore"){
            m_hp = max_hp;
            m_status = StatusCondition::NONE;
        }else if (item == "super-potion"){
            m_hp = std::min(max_hp, m_hp + 50);
        }else if (item == "hyper-potion"){
            m_hp = std::min(max_hp, m_hp + 200);
        }else if (item == "full-heal" || item == "awakening" || item == "parlyz-heal" || item == "antidote" || item == "burn-heal"){
            m_status = StatusCondition::NONE;
        }else if (item == "x-speed"){
            m_speed = std::min(6, m_speed + 1);
        }
        m_battle->item_used(item);
    }

    std::optional<FakeEnd> foe_fainted(){
        if (league_early_reset(*m_fight, m_foes[m_foe].slot, m_speed, m_used)){
            m_early_reset = true;
            return FakeEnd::RESET;
        }
        if (m_foe + 1 >= m_foes.size()){
            return FakeEnd::WON;
        }
        send_out(m_foe + 1);
        return std::nullopt;
    }

    std::optional<FakeEnd> foe_turn(){
        std::vector<FakeFoeTurn>& turns = m_foes[m_foe].turns;
        if (m_foe_turn >= turns.size()){
            return std::nullopt;
        }
        const FakeFoeTurn& turn = turns[m_foe_turn++];
        m_texts.push_back(letters(std::format("Foe {} used {}!", foe_data().species, turn.move)));
        m_hp = std::max(0, m_hp - turn.damage);
        if (m_hp == 0){
            return FakeEnd::LOST;
        }
        if (turn.our_speed != 0){
            m_speed = std::clamp(m_speed + turn.our_speed, -6, 6);
            m_texts.push_back(letters(std::format("{}'s SPEED {}!", LEAD, turn.our_speed < -1 ? "harshly fell" : "fell")));
        }
        if (turn.status != StatusCondition::NONE && m_status == StatusCondition::NONE){
            m_status = turn.status;
        }
        if (turn.confuses){
            m_texts.push_back(letters(std::format("{} became confused!", LEAD)));
        }
        if (turn.drags_out){
            if (m_fight->reset_on_roar){
                m_roared = true;
                return FakeEnd::RESET;
            }
            m_speed = 0;
            m_battle->lead_switched_in();
        }
        return std::nullopt;
    }

    std::string m_trainer_name;
    const LeagueFightData* m_fight = nullptr;
    const LeagueTrainerData* m_trainer = nullptr;
    std::optional<PlannedBattle> m_battle;
    Supplies m_supplies;
    int m_hp;
    StatusCondition m_status = StatusCondition::NONE;
    int m_speed = 0;
    std::vector<FakeFoe> m_foes;
    size_t m_foe = 0;
    size_t m_foe_turn = 0;
    int m_foe_hp = 0;
    int m_trainer_restores = 0;
    std::vector<std::string> m_texts;
    SlugCounts m_used;
    bool m_roared = false;
    bool m_early_reset = false;
};


struct FakeInput{
    std::string trainer = {};
    int hp = 171;
    std::vector<std::string> used_before = {};
    std::vector<FakeFoe> foes = {};
    //  The turns played at most.
    int turns = MAX_TURNS;
};
struct FakeExpectation{
    std::vector<std::string> actions = {};
    FakeEnd end = FakeEnd::WON;
    bool passes = true;
};
struct FakeCase{
    std::string name;
    FakeInput input;
    FakeExpectation expectation;
};

std::string actions_text(const std::vector<std::string>& actions){
    std::string text;
    for (const std::string& action : actions){
        text += text.empty() ? action : std::format(", {}", action);
    }
    return text;
}

class Test_PlannedBattleFake : public UnitTest{
public:
    explicit Test_PlannedBattleFake(FakeCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - fake battle: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeBattle battle(m_case.input.trainer, m_case.input.hp, m_case.input.foes);
        battle.used_before(m_case.input.used_before);
        const FakeResult result = battle.run(m_case.input.turns);
        const FakeExpectation& want = m_case.expectation;
        if (result.actions != want.actions || result.end != want.end || result.passes != want.passes){
            return std::format("played [{}], {}, {}; expected [{}], {}, {}.",
                actions_text(result.actions), end_name(result.end), result.passes ? "passes" : "doesn't pass",
                actions_text(want.actions), end_name(want.end), want.passes ? "passes" : "doesn't pass");
        }
        return true;
    }

private:
    FakeCase m_case;
};

const FakeCase FAKE_CASES[] = {
    {
        .name = "Lorelei's Lapras: Strength, Bite, Bite",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_LORELEI",
            .foes = {{
                .slot = 4,
                .hits = {{"strength", 38}, {"bite", 91}},
                .turns = {{.move = "body slam", .damage = 25}, {.move = "body slam", .damage = 25}, {.move = "body slam", .damage = 25}},
            }},
        },
        .expectation = {.actions = {"strength", "bite", "bite"}},
    },
    {
        .name = "Lorelei's Jynx puts us to sleep: a Full Heal",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_LORELEI",
            .foes = {{
                .slot = 3,
                .hits = {{"bite", 75}},
                .turns = {{.move = "lovely kiss", .status = StatusCondition::SLEEP}, {.move = "ice punch", .damage = 20}},
            }},
            .turns = 2,
        },
        .expectation = {.actions = {"bite", "full-heal"}, .end = FakeEnd::OUT_OF_TURNS, .passes = false},
    },
    {
        .name = "Bruno's Onix: two Rock Tombs after all three X Speeds, reset when it falls",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_BRUNO",
            .foes = {{
                .slot = 0,
                .hits = {{"surf", 112}},
                .turns = {
                    {.move = "rock tomb", .damage = 13, .our_speed = -1},
                    {.move = "rock tomb", .damage = 13, .our_speed = -1},
                    {.move = "iron tail", .damage = 15},
                    {.move = "rock tomb", .damage = 13, .our_speed = -1},
                    {.move = "rock tomb", .damage = 13, .our_speed = -1},
                },
            }},
        },
        .expectation = {
            .actions = {"x-special", "x-special", "x-speed", "x-speed", "x-speed", "surf"},
            .end = FakeEnd::RESET,
            .passes = false,
        },
    },
    {
        .name = "Bruno's Onix Roars: reset",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_BRUNO",
            .foes = {{.slot = 0, .turns = {{.move = "roar", .drags_out = true}}}},
        },
        .expectation = {.actions = {"x-special"}, .end = FakeEnd::RESET, .passes = false},
    },
    {
        .name = "Bruno's Machamp falls to a +2 Surf",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_BRUNO",
            .used_before = {"x-special", "x-special"},
            .foes = {{.slot = 4, .hp = 110, .hits = {{"surf", 110}}}},
        },
        .expectation = {.actions = {"surf"}},
    },
    {
        .name = "Agatha's Gengar: +3 and +1 Speed through a Confuse Ray, then Bite",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_AGATHA",
            .foes = {{
                .slot = 0,
                .hits = {{"bite", 145}},
                .turns = {{.move = "confuse ray", .confuses = true}, {.move = "shadow punch", .damage = 29}, {.move = "double team"}},
            }},
        },
        .expectation = {.actions = {"x-special", "x-special", "x-special", "x-speed", "bite"}},
    },
    {
        .name = "Lance's Dragonite: the heal block's Super Potions, then the KO-risk Full Restore",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_LANCE",
            .hp = 61,
            .foes = {{
                .slot = 4,
                .hp = 120,
                .hits = {{"blizzard", 120}},
                .turns = {{.move = "wing attack", .damage = 53}, {.move = "wing attack", .damage = 58}, {.move = "wing attack", .damage = 40}},
            }},
        },
        .expectation = {.actions = {"super-potion", "super-potion", "full-restore", "blizzard"}},
    },
    {
        .name = "Lance's Dragonair paralyzes us: a Parlyz Heal",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_LANCE",
            //  The plan's +2 from Gyarados: at +0 Blizzard's low rolls leave
            //  Dragonair in Lance's Full Restore range, and kill_order opens
            //  with Strength (as the sim does).
            .used_before = {"x-special", "x-special"},
            .foes = {{
                .slot = 2,
                .hits = {{"blizzard", 80}},
                .turns = {{.move = "thunder wave", .status = StatusCondition::PARALYSIS}, {.move = "safeguard"}},
            }},
            .turns = 2,
        },
        .expectation = {.actions = {"blizzard", "parlyz-heal"}, .end = FakeEnd::OUT_OF_TURNS, .passes = false},
    },
    {
        .name = "the Champion's Pidgeot: Guard Spec, +2, +2 Accuracy, +1 Speed, reset on Whirlwind",
        .input = {
            .trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR",
            .foes = {{
                .slot = 0,
                .turns = {
                    {.move = "aerial ace", .damage = 5}, {.move = "aerial ace", .damage = 5}, {.move = "aerial ace", .damage = 5},
                    {.move = "aerial ace", .damage = 5}, {.move = "aerial ace", .damage = 5}, {.move = "whirlwind", .drags_out = true},
                },
            }},
        },
        .expectation = {
            .actions = {"guard-spec", "x-special", "x-special", "x-accuracy", "x-accuracy", "x-speed"},
            .end = FakeEnd::RESET,
            .passes = false,
        },
    },
    {
        .name = "Lorelei won on two Full Restores: over the cap of one, no pass",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_LORELEI",
            .hp = 20,
            .foes = {{
                .slot = 4,
                .hits = {{"strength", 220}},
                .turns = {{.move = "body slam", .damage = 160}, {.move = "confuse ray", .confuses = true}},
            }},
        },
        .expectation = {.actions = {"full-restore", "full-restore", "strength"}, .end = FakeEnd::WON, .passes = false},
    },
};

} // namespace


void add_tests_AutoStoryPlannedBattleFake(UnitTestDatabase& database){
    for (const FakeCase& test_case : FAKE_CASES){
        database.add<Test_PlannedBattleFake>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
