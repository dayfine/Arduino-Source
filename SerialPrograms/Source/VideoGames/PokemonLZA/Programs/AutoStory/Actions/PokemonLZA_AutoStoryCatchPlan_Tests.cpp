/*  PokemonLZA AutoStory - Catch plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each kind of catch-plan choice is a table of cases, each a name, the
 *  plates or patrol bounds it is given, and what it should choose.
 *
 */

#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchPlan_Tests.h"
#include "PokemonLZA_AutoStorySpeciesCheck.h"
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  `caught` unset: the mark wasn't read.
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


struct NewSpeciesCase{
    std::string name;
    struct Input{
        ThrowSeen seen;
    };
    Input input;
    struct Expectation{
        std::optional<NewSpeciesSign> sign;
    };
    Expectation expectation;
};

std::string sign_text(std::optional<NewSpeciesSign> sign){
    if (!sign){
        return "no new species";
    }
    return *sign == NewSpeciesSign::BANNER ? "banner" : "plate was not caught";
}

const NewSpeciesCase NEW_SPECIES_CASES[] = {
    {
        .name = "the banner counts",
        .input = {.seen = {.registered = true, .plate_caught = false, .target_gone = true}},
        .expectation = {.sign = NewSpeciesSign::BANNER},
    },
    {
        .name = "no banner, a plate not caught and the target gone counts",
        .input = {.seen = {.registered = false, .plate_caught = false, .target_gone = true}},
        .expectation = {.sign = NewSpeciesSign::PLATE_NOT_CAUGHT},
    },
    {
        .name = "no banner, a caught plate and the target gone does not count",
        .input = {.seen = {.registered = false, .plate_caught = true, .target_gone = true}},
        .expectation = {.sign = std::nullopt},
    },
    {
        .name = "no banner, an unread plate and the target gone does not count",
        .input = {.seen = {.registered = false, .plate_caught = std::nullopt, .target_gone = true}},
        .expectation = {.sign = std::nullopt},
    },
    {
        .name = "no banner, a plate not caught and the target still there does not count",
        .input = {.seen = {.registered = false, .plate_caught = false, .target_gone = false}},
        .expectation = {.sign = std::nullopt},
    },
};

class Test_NewSpeciesSign : public UnitTest{
public:
    explicit Test_NewSpeciesSign(const NewSpeciesCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: new species sign: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ThrowSeen& seen = m_case.input.seen;
        if (seen.new_species() != m_case.expectation.sign){
            return std::format("Expected {}, got {}.", sign_text(m_case.expectation.sign), seen.new_species_text());
        }
        return true;
    }

private:
    const NewSpeciesCase& m_case;
};


struct SpeciesCheckCase{
    std::string name;
    struct Input{
        GrassCatches patrol = {};
        std::optional<int> dex_count = std::nullopt;
    };
    Input input;
    struct Expectation{
        SpeciesTally tally;
        bool enough;
    };
    Expectation expectation;
};

//  2 new species wanted, 5 in the Pokedex.
const SpeciesCheckCase SPECIES_CHECK_CASES[] = {
    {
        .name = "two banners, no count read: sure",
        .input = {.patrol = {.found = 2, .throws = 3}},
        .expectation = {.tally = SpeciesTally::BANNERS, .enough = true},
    },
    {
        .name = "two banners, a count of five: done",
        .input = {.patrol = {.found = 2, .throws = 3}, .dex_count = 5},
        .expectation = {.tally = SpeciesTally::BANNERS, .enough = true},
    },
    {
        .name = "two banners, a count short of five fails",
        .input = {.patrol = {.found = 2, .throws = 3}, .dex_count = 4},
        .expectation = {.tally = SpeciesTally::BANNERS, .enough = false},
    },
    {
        .name = "one new species by its plate only, a count of five: done",
        .input = {.patrol = {.found = 2, .found_by_plate = 1, .throws = 2}, .dex_count = 5},
        .expectation = {.tally = SpeciesTally::INFERRED, .enough = true},
    },
    {
        .name = "inferred, a count short of five fails",
        .input = {.patrol = {.found = 2, .found_by_plate = 1, .throws = 2}, .dex_count = 4},
        .expectation = {.tally = SpeciesTally::INFERRED, .enough = false},
    },
    {
        .name = "a rerun seeing only caught species, a count of five: done",
        .input = {.patrol = {.caught_seen = 3}, .dex_count = 5},
        .expectation = {.tally = SpeciesTally::INFERRED, .enough = true},
    },
    {
        .name = "a rerun seeing only caught species, no count read: taken as done",
        .input = {.patrol = {.caught_seen = 3}},
        .expectation = {.tally = SpeciesTally::INFERRED, .enough = true},
    },
    {
        .name = "one new species, no count read: short, fails",
        .input = {.patrol = {.found = 1, .throws = 12}},
        .expectation = {.tally = SpeciesTally::SHORT, .enough = false},
    },
    {
        .name = "short, but a count of five: banners missed, done",
        .input = {.patrol = {.found = 1, .throws = 12}, .dex_count = 5},
        .expectation = {.tally = SpeciesTally::SHORT, .enough = true},
    },
};

