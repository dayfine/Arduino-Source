/*  AutoStory Battle Track Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League fight's tracked state from battle texts as
 *  read_battle_text() gives them (letters, lower case), turn by turn.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattleTrack.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  One turn: the texts since the last choice, and the foe out after them.
struct TrackTurn{
    std::vector<std::string> texts = {};
    size_t foe_slot = 0;
};
struct TrackInput{
    std::vector<TrackTurn> turns = {};
};
//  The state at the last turn's choice.
struct TrackExpectation{
    StatStages own = {};
    StatStages foe = {};
    PlannedStatus foe_status = PlannedStatus::NONE;
    int foe_sleep_turns = 0;
    FieldWeather weather = FieldWeather::NONE;
    int trainer_full_restores = 0;
};
struct TrackCase{
    std::string name;
    TrackInput input;
    TrackExpectation expectation;
};

bool same(const StatStages& a, const StatStages& b){
    return a.attack == b.attack && a.defense == b.defense && a.sp_attack == b.sp_attack && a.sp_defense == b.sp_defense
        && a.speed == b.speed && a.accuracy == b.accuracy && a.evasion == b.evasion;
}

std::string describe(const StatStages& stages){
    return std::format("atk {} def {} spa {} spd {} spe {} acc {} eva {}",
        stages.attack, stages.defense, stages.sp_attack, stages.sp_defense, stages.speed, stages.accuracy, stages.evasion);
}

class Test_BattleTrack : public UnitTest{
public:
    explicit Test_BattleTrack(TrackCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryBattleTrack - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        BattleTrack track;
        for (size_t turn = 0; turn < m_case.input.turns.size(); turn++){
            if (turn > 0){
                track.turn_chosen();
            }
            track.read_turn(m_case.input.turns[turn].texts);
            track.foe_out(m_case.input.turns[turn].foe_slot, "");
        }
        const BattleTrackState& got = track.state();
        const TrackExpectation& want = m_case.expectation;
        if (!same(got.own, want.own) || !same(got.foe, want.foe) || got.foe_status != want.foe_status
            || got.foe_sleep_turns != want.foe_sleep_turns || got.weather != want.weather
            || got.trainer_full_restores != want.trainer_full_restores){
            return std::format("Ours {}; the foe's {}, status {}, sleep {}; weather {}; Full Restores {}.",
                describe(got.own), describe(got.foe), static_cast<int>(got.foe_status), got.foe_sleep_turns,
                static_cast<int>(got.weather), got.trainer_full_restores);
        }
        return true;
    }

private:
    TrackCase m_case;
};

const TrackCase TRACK_CASES[] = {
    {
        .name = "Growth raises the foe's Sp. Atk",
        .input = {.turns = {{.texts = {"foevenusaurusedgrowth", "foevenusaursspatkrose"}}}},
        .expectation = {.foe = {.sp_attack = 1}},
    },
    {
        .name = "Amnesia sharply raises the foe's Sp. Def",
        .input = {.turns = {{.texts = {"foeslowbrousedamnesia", "foeslowbrosspdefsharplyrose"}}}},
        .expectation = {.foe = {.sp_defense = 2}},
    },
    {
        .name = "Double Team read twice, the name misread once: one stage",
        .input = {.turns = {{.texts = {"foegengarsevasivenessrose", "foegengersevasivenessrose"}}}},
        .expectation = {.foe = {.evasion = 1}},
    },
    {
        .name = "Double Team on two turns: two stages",
        .input = {.turns = {{.texts = {"foegengarsevasivenessrose"}}, {.texts = {"foegengarsevasivenessrose"}}}},
        .expectation = {.foe = {.evasion = 2}},
    },
    {
        .name = "Screech harshly lowers our Defense",
        .input = {.turns = {{.texts = {"foearbokusedscreech", "blastoisesdefenseharshlyfell"}}}},
        .expectation = {.own = {.defense = -2}},
    },
    {
        .name = "Scary Face and Rock Tomb lower our Speed",
        .input = {.turns = {{.texts = {"blastoisesspeedharshlyfell"}}, {.texts = {"blastoisesspeedfell"}}}},
        .expectation = {.own = {.speed = -3}},
    },
    {
        .name = "our X Special's text: counted from the item, not the text",
        .input = {.turns = {{.texts = {"kosmosusedxspecial", "blastoisesspatkrose"}}}},
        .expectation = {},
    },
    {
        .name = "a new foe starts at stage 0",
        .input = {.turns = {{.texts = {"foevenusaursspatkrose"}, .foe_slot = 4}, {.texts = {}, .foe_slot = 5}}},
        .expectation = {},
    },
    {
        .name = "our lead falls asleep: not the foe's status",
        .input = {.turns = {{.texts = {"foejynxusedlovelykiss", "blastoisefellasleep"}}}},
        .expectation = {},
    },
    {
        .name = "Rest: 3 turns, one less per turn slept through",
        .input = {.turns = {{.texts = {"foelaprassleptandbecamehealthy"}}, {.texts = {"foelaprasisfastasleep", "foelaprasisfastasleep"}}}},
        .expectation = {.foe_status = PlannedStatus::SLEEP, .foe_sleep_turns = 2},
    },
    {
        .name = "another sleep: 2 turns at least",
        .input = {.turns = {{.texts = {"foejynxfellasleep"}}, {.texts = {"foejynxisfastasleep"}}}},
        .expectation = {.foe_status = PlannedStatus::SLEEP, .foe_sleep_turns = 1},
    },
    {
        .name = "the foe wakes up",
        .input = {.turns = {{.texts = {"foejynxfellasleep"}}, {.texts = {"foejynxwokeup"}}}},
        .expectation = {},
    },
    {
        .name = "Blizzard freezes the foe",
        .input = {.turns = {{.texts = {"foedragonitewasfrozensolid"}}}},
        .expectation = {.foe_status = PlannedStatus::FREEZE},
    },
    {
        .name = "the trainer's Full Restore cures the foe",
        .input = {.turns = {{.texts = {"foedragonitewasfrozensolid"}}, {.texts = {"lanceusedfullrestore", "lanceusedfullrestore"}}}},
        .expectation = {.trainer_full_restores = 1},
    },
    {
        .name = "a paralyzed foe",
        .input = {.turns = {{.texts = {"foedragonairisparalyzeditmaybeunabletomove"}}}},
        .expectation = {.foe_status = PlannedStatus::PARALYSIS},
    },
    {
        .name = "Sunny Day's sun on the 4th choice after",
        .input = {.turns = {{.texts = {"foevenusaurusedsunnyday", "thesunlightgotbright"}}, {}, {}, {}}},
        .expectation = {.weather = FieldWeather::SUN},
    },
    {
        .name = "Sunny Day's sun gone by the 5th choice after",
        .input = {.turns = {{.texts = {"thesunlightgotbright"}}, {}, {}, {}, {}}},
        .expectation = {},
    },
    {
        .name = "the sunlight faded",
        .input = {.turns = {{.texts = {"thesunlightgotbright"}}, {.texts = {"thesunlightfaded"}}}},
        .expectation = {},
    },
    {
        .name = "Solar Beam's charge is no sun",
        .input = {.turns = {{.texts = {"foevenusaurtookinsunlight"}}}},
        .expectation = {},
    },
    {
        .name = "the sun's reminder after a missed start",
        .input = {.turns = {{.texts = {"thesunlightisstrong"}}}},
        .expectation = {.weather = FieldWeather::SUN},
    },
    {
        .name = "Rain Dance's rain",
        .input = {.turns = {{.texts = {"itstartedtorain"}}}},
        .expectation = {.weather = FieldWeather::RAIN},
    },
};

} // namespace


void add_tests_AutoStoryBattleTrack(UnitTestDatabase& database){
    for (const TrackCase& test_case : TRACK_CASES){
        database.add<Test_BattleTrack>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
