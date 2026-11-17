/*  PokemonLZA AutoStory - Marker arrival Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryMarkerArrival.h"
#include "PokemonLZA_AutoStoryMarkerArrival_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  One look on the walk: the marker's digits as OCR read them (none: no
//  marker, or no digits), and whether an A prompt or a dialog shows.
struct Look{
    std::optional<int> read;
    bool prompt_or_dialog = false;
};

constexpr std::optional<int> NO_DIGITS = std::nullopt;

//  A look with an A prompt or a dialog, and no digits.
constexpr Look PROMPT{.read = NO_DIGITS, .prompt_or_dialog = true};

std::vector<Look> reads(std::vector<std::optional<int>> distances){
    std::vector<Look> looks;
    for (const std::optional<int>& distance : distances){
        looks.push_back(Look{.read = distance, .prompt_or_dialog = false});
    }
    return looks;
}

std::vector<Look> concat(std::vector<Look> first, const std::vector<Look>& second){
    first.insert(first.end(), second.begin(), second.end());
    return first;
}


struct ArrivalCase{
    std::string name;
    struct Input{
        MarkerProof proof = MarkerProof::NEAR_READ;
        std::vector<Look> looks = {};
    };
    Input input;
    struct Expectation{
        //  The look the walk arrives on (1-based); none: it never does.
        std::optional<size_t> arrived_on = std::nullopt;
        //  Whether the proof holds after the last look, so a prompt or a
        //  dialog after the walk ends it (MarkerArrival::near()).
        bool near = false;
    };
    Expectation expectation;
};

const ArrivalCase ARRIVAL_CASES[] = {
    {
        //  console B, 2026-11-16 17:34:33 to 17:34:37: 03.09's walk from
        //  the far gate read "t", "", "1" and nothing more, and passed on
        //  a prompt on the street 4 s in; 03.10 then failed from there.
        .name = "03.09's walk with no distance taken: a prompt on the street is not the lab gate",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({NO_DIGITS, NO_DIGITS, 1, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS}), {PROMPT, PROMPT}),
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
    {
        //  console B, 2026-11-14 21:27:50 to 21:28:12 (plza/autostory,
        //  passed): 37 down to 13, then the gate's scene took the marker
        //  away, and Taunie's dialog showed after the camera's sweep.
        .name = "03.09's walk that passed: 37 down to 13, the marker gone, then the dialog",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(
                reads({37, 35, 32, 29, NO_DIGITS, 24, 24, 21, 18, 16, 13, NO_DIGITS, NO_DIGITS, NO_DIGITS}),
                {PROMPT}
            ),
        },
        .expectation = {.arrived_on = 15, .near = true},
    },
    {
        .name = "a prompt at 44 to go, the reads agreeing, is not the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({50, 47, 44}), {Look{.read = 44, .prompt_or_dialog = true}}),
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
    {
        //  A walk that got near, then read far again (agreeing): the proof
        //  is gone with it.
        .name = "near, then far again: a prompt there is not the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({24, 21, 18, 26, 29, 31}), {PROMPT}),
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
    {
        //  console B, 2026-11-14 21:28:33 to 21:28:36 (plza/autostory,
        //  passed): up the lab's steps the digits never read, and the
        //  door's "A Enter" showed on the fifth look.
        .name = "up the lab's steps: no digits on four looks, then the door's prompt",
        .input = {
            .proof = MarkerProof::NO_FAR_READ,
            .looks = concat(reads({NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS}), {PROMPT}),
        },
        .expectation = {.arrived_on = 5, .near = true},
    },
    {
        //  console B, 2026-11-16 17:34:39 to 17:34:44: from the street (the
        //  walk before had passed early), 44 and 111 read, then a prompt.
        .name = "up the lab's steps from the street: 44 and 111 read, so a prompt is not the door",
        .input = {
            .proof = MarkerProof::NO_FAR_READ,
            .looks = concat(reads({NO_DIGITS, NO_DIGITS, 44, NO_DIGITS, 111, NO_DIGITS, NO_DIGITS, NO_DIGITS}), {PROMPT}),
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
};

UnitTestResult check_arrival(const ArrivalCase& test){
    MarkerArrival arrival(test.input.proof);
    std::optional<size_t> arrived_on;
    for (size_t index = 0; index < test.input.looks.size() && !arrived_on; index++){
        const Look& look = test.input.looks[index];
        arrival.look(look.read, look.prompt_or_dialog);
        if (arrival.arrived()){
            arrived_on = index + 1;
        }
    }
    if (arrived_on != test.expectation.arrived_on){
        return std::format(
            "expected arrival on look {}, got {} (last read taken {})",
            test.expectation.arrived_on ? std::to_string(*test.expectation.arrived_on) : "none",
            arrived_on ? std::to_string(*arrived_on) : "none",
            arrival.position().value_or(-1)
        );
    }
    if (arrival.near() != test.expectation.near){
        return std::format("expected near() {}, got {}", test.expectation.near, arrival.near());
    }
    return true;
}


class ArrivalTest : public UnitTest{
public:
    explicit ArrivalTest(ArrivalCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryMarkerArrival - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_arrival(m_case);
    }
private:
    ArrivalCase m_case;
};

} // namespace


void add_tests_AutoStoryMarkerArrival(UnitTestDatabase& database){
    for (const ArrivalCase& test : ARRIVAL_CASES){
        database.add<ArrivalTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
