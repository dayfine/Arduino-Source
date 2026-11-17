/*  AutoStory Planned Battle Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League foes data as built into the program, and the planned
 *  policy's state from it: damage rolls checked against @smogon/calc 0.12
 *  (PokemonSimulation's engine) with the same raw stats.
 *
 */

#include <cstdio>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Our Lv54 Blastoise (PokemonSimulation chain.py LV54), knowing the League plan's moves.
PartyMember blastoise(){
    return PartyMember{
        .species = "blastoise",
        .moves = {{"surf", 15}, {"bite", 25}, {"blizzard", 5}, {"strength", 15}},
        .level = 54,
        .stats = Stats{.hp = 171, .attack = 104, .defense = 134, .sp_attack = 118, .sp_defense = 149, .speed = 117},
    };
}

const LeagueFoeData& foe_of(std::string_view trainer, size_t slot){
    const auto it = league_foes_data().trainers.find(trainer);
    if (it == league_foes_data().trainers.end() || slot >= it->second.party.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No {} slot {} in the League foes.", trainer, slot));
    }
    return it->second.party[slot];
}

std::vector<int> damage_of(const std::vector<PlannedMove>& moves, std::string_view move){
    for (const PlannedMove& planned : moves){
        if (planned.id == move){
            return planned.damage;
        }
    }
    return {};
}


struct FoesInput{
    std::string trainer = {};
    size_t slot = 0;
};
struct FoesExpectation{
    std::string species = {};
    int level = 0;
    int hp = 0;
    int full_restores = 0;
};
struct FoesCase{
    std::string name;
    FoesInput input;
    FoesExpectation expectation;
};

class Test_LeagueFoes : public UnitTest{
public:
    explicit Test_LeagueFoes(FoesCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - League foes: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LeagueFoeData& foe = foe_of(m_case.input.trainer, m_case.input.slot);
        const auto& items = league_foes_data().trainers.find(m_case.input.trainer)->second.items;
        const auto restores = items.find("full-restore");
        const int carried = restores == items.end() ? 0 : restores->second;
        const FoesExpectation& want = m_case.expectation;
        if (foe.species != want.species || foe.level != want.level || foe.stats.hp != want.hp || carried != want.full_restores){
            return std::format("{} Lv{} {} HP, {} Full Restores.", foe.species, foe.level, foe.stats.hp, carried);
        }
        return true;
    }

private:
    FoesCase m_case;
};

const FoesCase FOES_CASES[] = {
    {
        .name = "Lorelei's lead",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 0},
        .expectation = {.species = "dewgong", .level = 52, .hp = 171, .full_restores = 2},
    },
    {
        .name = "Lance's lead",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0},
        .expectation = {.species = "gyarados", .level = 56, .hp = 189, .full_restores = 2},
    },
    {
        .name = "the Champion's last",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5},
        .expectation = {.species = "venusaur", .level = 63, .hp = 193, .full_restores = 4},
    },
};


struct DamageInput{
    std::string trainer = {};
    size_t slot = 0;
    //  The fight as tracked: stages, the foe's status, the weather.
    BattleTrackState track = {};
    //  Ours or the foe's.
    bool ours = true;
    //  Our HP (of 171).
    int hp = 171;
    std::string move = {};
};
struct DamageExpectation{
    //  Lowest first; empty for no damage.
    std::vector<int> rolls = {};
};
struct DamageCase{
    std::string name;
    DamageInput input;
    DamageExpectation expectation;
};

class Test_PlannedDamage : public UnitTest{
public:
    explicit Test_PlannedDamage(DamageCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - damage: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlannedInputs inputs{
            .lead = blastoise(),
            .hp = m_case.input.hp,
            .track = m_case.input.track,
            .foe = {.foe = foe_of(m_case.input.trainer, m_case.input.slot), .slot = m_case.input.slot},
        };
        const PlannedState state = planned_state(inputs, league_foes_data());
        const std::vector<int> got = damage_of(m_case.input.ours ? state.own.moves : state.foe.moves, m_case.input.move);
        if (got != m_case.expectation.rolls){
            return std::format("Rolls {}..{} ({}).", got.empty() ? 0 : got.front(), got.empty() ? 0 : got.back(), got.size());
        }
        return true;
    }

private:
    DamageCase m_case;
};

