/*  AutoStory League Parity Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each decision of StoryData/frlg-league-parity.json (PokemonSimulation's
 *  pokesim.cli.frlg_league_parity: N League chains of its planned policy)
 *  holds what the program can read or track and, apart, what only the
 *  sim knows ("hidden": the foe's exact HP, its exact sleep counter, its
 *  moves' PP). The program's policy decides each turn twice: from the
 *  state the program can see, and from that state with the hidden part
 *  put back. With the hidden part back it must choose as the sim did: the
 *  same row, item and move slot, and the same top-ups between fights. Each
 *  that differs is printed with its whole decision line. A visible-state
 *  difference is put down to the hidden part that fixes it.
 *
 */

#include <array>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryLeagueParity_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view PARITY_FORMAT = "frlg-league-parity/1";

//  What the sim knows and the program can't, put back one at a time.
enum class Hidden{
    FOE_HP,
    FOE_SLEEP,
    FOE_PP,
};
constexpr std::array<Hidden, 3> ALL_HIDDEN{Hidden::FOE_HP, Hidden::FOE_SLEEP, Hidden::FOE_PP};

std::string_view hidden_name(Hidden hidden){
    switch (hidden){
    case Hidden::FOE_HP:
        return "foe HP (bar)";
    case Hidden::FOE_SLEEP:
        return "foe sleep length";
    case Hidden::FOE_PP:
        return "foe PP";
    }
    return "";
}

PlannedStatus planned_status_of(std::string_view name){
    static const std::map<std::string_view, PlannedStatus> STATUSES{
        {"none", PlannedStatus::NONE}, {"poison", PlannedStatus::POISON}, {"paralysis", PlannedStatus::PARALYSIS},
        {"sleep", PlannedStatus::SLEEP}, {"burn", PlannedStatus::BURN}, {"freeze", PlannedStatus::FREEZE},
    };
    const auto it = STATUSES.find(name);
    return it == STATUSES.end() ? PlannedStatus::NONE : it->second;
}

//  The status as the program reads it off our HP box.
StatusCondition condition_of(std::string_view name){
    switch (planned_status_of(name)){
    case PlannedStatus::NONE:
        return StatusCondition::NONE;
    case PlannedStatus::POISON:
        return StatusCondition::POISON;
    case PlannedStatus::PARALYSIS:
        return StatusCondition::PARALYSIS;
    case PlannedStatus::SLEEP:
        return StatusCondition::SLEEP;
    case PlannedStatus::BURN:
    case PlannedStatus::FREEZE:
        return StatusCondition::OTHER;
    }
    return StatusCondition::NONE;
}

int integer(const JsonObject& object, const std::string& key){
    return static_cast<int>(object.get_integer_default(key, 0));
}

SlugCounts counts_of(const JsonObject& object){
    SlugCounts counts;
    for (const auto& [name, count] : object){
        counts[name] = static_cast<int>(count.to_integer_throw());
    }
    return counts;
}

StatStages stages_of(const JsonObject& object){
    const JsonObject& stages = object.get_object_throw("stages");
    return StatStages{
        .attack = integer(stages, "atk"), .defense = integer(stages, "def"), .sp_attack = integer(stages, "spa"),
        .sp_defense = integer(stages, "spd"), .speed = integer(stages, "spe"), .accuracy = integer(stages, "accuracy"),
        .evasion = integer(stages, "evasion"),
    };
}

FieldWeather weather_of(std::string_view name){
    if (name == "sun"){
        return FieldWeather::SUN;
    }
    if (name == "rain"){
        return FieldWeather::RAIN;
    }
    return FieldWeather::NONE;
}