class Test_SpeciesCheck : public UnitTest{
public:
    explicit Test_SpeciesCheck(const SpeciesCheckCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: species check: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const SpeciesCheckCase::Input& in = m_case.input;
        const SpeciesCheckCase::Expectation& want = m_case.expectation;
        const SpeciesTally tally = species_tally(in.patrol, 2);
        if (tally != want.tally){
            return std::format("Expected the tally {}, got {}.", species_tally_text(want.tally), species_tally_text(tally));
        }
        const bool enough = enough_species(tally, in.dex_count, 5);
        if (enough != want.enough){
            return std::format("Expected enough: {}, got {}.", want.enough, enough);
        }
        return true;
    }

private:
    const SpeciesCheckCase& m_case;
};


struct PartyCountCase{
    std::string name;
    struct Input{
        std::optional<int> filled_slots;
        int dex_species;
    };
    Input input;
    struct Expectation{
        bool done;
    };
    Expectation expectation;
};

const PartyCountCase PARTY_COUNT_CASES[] = {
    {
        .name = "five filled slots of five species end the patrol",
        .input = {.filled_slots = 5, .dex_species = 5},
        .expectation = {.done = true},
    },
    {
        .name = "more filled slots than species end it too",
        .input = {.filled_slots = 6, .dex_species = 5},
        .expectation = {.done = true},
    },
    {
        .name = "four filled slots of five go on",
        .input = {.filled_slots = 4, .dex_species = 5},
        .expectation = {.done = false},
    },
    {
        .name = "a party bar not read goes on",
        .input = {.filled_slots = std::nullopt, .dex_species = 5},
        .expectation = {.done = false},
    },
    {
        .name = "no slot read (no bar on screen) goes on",
        .input = {.filled_slots = 0, .dex_species = 5},
        .expectation = {.done = false},
    },
};

class Test_PartyCount : public UnitTest{
public:
    explicit Test_PartyCount(const PartyCountCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: party count: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool done = party_has_species(m_case.input.filled_slots, m_case.input.dex_species);
        if (done != m_case.expectation.done){
            return std::format("Expected {}, got {}.", m_case.expectation.done ? "done" : "go on", done ? "done" : "go on");
        }
        return true;
    }

private:
    const PartyCountCase& m_case;
};


//  2 new species and 5 in the party wanted.
struct CatchSummaryCase{
    std::string name;
    struct Input{
        GrassCatches patrol = {};
        std::optional<int> filled_slots = std::nullopt;
    };
    Input input;
    struct Expectation{
        std::string text;
    };
    Expectation expectation;
};

const CatchSummaryCase CATCH_SUMMARY_CASES[] = {
    {
        //  console B 2026-11-18 00:39: the second catch's banner missed,
        //  the bar went 4 to 6.
        .name = "a full party bar with a banner missed is done, not short",
        .input = {.patrol = {.found = 1, .throws = 4}, .filled_slots = 6},
        .expectation = {.text = "the party bar: 6 filled slots of 5: done; the banners: 1 new species (0 by a plate only) in 4 throws, 0 locks on caught species only"},
    },
    {
        .name = "a party bar short of five is short whatever the banners",
        .input = {.patrol = {.found = 2, .throws = 3}, .filled_slots = 4},
        .expectation = {.text = "the party bar: 4 filled slots of 5: short; the banners: 2 new species (0 by a plate only) in 3 throws, 0 locks on caught species only"},
    },
    {
        .name = "an unread party bar leaves the banners' tally to decide",
        .input = {.patrol = {.found = 1, .throws = 4, .caught_seen = 2}},
        .expectation = {.text = "the party bar: not read; the banners: 1 new species (0 by a plate only) in 4 throws, 2 locks on caught species only: short"},
    },
    {
        .name = "an unread party bar with every banner seen",
        .input = {.patrol = {.found = 2, .throws = 2}},
        .expectation = {.text = "the party bar: not read; the banners: 2 new species (0 by a plate only) in 2 throws, 0 locks on caught species only: all by the banner"},
    },
};