const DamageCase DAMAGE_CASES[] = {
    {
        .name = "Lance's Gyarados's Hyper Beam on us",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0, .ours = false, .move = "hyper-beam"},
        .expectation = {.rolls = {68, 68, 69, 70, 71, 72, 72, 73, 74, 75, 76, 76, 77, 78, 79, 80}},
    },
    {
        .name = "our Surf at +2 on Lance's Dragonite",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .track = {.own = {.sp_attack = 2}}, .move = "surf"},
        .expectation = {.rolls = {45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 54}},
    },
    {
        .name = "our Surf at +2 on Lance's Dragonite under a third of our HP: Torrent",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .track = {.own = {.sp_attack = 2}}, .hp = 57, .move = "surf"},
        .expectation = {.rolls = {68, 69, 70, 71, 72, 72, 73, 74, 75, 76, 76, 77, 78, 79, 80, 81}},
    },
    {
        .name = "our Surf at +2 on Lance's Dragonite just over a third of our HP: no Torrent",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .track = {.own = {.sp_attack = 2}}, .hp = 58, .move = "surf"},
        .expectation = {.rolls = {45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 54}},
    },
    {
        .name = "our Blizzard on Lance's Dragonite",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .move = "blizzard"},
        .expectation = {.rolls = {159, 161, 163, 165, 167, 169, 171, 172, 174, 176, 178, 180, 182, 184, 186, 188}},
    },
    {
        .name = "our Strength at -1 (Intimidate) on Lance's Gyarados",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0, .track = {.own = {.attack = -1}}, .move = "strength"},
        .expectation = {.rolls = {22, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 27}},
    },
    {
        .name = "Dragonair's Dragon Rage on us",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 1, .ours = false, .move = "dragon-rage"},
        .expectation = {.rolls = {40}},
    },
    {
        .name = "our Surf on Lorelei's Lapras (Water Absorb)",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 4, .move = "surf"},
        .expectation = {.rolls = {}},
    },
    {
        .name = "our Surf on the Champion's Venusaur",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5, .move = "surf"},
        .expectation = {.rolls = {27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32}},
    },
    {
        .name = "our Surf on Venusaur in Sunny Day's sun: halved",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5, .track = {.weather = FieldWeather::SUN}, .move = "surf"},
        .expectation = {.rolls = {13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 16}},
    },
    {
        .name = "our Surf on Venusaur in rain: x1.5",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5, .track = {.weather = FieldWeather::RAIN}, .move = "surf"},
        .expectation = {.rolls = {39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47}},
    },
    {
        .name = "Arcanine's Flamethrower on us in the sun: x1.5",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 4, .track = {.weather = FieldWeather::SUN}, .ours = false, .move = "flamethrower"},
        .expectation = {.rolls = {46, 47, 47, 48, 48, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54, 55}},
    },
    {
        .name = "Venusaur's Solar Beam on us after Growth",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5, .track = {.foe = {.sp_attack = 1}}, .ours = false, .move = "solar-beam"},
        .expectation = {.rolls = {255, 258, 261, 264, 267, 270, 273, 276, 279, 282, 285, 288, 291, 294, 297, 300}},
    },
    {
        .name = "our Blizzard at +2 on Lorelei's Dewgong (Thick Fat)",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 0, .track = {.own = {.sp_attack = 2}}, .move = "blizzard"},
        .expectation = {.rolls = {12, 12, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15}},
    },
    {
        .name = "our Blizzard at +3 on Dewgong (Thick Fat)",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 0, .track = {.own = {.sp_attack = 3}}, .move = "blizzard"},
        .expectation = {.rolls = {16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 19}},
    },
    {
        .name = "our Surf on Slowbro after Amnesia",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 2, .track = {.foe = {.sp_defense = 2}}, .move = "surf"},
        .expectation = {.rolls = {17, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 20, 20, 20, 20, 21}},
    },
    {
        .name = "Arbok's Iron Tail on us after Screech",
        .input = {.trainer = "TRAINER_ELITE_FOUR_AGATHA", .slot = 3, .track = {.own = {.defense = -2}}, .ours = false, .move = "iron-tail"},
        .expectation = {.rolls = {33, 33, 33, 34, 34, 35, 35, 35, 36, 36, 37, 37, 37, 38, 38, 39}},
    },
    {
        .name = "a burned Machamp's Cross Chop on us: Guts",
        .input = {.trainer = "TRAINER_ELITE_FOUR_BRUNO", .slot = 4, .track = {.foe_status = PlannedStatus::BURN}, .ours = false, .move = "cross-chop"},
        .expectation = {.rolls = {95, 96, 97, 98, 99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 112}},
    },
};


struct StateInput{
    std::string trainer = {};
    size_t slot = 0;
    BattleTrackState track = {};
};
struct StateExpectation{
    int our_speed = 0;
    int foe_speed = 0;
    bool foe_sleeping = false;
    double surf_accuracy = 1.0;
};
struct StateCase{
    std::string name;
    StateInput input;
    StateExpectation expectation;
};