BattleTrackState track_of(const JsonObject& line){
    const JsonObject& own = line.get_object_throw("own");
    const JsonObject& foe = line.get_object_throw("foe");
    const JsonObject& field = line.get_object_throw("field");
    BattleTrackState track{
        .own = stages_of(own),
        .mist_turns = integer(own, "mist_turns"),
        .foe = stages_of(foe),
        .foe_status = planned_status_of(foe.get_string_throw("status")),
        .foe_sleep_turns = integer(foe, "sleep_min_left"),
        .foe_recharging = foe.get_boolean_default("recharging", false),
        .weather = weather_of(field.get_string_throw("weather")),
        .weather_turns = integer(field, "weather_turns"),
        .trainer_full_restores = integer(line.get_object_throw("trainer_state"), "full_restores_used"),
    };
    if (const std::string* charging = foe.get_string("charging")){
        track.foe_charging = *charging;
    }
    return track;
}

const LeagueTrainerData& trainer_of(const std::string& trainer){
    const auto it = league_foes_data().trainers.find(trainer);
    if (it == league_foes_data().trainers.end()){
        throw ParseException(std::format("frlg-league-parity.json: no League trainer {}.", trainer));
    }
    return it->second;
}

PlannedFoeTrack foe_track_of(const JsonObject& line){
    const LeagueTrainerData& trainer = trainer_of(line.get_string_throw("trainer"));
    const JsonObject& foe = line.get_object_throw("foe");
    const size_t slot = static_cast<size_t>(foe.get_integer_throw("slot"));
    if (slot >= trainer.party.size()){
        throw ParseException(std::format("frlg-league-parity.json: no party slot {}.", slot));
    }
    return PlannedFoeTrack{
        .foe = trainer.party[slot],
        .slot = slot,
        .fill = foe.get_double_throw("fill"),
        .party_alive = integer(line.get_object_throw("trainer_state"), "foes_left"),
        .full_restores_carried = count_of(trainer.items, "full-restore"),
    };
}

PartyMember lead_of(const JsonObject& own){
    const JsonObject& stats = own.get_object_throw("stats");
    PartyMember lead{
        .species = own.get_string_throw("species"),
        .level = static_cast<uint8_t>(own.get_integer_throw("level")),
        .stats = Stats{
            .hp = static_cast<uint16_t>(integer(stats, "hp")), .attack = static_cast<uint16_t>(integer(stats, "atk")),
            .defense = static_cast<uint16_t>(integer(stats, "def")), .sp_attack = static_cast<uint16_t>(integer(stats, "spa")),
            .sp_defense = static_cast<uint16_t>(integer(stats, "spd")), .speed = static_cast<uint16_t>(integer(stats, "spe")),
        },
    };
    for (const JsonValue& value : own.get_array_throw("moves")){
        const JsonObject& move = value.to_object_throw();
        lead.moves.push_back(KnownMove{.move = move.get_string_throw("id"), .pp = static_cast<uint8_t>(integer(move, "pp"))});
    }
    return lead;
}

//  The state the program builds from what it can see.
PlannedState visible_state(const JsonObject& line){
    const JsonObject& own = line.get_object_throw("own");
    const PlannedInputs inputs{
        .lead = lead_of(own),
        .hp = integer(own, "hp"),
        .status = condition_of(own.get_string_throw("status")),
        .track = track_of(line),
        .foe = foe_track_of(line),
        .bag = counts_of(line.get_object_throw("bag")),
    };
    PlannedState state = planned_state(inputs, league_foes_data());
    state.attacks_on_foe = integer(line, "attacks");
    return state;
}

void put_back(const JsonObject& hidden, Hidden part, PlannedState& state){
    switch (part){
    case Hidden::FOE_HP:
        state.foe.hp = integer(hidden, "foe_hp");
        return;
    case Hidden::FOE_SLEEP:
        state.foe.sleeping = integer(hidden, "foe_sleep_turns") > 1;
        return;
    case Hidden::FOE_PP:{
        const SlugCounts pp = counts_of(hidden.get_object_throw("foe_pp"));
        for (PlannedMove& move : state.foe.moves){
            move.pp = count_of(pp, move.id);
        }
        return;
    }
    }
}


//  The sim's choice of one turn.
struct SimChoice{
    std::string row;
    std::optional<std::string> item;
    std::optional<size_t> move_slot;
};

