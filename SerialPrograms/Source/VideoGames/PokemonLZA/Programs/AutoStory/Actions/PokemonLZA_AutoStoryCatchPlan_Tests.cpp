/*  PokemonLZA AutoStory - Catch plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each kind of catch-plan choice is a table of cases, each a name, the
 *  plates or patrol bounds it is given, and what it should choose.
 *
 */

#include <chrono>
#include <format>
#include <iterator>
#include <optional>
#include <utility>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryBracketWatch.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchPlan_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  A lock-on target's plate: where it is on screen, and whether its
//  species is caught (nothing: the mark wasn't read).
struct Plate{
    double x;
    double y;
    std::optional<bool> caught = false;
};

LockTarget lock_target(const Plate& plate){
    LockTarget target;
    target.x = plate.x;
    target.y = plate.y;
    target.width = 0.05;
    target.caught = plate.caught;
    return target;
}

std::vector<LockTarget> lock_targets(const std::vector<Plate>& plates){
    std::vector<LockTarget> targets;
    for (const Plate& plate : plates){
        targets.push_back(lock_target(plate));
    }
    return targets;
}

std::string joined(const std::vector<int>& values){
    std::string out;
    for (const int value : values){
        out += out.empty() ? std::format("{}", value) : std::format(", {}", value);
    }
    return out;
}

std::string index_text(std::optional<size_t> index){
    return index ? std::format("target {}", *index) : "no target";
}


//  nearest_uncaught(): the lowest plate whose species isn't caught.
struct NearestCase{
    std::string name;
    struct Input{
        std::vector<Plate> plates;
    };
    Input input;
    struct Expectation{
        std::optional<size_t> chosen;
    };
    Expectation expectation;
};

const NearestCase NEAREST_CASES[] = {
    {
        .name = "the lowest plate not caught; an unread mark counts as not caught",
        .input = {.plates = {{0.50, 0.25}, {0.55, 0.45, true}, {0.45, 0.35, std::nullopt}, {0.60, 0.30}}},
        .expectation = {.chosen = 2},
    },
    {
        .name = "none when every species is caught",
        .input = {.plates = {{0.5, 0.4, true}, {0.5, 0.5, true}}},
        .expectation = {.chosen = std::nullopt},
    },
    {
        .name = "none from no plates",
        .input = {.plates = {}},
        .expectation = {.chosen = std::nullopt},
    },
};

class Test_NearestUncaught : public UnitTest{
public:
    explicit Test_NearestUncaught(const NearestCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: nearest target: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<size_t> chosen = nearest_uncaught(lock_targets(m_case.input.plates));
        if (chosen != m_case.expectation.chosen){
            return std::format("Expected {}, got {}.", index_text(m_case.expectation.chosen), index_text(chosen));
        }
        return true;
    }

private:
    const NearestCase& m_case;
};


//  lock_target_far(): a plate high on the screen is far.
struct FarCase{
    std::string name;
    struct Input{
        Plate plate;
    };
    Input input;
    struct Expectation{
        bool far;
    };
    Expectation expectation;
};

const FarCase FAR_CASES[] = {
    {
        .name = "a high plate is far",
        .input = {.plate = {0.5, 0.20}},
        .expectation = {.far = true},
    },
    {
        .name = "a lower plate is near",
        .input = {.plate = {0.5, 0.40}},
        .expectation = {.far = false},
    },
};

class Test_LockTargetFar : public UnitTest{
public:
    explicit Test_LockTargetFar(const FarCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: far target: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool far = lock_target_far(lock_target(m_case.input.plate));
        if (far != m_case.expectation.far){
            return std::format("Expected far: {}, got {}.", m_case.expectation.far, far);
        }
        return true;
    }

private:
    const FarCase& m_case;
};


//  LockTargetCycle: the targets a flick at a time, until a flick finds no
//  other, comes round to the first, or the cycle has seen max_views.
struct CycleCase{
    std::string name;
    struct Input{
        size_t max_views;
        //  The target locked on after each flick, the first before any.
        std::vector<Plate> views;
        //  The target, in the cycle's order, to count the flicks to.
        size_t flick_target;
    };
    Input input;
    struct Expectation{
        //  The views added, up to and with the one where the cycle stops.
        size_t views_added;
        size_t targets;
        //  From the lock where the cycle stopped: left is negative.
        int flicks;
    };
    Expectation expectation;
};

