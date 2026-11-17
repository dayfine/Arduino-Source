/*  PokemonLZA AutoStory - Marker arrival Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
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

using Look = MarkerLook;
using namespace std::chrono_literals;

constexpr std::optional<int> NO_DIGITS = std::nullopt;

//  A look with an A prompt or a dialog, the marker up with no digits.
constexpr Look PROMPT{.distance = NO_DIGITS, .marker_shows = true, .prompt_or_dialog = true};
//  A look with a dialog and no marker: a scene took it away.
constexpr Look SCENE_DIALOG{.distance = NO_DIGITS, .marker_shows = false, .prompt_or_dialog = true};
//  A look with no marker and no prompt.
constexpr Look MARKER_GONE{.distance = NO_DIGITS, .marker_shows = false, .prompt_or_dialog = false};

std::vector<Look> reads(std::vector<std::optional<int>> distances){
    std::vector<Look> looks;
    for (const std::optional<int>& distance : distances){
        looks.push_back(Look{.distance = distance, .marker_shows = true, .prompt_or_dialog = false});
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
        //  The time from one look to the next: 0.64 s on the walk that
        //  passed 03.09 in run 10 (console B, 2026-11-16 21:36:55 to
        //  21:36:58).
        Milliseconds look_period = 640ms;
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
        //  console B, 2026-11-16 21:33:36 to 21:33:50 (run 9): 40 taken at
        //  Wild Zone 1's exit, then "1", "0", "4" and "4" (the marker
        //  showed 39) agreed on 4 with the exit pillar's "Esci" prompt up,
        //  5.4 s after 40, and the walk passed there.
        .name = "03.09's walk: a read of 4 five seconds after 40 is a misread, so the exit's prompt is not the lab gate",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(
                reads({174, 1, NO_DIGITS, 731, 1, 51, 7, 5, 41, NO_DIGITS, 4, NO_DIGITS, NO_DIGITS, 40, NO_DIGITS, 1, NO_DIGITS, 4, NO_DIGITS, NO_DIGITS}),
                {Look{.distance = 4, .marker_shows = true, .prompt_or_dialog = true}}
            ),
            .look_period = 770ms,
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
    {
        //  console B, 2026-11-16 21:36:55 to 21:36:58 (run 10, passed): 23
        //  down to 12 at 0.64 s a look, 16 to 12 in one.
        .name = "a walk at full pace: 23 down to 8, then a prompt, is at the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({23, 21, 18, 16, 12, 9}), {Look{.distance = 8, .marker_shows = true, .prompt_or_dialog = true}}),
        },
        .expectation = {.arrived_on = 7, .near = true},
    },
    {
        .name = "40, then no digits for 7 s, then 9 and 8: the walk can have come that near",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(
                reads({46, 44, 42, 40, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, 9, 8}),
                {Look{.distance = 8, .marker_shows = true, .prompt_or_dialog = true}}
            ),
        },
        .expectation = {.arrived_on = 17, .near = true},
    },
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
        //  away, and Ryon's dialog showed after the camera's sweep.
        .name = "03.09's walk that passed: 37 down to 13, the marker gone, then the dialog",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(
                reads({37, 35, 32, 29, NO_DIGITS, 24, 24, 21, 18, 16, 13}),
                {MARKER_GONE, MARKER_GONE, MARKER_GONE, SCENE_DIALOG}
            ),
        },
        .expectation = {.arrived_on = 15, .near = true},
    },
    {
        //  console B, 2026-11-16 18:47:15 to 18:47:18: by a dialog on the
        //  street (not the lab gate's scene), OCR read "E5", "41", "v1" and
        //  "E6"; 5, 1 and 6 agreed and were taken, and the dialog passed
        //  the walk at "6 to go". The gate's scene came 23 to go later.
        .name = "03.09's walk: junk single digits by a street dialog are not near the lab gate",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(
                reads({NO_DIGITS, NO_DIGITS, 5, 41, NO_DIGITS, NO_DIGITS, 1}),
                {Look{.distance = 6, .marker_shows = true, .prompt_or_dialog = true}}
            ),
        },
        .expectation = {.arrived_on = std::nullopt, .near = false},
    },
    {
        //  A prompt with the marker still up at 12 to go: the lab gate's
        //  scene takes the marker away before its dialog (console B,
        //  2026-11-14 and 2026-11-16 18:47:40 to 18:47:51).
        .name = "a prompt with the marker up at 12 to go is not the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({24, 21, 18, 15, 12}), {Look{.distance = 12, .marker_shows = true, .prompt_or_dialog = true}}),
        },
        .expectation = {.arrived_on = std::nullopt, .near = true},
    },
    {
        .name = "a prompt with the marker up below 10 to go is the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({18, 15, 12, 9}), {Look{.distance = 8, .marker_shows = true, .prompt_or_dialog = true}}),
        },
        .expectation = {.arrived_on = 5, .near = true},
    },
    {
        //  console B, 2026-11-16 18:47:37 to 18:47:51: 23 down to 12, the
        //  marker gone, Ryon's dialog 10 s later: the lab gate's scene.
        .name = "the lab gate's scene after 23 down to 12: the marker gone, then the dialog",
        .input = {
            .proof = MarkerProof::NO_FAR_READ,
            .looks = concat(reads({23, 22, 18, 15, 12, NO_DIGITS}), {MARKER_GONE, MARKER_GONE, SCENE_DIALOG}),
        },
        .expectation = {.arrived_on = 9, .near = true},
    },
    {
        .name = "a prompt at 44 to go, the reads agreeing, is not the objective",
        .input = {
            .proof = MarkerProof::NEAR_READ,
            .looks = concat(reads({50, 47, 44}), {Look{.distance = 44, .prompt_or_dialog = true}}),
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
        Look look = test.input.looks[index];
        look.at = WallClock{} + test.input.look_period * index;
        arrival.look(look);
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