SimChoice sim_choice(const JsonObject& line){
    const JsonObject& choice = line.get_object_throw("choice");
    SimChoice sim{.row = choice.get_string_throw("row")};
    if (const std::string* item = choice.get_string("item")){
        sim.item = *item;
    }
    if (const JsonValue* slot = choice.get_value("move_slot")){
        sim.move_slot = static_cast<size_t>(slot->to_integer_throw());
    }
    return sim;
}

//  plan:struggle's move is the caller's best attack, not the policy's.
bool same_choice(const PlannedChoice& program, const SimChoice& sim){
    if (program.row != sim.row || program.item != sim.item){
        return false;
    }
    return sim.row == "plan:struggle" || program.move_slot == sim.move_slot;
}

std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += text.empty() ? item : std::format(", {}", item);
    }
    return text;
}

std::string choice_text(const PlannedChoice& choice){
    return std::format("{} {}", choice.row,
        choice.item ? *choice.item : choice.move_slot ? std::format("move {}", *choice.move_slot) : std::string("-"));
}


//  The tallies by the sim's row.
struct RowTally{
    int decisions = 0;
    //  The program chose otherwise from what it can see.
    int visible = 0;
    //  ... and with the hidden part put back.
    int oracle = 0;
};

class Parity{
public:
    void turn(const JsonObject& line){
        const SimChoice sim = sim_choice(line);
        const LeagueFightData* fight = league_fight_data(league_plan_data(), line.get_string_throw("fight"));
        if (fight == nullptr){
            throw ParseException("frlg-league-parity.json: a fight the League plan doesn't have.");
        }
        const int streak = integer(line, "streak");
        const PlannedState visible = visible_state(line);
        RowTally& tally = m_rows[sim.row];
        tally.decisions++;
        if (same_choice(choose_planned(visible, *fight, streak), sim)){
            return;
        }
        tally.visible++;
        const JsonObject& hidden = line.get_object_throw("hidden");
        PlannedState oracle = visible;
        for (Hidden part : ALL_HIDDEN){
            put_back(hidden, part, oracle);
        }
        const PlannedChoice program = choose_planned(oracle, *fight, streak);
        if (!same_choice(program, sim)){
            tally.oracle++;
            note_oracle(line, program, sim);
            return;
        }
        m_causes[cause(visible, hidden, *fight, streak, sim)]++;
    }
    void between(const JsonObject& line);
    std::string table() const;
    int oracle_mismatches() const;

private:
    std::string cause(const PlannedState& visible, const JsonObject& hidden, const LeagueFightData& fight, int streak, const SimChoice& sim) const{
        for (Hidden part : ALL_HIDDEN){
            PlannedState state = visible;
            put_back(hidden, part, state);
            if (same_choice(choose_planned(state, fight, streak), sim)){
                return std::string(hidden_name(part));
            }
        }
        return "several hidden parts";
    }
    void note_oracle(const JsonObject& line, const PlannedChoice& program, const SimChoice& sim){
        const JsonObject& foe = line.get_object_throw("foe");
        const bool screens = integer(foe, "reflect_turns") > 0 || integer(foe, "light_screen_turns") > 0;
        if (screens){
            m_screen_lines++;
        }
        m_examples.push_back(std::format("chain {} {} turn {} vs {}{}: sim {} {}, program {}\n    {}",
            integer(line, "chain"), line.get_string_throw("fight"), integer(line, "turn"), foe.get_string_throw("species"),
            screens ? " (Reflect/Light Screen up)" : "", sim.row, sim.item ? *sim.item : sim.move_slot ? std::format("move {}", *sim.move_slot) : "-",
            choice_text(program), line.dump(0)));
    }

    std::map<std::string, RowTally> m_rows;
    std::map<std::string, int> m_causes;
    int m_screen_lines = 0;
    std::vector<std::string> m_examples;
};

