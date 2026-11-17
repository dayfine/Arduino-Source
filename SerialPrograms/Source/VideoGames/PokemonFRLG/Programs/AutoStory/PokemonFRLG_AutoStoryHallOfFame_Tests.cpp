/*  AutoStory Hall of Fame Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  HallOfFameShots on the looks pokefirered's Hall of Fame gives at one
 *  every 250 ms (hall_of_fame.c): each member slides in (under 0.4 s) and
 *  holds 2 s, the confetti moves for 6.7 s, the player's picture holds
 *  2 s and slides, and the LEAGUE CHAMP card waits for A.
 *
 */

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryHallOfFame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  `count` looks, each changed or still.
void add_looks(size_t count, bool changed, std::vector<bool>& looks){
    looks.insert(looks.end(), count, changed);
}

//  The looks from the save message's end to the card, with `members`
//  party members and a card shown `card_looks` looks.
std::vector<bool> hall_of_fame_looks(size_t members, size_t card_looks){
    std::vector<bool> looks;
    for (size_t c = 0; c < members; c++){
        add_looks(/*count=*/2, /*changed=*/true, looks);      //  the slide in
        add_looks(/*count=*/8, /*changed=*/false, looks);     //  the 2 s hold with its name
    }
    add_looks(/*count=*/27, /*changed=*/true, looks);         //  the welcome and the confetti
    add_looks(/*count=*/2, /*changed=*/true, looks);          //  the fade to the player
    add_looks(/*count=*/8, /*changed=*/false, looks);         //  the player's picture holds 2 s
    add_looks(/*count=*/5, /*changed=*/true, looks);          //  and slides right
    add_looks(/*count=*/card_looks, /*changed=*/false, looks);
    return looks;
}

struct HallOfFameShotsInput{
    std::vector<bool> looks;
};

struct HallOfFameShotsExpectation{
    size_t mons;
    //  The look whose frame the TEAM shot is (the first of the confetti), if any.
    std::optional<size_t> team_frame;
    bool champion;
};

struct HallOfFameShotsCase{
    std::string name;
    HallOfFameShotsInput input;
    HallOfFameShotsExpectation expectation;
};

class Test_AutoStoryHallOfFameShots : public UnitTest{
public:
    explicit Test_AutoStoryHallOfFameShots(HallOfFameShotsCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Hall of Fame shots: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        HallOfFameShots shots;
        size_t mons = 0;
        size_t teams = 0;
        size_t champions = 0;
        std::optional<size_t> run_start;
        std::optional<size_t> team_frame;
        for (size_t look = 0; look < m_case.input.looks.size(); look++){
            const HallOfFameShot shot = shots.see(m_case.input.looks[look]);
            if (shots.run_started()){
                run_start = look;
            }
            mons += shot == HallOfFameShot::MON;
            champions += shot == HallOfFameShot::CHAMPION;
            if (shot == HallOfFameShot::TEAM){
                teams++;
                team_frame = run_start;
            }
        }
        const HallOfFameShotsExpectation& expected = m_case.expectation;
        if (mons != expected.mons){
            return std::format("{} member shots, expected {}.", mons, expected.mons);
        }
        if (teams > 1 || team_frame != expected.team_frame){
            return std::format("{} team shots, of look {}.", teams, team_frame ? std::to_string(*team_frame) : "-");
        }
        if ((champions == 1) != expected.champion || champions > 1 || shots.done() != expected.champion){
            return std::format("{} card shots.", champions);
        }
        return true;
    }

private:
    HallOfFameShotsCase m_case;
};

const HallOfFameShotsCase HALL_OF_FAME_SHOTS_CASES[] = {
    {
        .name = "a party of six",
        .input = {.looks = hall_of_fame_looks(6, 20)},
        .expectation = {.mons = 6, .team_frame = 60, .champion = true},
    },
    {
        .name = "a party of two",
        .input = {.looks = hall_of_fame_looks(2, 20)},
        .expectation = {.mons = 2, .team_frame = 20, .champion = true},
    },
    {
        //  The player's picture holds 2 s before it slides: not the card.
        .name = "the card not reached yet",
        .input = {.looks = hall_of_fame_looks(6, 8)},
        .expectation = {.mons = 6, .team_frame = 60, .champion = false},
    },
    {
        .name = "a still screen with no team",
        .input = {.looks = std::vector<bool>(40, false)},
        .expectation = {.mons = 1, .team_frame = std::nullopt, .champion = false},
    },
};

} // namespace


void add_tests_AutoStoryHallOfFame(UnitTestDatabase& database){
    for (const HallOfFameShotsCase& test_case : HALL_OF_FAME_SHOTS_CASES){
        database.add<Test_AutoStoryHallOfFameShots>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