class Test_CatchSummary : public UnitTest{
public:
    explicit Test_CatchSummary(const CatchSummaryCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: summary: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string text = catch_summary_text(m_case.input.patrol, m_case.input.filled_slots, /*new_species=*/2, /*party_species=*/5);
        if (text != m_case.expectation.text){
            return std::format("Expected \"{}\", got \"{}\".", m_case.expectation.text, text);
        }
        return true;
    }

private:
    const CatchSummaryCase& m_case;
};


//  Plates: first-throw catches at 0.49 and 0.50 of the height, the Mareep
//  missed 11 times at 0.40.
struct StepCase{
    std::string name;
    struct Input{
        Plate plate;
        int misses;
        //  Since the last read or throw.
        int steps;
    };
    Input input;
    struct Expectation{
        bool step;
    };
    Expectation expectation;
};

const StepCase STEP_CASES[] = {
    {
        .name = "the Mareep missed at 60 is stepped toward before the first throw",
        .input = {.plate = {0.53, 0.40}, .misses = 0, .steps = 0},
        .expectation = {.step = true},
    },
    {
        .name = "a plate as low as the first-throw catch at 98 is thrown at",
        .input = {.plate = {0.53, 0.49}, .misses = 0, .steps = 0},
        .expectation = {.step = false},
    },
    {
        .name = "a far plate after 4 steps before the first throw is thrown at",
        .input = {.plate = {0.53, 0.40}, .misses = 0, .steps = 4},
        .expectation = {.step = false},
    },
    {
        .name = "a far plate after a miss is stepped toward",
        .input = {.plate = {0.53, 0.40}, .misses = 1, .steps = 1},
        .expectation = {.step = true},
    },
    {
        .name = "a far plate after a miss and 2 steps is thrown at",
        .input = {.plate = {0.53, 0.40}, .misses = 1, .steps = 2},
        .expectation = {.step = false},
    },
    {
        .name = "a near plate after a miss is thrown at",
        .input = {.plate = {0.53, 0.50}, .misses = 2, .steps = 0},
        .expectation = {.step = false},
    },
};

class Test_StepTowardTarget : public UnitTest{
public:
    explicit Test_StepTowardTarget(const StepCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: step toward the target: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StepCase::Input& in = m_case.input;
        const bool step = step_toward_target(lock_target(in.plate), in.misses, in.steps);
        if (step != m_case.expectation.step){
            return std::format("Expected step: {}, got {}.", m_case.expectation.step, step);
        }
        return true;
    }

private:
    const StepCase& m_case;
};


struct CycleCase{
    std::string name;
    struct Input{
        size_t max_views;
        //  The target locked on after each flick, the first before any.
        std::vector<Plate> views;
        size_t flick_target;
    };
    Input input;
    struct Expectation{
        //  Up to and with the one where the cycle stops.
        size_t views_added;
        size_t targets;
        //  Left is negative.
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
        .name = "Wild Zone 1's spots, 100 first, then 60",
        .input = {.spots = {std::begin(CATCH_SPOTS), std::end(CATCH_SPOTS)}, .visits = 4},
        .expectation = {.stops = {100, 60, 100, 60}},
    },
    {
        .name = "an odd count ends on the first spot",
        .input = {.spots = {100, 60}, .visits = 3},
        .expectation = {.stops = {100, 60, 100}},
    },
    {
        .name = "no visits, no stops",
        .input = {.spots = {100, 60}, .visits = 0},
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
    for (const NewSpeciesCase& item : NEW_SPECIES_CASES){
        database.add<Test_NewSpeciesSign>(item);
    }
    for (const SpeciesCheckCase& item : SPECIES_CHECK_CASES){
        database.add<Test_SpeciesCheck>(item);
    }
    for (const PartyCountCase& item : PARTY_COUNT_CASES){
        database.add<Test_PartyCount>(item);
    }
    for (const CatchSummaryCase& item : CATCH_SUMMARY_CASES){
        database.add<Test_CatchSummary>(item);
    }
    for (const StepCase& item : STEP_CASES){
        database.add<Test_StepTowardTarget>(item);
    }
    for (const CycleCase& item : CYCLE_CASES){
        database.add<Test_LockTargetCycle>(item);
    }
    for (const StopsCase& item : STOPS_CASES){
        database.add<Test_AlternatingStops>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