void Parity::between(const JsonObject& line){
    const LeagueFightData* next = league_fight_data(league_plan_data(), line.get_string_throw("fight"));
    if (next == nullptr){
        throw ParseException("frlg-league-parity.json: a fight the League plan doesn't have.");
    }
    const JsonObject& carry = line.get_object_throw("carry");
    PlannedCarry program{
        .hp = integer(carry, "hp"),
        .max_hp = integer(carry, "max_hp"),
        .status = planned_status_of(carry.get_string_throw("status")),
        .pp = counts_of(carry.get_object_throw("pp")),
        .full_pp = counts_of(carry.get_object_throw("full_pp")),
        .bag = counts_of(carry.get_object_throw("bag")),
    };
    std::vector<std::string> sim;
    for (const JsonValue& item : line.get_array_throw("used")){
        sim.push_back(item.to_string_throw());
    }
    RowTally& tally = m_rows["between"];
    tally.decisions++;
    const std::vector<std::string> used = planned_between(program, *next);
    if (used != sim){
        tally.visible++;
        tally.oracle++;
        m_examples.push_back(std::format("chain {} before {}: sim [{}], program [{}]\n    {}", integer(line, "chain"),
            line.get_string_throw("fight"), joined(sim), joined(used), line.dump(0)));
    }
}

std::string Parity::table() const{
    std::string text = "row: decisions, differ from what the program sees, differ with the hidden part back\n";
    for (const auto& [row, tally] : m_rows){
        text += std::format("  {}: {}, {}, {}\n", row, tally.decisions, tally.visible, tally.oracle);
    }
    text += "visible-only differences, by the hidden part that fixes them:\n";
    for (const auto& [cause, count] : m_causes){
        text += std::format("  {}: {}\n", cause, count);
    }
    text += std::format("with the hidden part back, {} differ with the foe's Reflect or Light Screen up\n", m_screen_lines);
    for (const std::string& example : m_examples){
        text += std::format("  {}\n", example);
    }
    return text;
}

int Parity::oracle_mismatches() const{
    int total = 0;
    for (const auto& [row, tally] : m_rows){
        total += tally.oracle;
    }
    return total;
}

const JsonObject& parity_root(const JsonValue& value){
    const JsonObject& root = value.to_object_throw();
    if (root.get_string_default("format") != PARITY_FORMAT){
        throw ParseException(std::format("frlg-league-parity.json: not a {} file.", PARITY_FORMAT));
    }
    return root;
}

std::string parity_text(std::string_view file){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == file){
            return text;
        }
    }
    throw ParseException(std::format("StoryData/{}.json is not built into the program.", file));
}


struct ParityInput{
    //  A StoryData file of the sim's decisions.
    std::string file = {};
};
struct ParityExpectation{
    //  Decisions the program makes otherwise with the hidden part put back.
    int mismatches = 0;
};
struct ParityCase{
    std::string name;
    ParityInput input;
    ParityExpectation expectation;
};

class Test_LeagueParity : public UnitTest{
public:
    explicit Test_LeagueParity(ParityCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryLeagueParity - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const JsonValue value = parse_json(parity_text(m_case.input.file));
        Parity parity;
        for (const JsonValue& decision : parity_root(value).get_array_throw("decisions")){
            const JsonObject& line = decision.to_object_throw();
            if (line.get_string_throw("kind") == "between"){
                parity.between(line);
            }else{
                parity.turn(line);
            }
        }
        const std::string table = parity.table();
        logger.log(table);
        if (parity.oracle_mismatches() != m_case.expectation.mismatches){
            return std::format("expected {} decisions to differ with the hidden part put back, read {}:\n{}",
                m_case.expectation.mismatches, parity.oracle_mismatches(), table);
        }
        return true;
    }

private:
    ParityCase m_case;
};

const ParityCase PARITY_CASES[] = {
    {
        .name = "the sim's planned League chains",
        .input = {.file = "frlg-league-parity"},
        .expectation = {.mismatches = 0},
    },
};

} // namespace


void add_tests_AutoStoryLeagueParity(UnitTestDatabase& database){
    for (const ParityCase& test_case : PARITY_CASES){
        database.add<Test_LeagueParity>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