const CycleCase CYCLE_CASES[] = {
    {
        .name = "stops where a flick finds no other, the nearest back left",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.52, 0.40}, {0.48, 0.30}, {0.49, 0.31}}, .flick_target = 1},
        .expectation = {.views_added = 4, .targets = 3, .flicks = -1},
    },
    {
        .name = "stops when it comes round to the first, the second a flick right",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.50, 0.40}, {0.51, 0.26}}, .flick_target = 1},
        .expectation = {.views_added = 3, .targets = 2, .flicks = 1},
    },
    {
        .name = "is bounded by its views, the lock on the last",
        .input = {
            .max_views = 4,
            .views = {{0.0, 0.20}, {0.1, 0.25}, {0.2, 0.30}, {0.3, 0.35}, {0.4, 0.40}, {0.5, 0.45}, {0.6, 0.50}},
            .flick_target = 3,
        },
        .expectation = {.views_added = 4, .targets = 4, .flicks = 0},
    },
};

class Test_LockTargetCycle : public UnitTest{
public:
    explicit Test_LockTargetCycle(const CycleCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: target cycle: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LockTargetCycle cycle(m_case.input.max_views);
        size_t added = 0;
        for (const Plate& view : m_case.input.views){
            added++;
            if (!cycle.add(lock_target(view))){
                break;
            }
        }
        const size_t targets = cycle.targets().size();
        const int flicks = cycle.flicks_to(m_case.input.flick_target);
        const CycleCase::Expectation& expected = m_case.expectation;
        if (added != expected.views_added || targets != expected.targets || flicks != expected.flicks){
            return std::format(
                "Expected {} views added, {} targets and {} flicks; got {}, {} and {}.",
                expected.views_added, expected.targets, expected.flicks, added, targets, flicks
            );
        }
        return true;
    }

private:
    const CycleCase& m_case;
};


//  BracketWatch: frames one after another, each with or without brackets.
struct WatchCase{
    std::string name;
    struct Frame{
        int ms;
        //  The brackets' middle x and height; nothing: none on the frame.
        std::optional<std::pair<double, double>> brackets;
    };
    struct Input{
        int settle_ms;
        int limit_ms;
        std::vector<Frame> frames;
    };
    Input input;
    struct Expectation{
        //  The frames added before the watch was done (all when it never was).
        size_t frames_watched;
        //  The middle x of the brackets chosen; nothing: none chosen.
        std::optional<double> chosen_x;
    };
    Expectation expectation;
};

const WatchCase WATCH_CASES[] = {
    {
        .name = "the tallest of the brackets seen until it settles",
        .input = {
            .settle_ms = 1500, .limit_ms = 6000,
            .frames = {{0, std::nullopt}, {300, {{0.30, 0.07}}}, {600, {{0.70, 0.10}}}, {900, {{0.55, 0.08}}}, {1800, {{0.40, 0.12}}}, {2100, {{0.45, 0.15}}}},
        },
        .expectation = {.frames_watched = 5, .chosen_x = 0.40},
    },
    {
        .name = "nothing at the limit when no brackets show",
        .input = {.settle_ms = 1500, .limit_ms = 1000, .frames = {{0, std::nullopt}, {500, std::nullopt}, {1000, std::nullopt}, {1300, {{0.5, 0.1}}}}},
        .expectation = {.frames_watched = 3, .chosen_x = std::nullopt},
    },
    {
        .name = "brackets first seen late still settle",
        .input = {.settle_ms = 1000, .limit_ms = 6000, .frames = {{0, std::nullopt}, {2500, {{0.60, 0.09}}}, {3000, std::nullopt}, {3500, std::nullopt}}},
        .expectation = {.frames_watched = 4, .chosen_x = 0.60},
    },
};