class Test_PlannedTrackState : public UnitTest{
public:
    explicit Test_PlannedTrackState(StateCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - state: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlannedInputs inputs{
            .lead = blastoise(),
            .hp = 171,
            .track = m_case.input.track,
            .foe = {.foe = foe_of(m_case.input.trainer, m_case.input.slot), .slot = m_case.input.slot},
        };
        const PlannedState state = planned_state(inputs, league_foes_data());
        double surf_accuracy = 0;
        for (const PlannedMove& move : state.own.moves){
            surf_accuracy = move.id == "surf" ? move.accuracy : surf_accuracy;
        }
        const StateExpectation& want = m_case.expectation;
        if (state.own.speed != want.our_speed || state.foe.speed != want.foe_speed
            || state.foe.sleeping != want.foe_sleeping || surf_accuracy != want.surf_accuracy){
            return std::format("Speed {} vs {}, foe sleeping {}, Surf accuracy {}.", state.own.speed, state.foe.speed, state.foe.sleeping, surf_accuracy);
        }
        return true;
    }

private:
    StateCase m_case;
};

//  Our Speed 117 is 128 with Thunder's boost.
const StateCase STATE_CASES[] = {
    {
        .name = "Venusaur paralyzed: a quarter of its Speed",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5, .track = {.foe_status = PlannedStatus::PARALYSIS}},
        .expectation = {.our_speed = 128, .foe_speed = 31},
    },
    {
        .name = "after Scary Face: our Speed at -2",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 2, .track = {.own = {.speed = -2}}},
        .expectation = {.our_speed = 64, .foe_speed = 70},
    },
    {
        .name = "Gengar after Double Team: our Surf can miss",
        .input = {.trainer = "TRAINER_ELITE_FOUR_AGATHA", .slot = 0, .track = {.foe = {.evasion = 1}}},
        .expectation = {.our_speed = 128, .foe_speed = 126, .surf_accuracy = 0.75},
    },
    {
        .name = "Lapras rested: asleep for more than this turn",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 4, .track = {.foe_status = PlannedStatus::SLEEP, .foe_sleep_turns = 2}},
        .expectation = {.our_speed = 128, .foe_speed = 94, .foe_sleeping = true},
    },
    {
        .name = "Lapras on its last turn of sleep: may wake and hit",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 4, .track = {.foe_status = PlannedStatus::SLEEP, .foe_sleep_turns = 1}},
        .expectation = {.our_speed = 128, .foe_speed = 94, .foe_sleeping = false},
    },
};


//  One choice of a League fight: the foe read out, the foe index the
//  battle loop passed, our HP and the texts since the last choice.
struct SendTurn{
    std::string foe_out = {};
    size_t foe_index = 0;
    int hp = 171;
    std::vector<std::string> texts = {};
    //  X items used after this choice.
    std::vector<std::string> used = {};
};
struct SendsInput{
    std::string trainer = {};
    std::vector<SendTurn> turns = {};
};
//  A choice's log line: the foe, its party slot, the party alive and our
//  danger; or the plan's early reset.
struct SendSeen{
    std::string species = {};
    size_t slot = 0;
    int left = 0;
    bool danger = false;
    bool early_reset = false;

    bool operator==(const SendSeen&) const = default;
};
struct SendsExpectation{
    std::vector<SendSeen> seen = {};
};
struct SendsCase{
    std::string name;
    SendsInput input;
    SendsExpectation expectation;
};

//  The turn log's "<species> <hp>/<max> (slot <s>, <n> left), D <d>".
SendSeen seen_of(const PlannedBattle::Turn& turn){
    if (turn.early_reset){
        return SendSeen{.early_reset = true};
    }
    SendSeen seen;
    const std::string& log = turn.log;
    const size_t at = log.find(" (slot ");
    const size_t hp = log.rfind(' ', at == std::string::npos ? 0 : at - 1);
    const size_t name = log.rfind(", ", hp == std::string::npos ? 0 : hp);
    const size_t danger = log.find(", D ");
    if (at == std::string::npos || hp == std::string::npos || name == std::string::npos || danger == std::string::npos){
        return seen;
    }
    seen.species = log.substr(name + 2, hp - name - 2);
    int slot = 0;
    if (std::sscanf(log.c_str() + at, " (slot %d, %d left)", &slot, &seen.left) != 2){
        return SendSeen{};
    }
    seen.slot = static_cast<size_t>(slot);
    seen.danger = std::atoi(log.c_str() + danger + 4) > 0;
    return seen;
}

std::string describe(const std::vector<SendSeen>& seen){
    std::string text;
    for (const SendSeen& turn : seen){
        text += turn.early_reset ? std::string("[early reset] ")
            : std::format("[{} slot {}, {} left, danger {}] ", turn.species, turn.slot, turn.left, turn.danger);
    }
    return text;
}

