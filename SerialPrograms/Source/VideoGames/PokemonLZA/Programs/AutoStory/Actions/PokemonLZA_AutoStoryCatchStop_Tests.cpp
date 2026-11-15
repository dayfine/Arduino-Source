/*  PokemonLZA AutoStory - Catch stop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each kind of catch-stop choice is a table of cases, each a name, what
 *  the stop saw, and what it should do. The banner watch runs on scripted
 *  frames: the capture times the pivot hands over and when the banner shows.
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetector.h"
#include "PokemonLZA_AutoStoryCatchStop.h"
#include "PokemonLZA_AutoStoryCatchStop_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  A plate that locks on at (`x`, `y`), its caught mark `caught`.
LockTarget plate(double x, double y, std::optional<bool> caught){
    LockTarget target;
    target.x = x;
    target.y = y;
    target.width = 0.05;
    target.caught = caught;
    return target;
}


//  spot_visit(): a catch spot whose species was caught there is only
//  turned back at.
struct SpotVisitCase{
    std::string name;
    struct Input{
        int spot;
        std::vector<int> caught_at;
    };
    Input input;
    struct Expectation{
        SpotVisit visit;
    };
    Expectation expectation;
};

std::string_view spot_visit_text(SpotVisit visit){
    return visit == SpotVisit::TURN_BACK ? "turn back" : "catch";
}

const SpotVisitCase SPOT_VISIT_CASES[] = {
    {
        .name = "nothing caught yet: catch at 100",
        .input = {.spot = 100, .caught_at = {}},
        .expectation = {.visit = SpotVisit::CATCH},
    },
    {
        .name = "Scatterbug caught at 100: back to 60 at once",
        .input = {.spot = 100, .caught_at = {100}},
        .expectation = {.visit = SpotVisit::TURN_BACK},
    },
    {
        .name = "Scatterbug caught at 100: catch at 60",
        .input = {.spot = 60, .caught_at = {100}},
        .expectation = {.visit = SpotVisit::CATCH},
    },
};

class Test_SpotVisit : public UnitTest{
public:
    explicit Test_SpotVisit(const SpotVisitCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: spot visit: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const SpotVisit visit = spot_visit(m_case.input.spot, m_case.input.caught_at);
        if (visit != m_case.expectation.visit){
            return std::format("Expected {}, got {}.", spot_visit_text(m_case.expectation.visit), spot_visit_text(visit));
        }
        return true;
    }

private:
    const SpotVisitCase& m_case;
};


//  after_throw(): what a catch stop does after a throw at a locked target.
struct AfterThrowCase{
    std::string name;
    struct Input{
        ThrowSeen seen;
        bool battle = false;
        //  The plate that locks on with ZL held after the throw; nothing: none.
        std::optional<LockTarget> relocked = std::nullopt;
    };
    Input input;
    struct Expectation{
        AfterThrow next;
    };
    Expectation expectation;
};

std::string_view after_throw_text(AfterThrow next){
    switch (next){
    case AfterThrow::STOP_ENDS:
        return "the stop ends";
    case AfterThrow::WEAKEN:
        return "weaken";
    case AfterThrow::THROW_AGAIN:
        return "throw again";
    case AfterThrow::LOOK_AGAIN:
        return "look again";
    }
    return "?";
}

const AfterThrowCase AFTER_THROW_CASES[] = {
    {
        .name = "the banner ends the stop",
        .input = {.seen = {.registered = true, .plate_caught = false, .target_gone = false}},
        .expectation = {.next = AfterThrow::STOP_ENDS},
    },
    {
        .name = "a target not caught gone with no battle ends the stop",
        .input = {.seen = {.registered = false, .plate_caught = false, .target_gone = true}},
        .expectation = {.next = AfterThrow::STOP_ENDS},
    },
    {
        .name = "a break-out into a wild battle weakens it",
        .input = {.seen = {.registered = false, .plate_caught = false, .target_gone = false}, .battle = true},
        .expectation = {.next = AfterThrow::WEAKEN},
    },
    {
        .name = "a miss with a plate not caught locking on again throws again at once",
        .input = {
            .seen = {.registered = false, .plate_caught = false, .target_gone = false},
            .relocked = plate(0.51, 0.40, false),
        },
        .expectation = {.next = AfterThrow::THROW_AGAIN},
    },
    {
        .name = "a miss with an unread plate locking on again throws again at once",
        .input = {
            .seen = {.registered = false, .plate_caught = std::nullopt, .target_gone = false},
            .relocked = plate(0.51, 0.40, std::nullopt),
        },
        .expectation = {.next = AfterThrow::THROW_AGAIN},
    },
    {
        .name = "a plate of a caught species locking on after the throw looks again",
        .input = {
            .seen = {.registered = false, .plate_caught = false, .target_gone = false},
            .relocked = plate(0.53, 0.42, true),
        },
        .expectation = {.next = AfterThrow::LOOK_AGAIN},
    },
    {
        .name = "a caught species' target gone looks again",
        .input = {.seen = {.registered = false, .plate_caught = true, .target_gone = true}},
        .expectation = {.next = AfterThrow::LOOK_AGAIN},
    },
};

class Test_AfterThrow : public UnitTest{
public:
    explicit Test_AfterThrow(const AfterThrowCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: after a throw: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const AfterThrowCase::Input& in = m_case.input;
        const AfterThrow next = after_throw(in.seen, in.battle, in.relocked);
        if (next != m_case.expectation.next){
            return std::format("Expected {}, got {}.", after_throw_text(m_case.expectation.next), after_throw_text(next));
        }
        return true;
    }

private:
    const AfterThrowCase& m_case;
};


//  CameraPitch: when a catch stop resets the camera's pitch.
struct PitchCase{
    std::string name;
    enum class Event{
        ZL_HELD,    //  a lock-on, a throw or a fight
        LOOK,       //  before a sweep or the walk on: level_now()
    };
    struct Input{
        std::vector<Event> events;
    };
    Input input;
    struct Expectation{
        //  What each LOOK's level_now() says, in order.
        std::vector<bool> levels;
    };
    Expectation expectation;
};

const PitchCase PITCH_CASES[] = {
    {
        .name = "a stop's start after a walk needs no reset",
        .input = {.events = {PitchCase::Event::LOOK}},
        .expectation = {.levels = {false}},
    },
    {
        .name = "one reset after a lock-on and a throw, then none",
        .input = {.events = {PitchCase::Event::LOOK, PitchCase::Event::ZL_HELD, PitchCase::Event::ZL_HELD, PitchCase::Event::LOOK, PitchCase::Event::LOOK}},
        .expectation = {.levels = {false, true, false}},
    },
    {
        .name = "a stop with no lock-on ends with no reset",
        .input = {.events = {PitchCase::Event::LOOK, PitchCase::Event::LOOK}},
        .expectation = {.levels = {false, false}},
    },
};

std::string levels_text(const std::vector<bool>& levels){
    std::string out;
    for (const bool level : levels){
        out += out.empty() ? "" : ", ";
        out += level ? "level" : "keep";
    }
    return out;
}

class Test_CameraPitch : public UnitTest{
public:
    explicit Test_CameraPitch(const PitchCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: camera pitch: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        CameraPitch pitch;
        std::vector<bool> levels;
        for (const PitchCase::Event event : m_case.input.events){
            if (event == PitchCase::Event::ZL_HELD){
                pitch.zl_held();
            }else{
                levels.push_back(pitch.level_now());
            }
        }
        if (levels != m_case.expectation.levels){
            return std::format("Expected [{}], got [{}].", levels_text(m_case.expectation.levels), levels_text(levels));
        }
        return true;
    }

private:
    const PitchCase& m_case;
};

} // namespace


void add_tests_AutoStoryCatchStop(UnitTestDatabase& database){
    for (const SpotVisitCase& item : SPOT_VISIT_CASES){
        database.add<Test_SpotVisit>(item);
    }
    for (const AfterThrowCase& item : AFTER_THROW_CASES){
        database.add<Test_AfterThrow>(item);
    }
    for (const PitchCase& item : PITCH_CASES){
        database.add<Test_CameraPitch>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