class Test_BracketWatch : public UnitTest{
public:
    explicit Test_BracketWatch(const WatchCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: bracket watch: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const WatchCase::Input& in = m_case.input;
        BracketWatch watch{std::chrono::milliseconds(in.settle_ms), std::chrono::milliseconds(in.limit_ms)};
        size_t watched = 0;
        for (const WatchCase::Frame& frame : in.frames){
            std::optional<BracketSighting> sighting;
            if (frame.brackets){
                sighting = BracketSighting{frame.brackets->first, 0.4, frame.brackets->second};
            }
            watch.add(std::chrono::milliseconds(frame.ms), sighting);
            watched++;
            if (watch.done()){
                break;
            }
        }
        const std::optional<BracketSighting> chosen = watch.nearest();
        const std::optional<double> chosen_x = chosen ? std::optional<double>(chosen->x) : std::nullopt;
        const WatchCase::Expectation& expected = m_case.expectation;
        if (watched != expected.frames_watched || chosen_x != expected.chosen_x){
            return std::format(
                "Expected {} frames and x {}; got {} and {}.",
                expected.frames_watched, expected.chosen_x.value_or(-1), watched, chosen_x.value_or(-1)
            );
        }
        return true;
    }

private:
    const WatchCase& m_case;
};


//  camera_turn_toward(): the right stick toward brackets off the middle.
struct TurnCase{
    std::string name;
    struct Input{
        double x;
    };
    Input input;
    struct Expectation{
        double stick_x;
        int ms;
    };
    Expectation expectation;
};

const TurnCase TURN_CASES[] = {
    {.name = "none near the middle", .input = {.x = 0.54}, .expectation = {.stick_x = 0, .ms = 0}},
    {.name = "a short turn right", .input = {.x = 0.60}, .expectation = {.stick_x = 0.4, .ms = 240}},
    {.name = "a short turn left", .input = {.x = 0.40}, .expectation = {.stick_x = -0.4, .ms = 240}},
    {.name = "the longest turn a quarter off or more", .input = {.x = 0.95}, .expectation = {.stick_x = 0.4, .ms = 480}},
};

class Test_CameraTurn : public UnitTest{
public:
    explicit Test_CameraTurn(const TurnCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: camera turn: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const CameraTurn turn = camera_turn_toward(m_case.input.x);
        const TurnCase::Expectation& expected = m_case.expectation;
        if (turn.stick_x != expected.stick_x || turn.time.count() != expected.ms){
            return std::format(
                "Expected stick {} for {} ms, got {} for {} ms.",
                expected.stick_x, expected.ms, turn.stick_x, turn.time.count()
            );
        }
        return true;
    }

private:
    const TurnCase& m_case;
};


//  alternating_stops(): back and forth over the catch spots.
struct StopsCase{
    std::string name;
    struct Input{
        std::vector<int> spots;
        size_t visits;
    };
    Input input;
    struct Expectation{
        std::vector<int> stops;
    };
    Expectation expectation;
};

const StopsCase STOPS_CASES[] = {
    {
        .name = "Wild Zone 1's spots, three times each",
        .input = {.spots = {std::begin(CATCH_SPOTS), std::end(CATCH_SPOTS)}, .visits = 6},
        .expectation = {.stops = {96, 57, 96, 57, 96, 57}},
    },
    {
        .name = "an odd count ends on the first spot",
        .input = {.spots = {96, 57}, .visits = 3},
        .expectation = {.stops = {96, 57, 96}},
    },
    {
        .name = "no visits, no stops",
        .input = {.spots = {96, 57}, .visits = 0},
        .expectation = {.stops = {}},
    },
    {
        .name = "no spots, no stops",
        .input = {.spots = {}, .visits = 4},
        .expectation = {.stops = {}},
    },
};

class Test_AlternatingStops : public UnitTest{
public:
    explicit Test_AlternatingStops(const StopsCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: patrol stops: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<int> stops = alternating_stops(m_case.input.spots, m_case.input.visits);
        if (stops != m_case.expectation.stops){
            return std::format("Expected [{}], got [{}].", joined(m_case.expectation.stops), joined(stops));
        }
        return true;
    }

private:
    const StopsCase& m_case;
};

} // namespace


void add_tests_AutoStoryCatchPlan(UnitTestDatabase& database){
    for (const NearestCase& item : NEAREST_CASES){
        database.add<Test_NearestUncaught>(item);
    }
    for (const FarCase& item : FAR_CASES){
        database.add<Test_LockTargetFar>(item);
    }
    for (const CycleCase& item : CYCLE_CASES){
        database.add<Test_LockTargetCycle>(item);
    }
    for (const WatchCase& item : WATCH_CASES){
        database.add<Test_BracketWatch>(item);
    }
    for (const TurnCase& item : TURN_CASES){
        database.add<Test_CameraTurn>(item);
    }
    for (const StopsCase& item : STOPS_CASES){
        database.add<Test_AlternatingStops>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