class Test_PlannedSends : public UnitTest{
public:
    explicit Test_PlannedSends(SendsCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - sends: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::optional<PlannedBattle> battle = planned_battle(m_case.input.trainer);
        if (!battle){
            return std::format("No League fight {}.", m_case.input.trainer);
        }
        Supplies supplies;
        supplies.lead = blastoise();
        supplies.other_counts = {{"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2}};
        std::vector<SendSeen> got;
        for (const SendTurn& turn : m_case.input.turns){
            supplies.foe_out = turn.foe_out;
            const PolicyView view{
                .hp = static_cast<double>(turn.hp),
                .max_hp = 171,
                .speed = 117,
                .foe_index = turn.foe_index,
                .battle_texts = turn.texts,
            };
            got.push_back(seen_of(battle->choose(supplies, view)));
            for (const std::string& item : turn.used){
                battle->item_used(item);
            }
        }
        if (got != m_case.expectation.seen){
            return std::format("Got {}; expected {}.", describe(got), describe(m_case.expectation.seen));
        }
        return true;
    }

private:
    SendsCase m_case;
};

const SendsCase SENDS_CASES[] = {
    {
        //  The battle loop's foe index stuck at Venusaur's slot from the
        //  second foe on; Blastoise met Arcanine at 26 HP and fell to
        //  Extreme Speed with the policy seeing no danger.
        .name = "console B, 2026-11-17, Champion try 1: Venusaur second",
        .input = {
            .trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR",
            .turns = {
                {.foe_out = "pidgeot", .foe_index = 0},
                {.foe_out = "venusaur", .foe_index = 5},
                {.foe_out = "alakazam", .foe_index = 5},
                {.foe_out = "rhydon", .foe_index = 5},
                {.foe_out = "gyarados", .foe_index = 5},
                {.foe_out = "arcanine", .foe_index = 5, .hp = 26},
            },
        },
        .expectation = {.seen = {
            {.species = "pidgeot", .slot = 0, .left = 6, .danger = true},
            //  Solar Beam is its only attack: no hit without the sun.
            {.species = "venusaur", .slot = 5, .left = 5, .danger = false},
            {.species = "alakazam", .slot = 1, .left = 4, .danger = true},
            {.species = "rhydon", .slot = 2, .left = 3, .danger = true},
            {.species = "gyarados", .slot = 3, .left = 2, .danger = true},
            {.species = "arcanine", .slot = 4, .left = 1, .danger = true},
        }},
    },
    {
        .name = "Bruno's Onix falls after three X Speeds with our Speed below +0: the plan's early reset",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_BRUNO",
            .turns = {
                {.foe_out = "onix", .used = {"x-speed", "x-speed", "x-speed"}},
                {
                    .foe_out = "hitmonlee",
                    //  Four falls, each after another text: a text read twice in a row
                    //  is one change (BattleTrack::read_turn()).
                    .texts = {
                        "blastoisesspeedfell", "foeonixusedrocktomb", "blastoisesspeedfell", "foeonixusedrocktomb",
                        "blastoisesspeedfell", "foeonixusedrocktomb", "blastoisesspeedfell",
                    },
                },
            },
        },
        .expectation = {.seen = {{.species = "onix", .slot = 0, .left = 5, .danger = true}, {.early_reset = true}}},
    },
    {
        .name = "Bruno's Hitmonlee falls first: no early reset",
        .input = {
            .trainer = "TRAINER_ELITE_FOUR_BRUNO",
            .turns = {
                {
                    .foe_out = "hitmonlee",
                    .used = {"x-speed", "x-speed", "x-speed"},
                },
                {
                    .foe_out = "onix",
                    //  Four falls, each after another text: a text read twice in a row
                    //  is one change (BattleTrack::read_turn()).
                    .texts = {
                        "blastoisesspeedfell", "foeonixusedrocktomb", "blastoisesspeedfell", "foeonixusedrocktomb",
                        "blastoisesspeedfell", "foeonixusedrocktomb", "blastoisesspeedfell",
                    },
                },
            },
        },
        .expectation = {.seen = {
            {.species = "hitmonlee", .slot = 2, .left = 5, .danger = true},
            {.species = "onix", .slot = 0, .left = 4, .danger = true},
        }},
    },
};

} // namespace


void add_tests_AutoStoryPlannedBattle(UnitTestDatabase& database){
    for (const SendsCase& test_case : SENDS_CASES){
        database.add<Test_PlannedSends>(test_case);
    }
    for (const FoesCase& test_case : FOES_CASES){
        database.add<Test_LeagueFoes>(test_case);
    }
    for (const DamageCase& test_case : DAMAGE_CASES){
        database.add<Test_PlannedDamage>(test_case);
    }
    for (const StateCase& test_case : STATE_CASES){
        database.add<Test_PlannedTrackState>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
