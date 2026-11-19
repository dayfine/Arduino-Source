/*  PokemonLZA AutoStory - Catch plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each kind of catch-plan choice is a table of cases, each a name, the
 *  plates or patrol bounds it is given, and what it should choose.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchPlan_Tests.h"
#include "PokemonLZA_AutoStorySpeciesCheck.h"

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
        .name = "no banner, an unread plate and the target gone does not count",
        .input = {.seen = {.registered = false, .plate_caught = std::nullopt, .target_gone = true}},
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
        const bool enough = enough_species(tally, in.dex_count, /*dex_needed=*/5);
        if (enough != want.enough){
            return std::format("Expected enough: {}, got {}.", want.enough, enough);
        }
        return true;
    }

private:
    const SpeciesCheckCase& m_case;
};


struct SettleCase{
    std::string name;
    struct Input{
        //  One per look; the last repeats.
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        std::optional<int> slots;
    };
    Expectation expectation;
};

const SettleCase SETTLE_CASES[] = {
    {
        //  03.08 read 4 of 5 and failed; its retry at once read 5 (console B, 2026-11-18).
        .name = "the last catch joining the bar a second after the first read counts",
        .input = {.reads = {4, 4, 4, 4, 5}},
        .expectation = {.slots = 5},
    },
};

class Test_SettledPartySlots : public UnitTest{
public:
    explicit Test_SettledPartySlots(const SettleCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: settled party bar: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::optional<int>>& reads = m_case.input.reads;
        size_t look = 0;
        const std::optional<int> got = settled_party_slots([&]{
            return reads[std::min(look++, reads.size() - 1)];
        });
        if (got != m_case.expectation.slots){
            return std::format("Expected {}, got {}.", m_case.expectation.slots.value_or(-1), got.value_or(-1));
        }
        return true;
    }

private:
    const SettleCase& m_case;
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
        .name = "an unread party bar leaves the banners' tally to decide",
        .input = {.patrol = {.found = 1, .throws = 4, .caught_seen = 2}},
        .expectation = {.text = "the party bar: not read; the banners: 1 new species (0 by a plate only) in 4 throws, 2 locks on caught species only: short"},
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
        .name = "a far plate after a miss is stepped toward",
        .input = {.plate = {0.53, 0.40}, .misses = 1, .steps = 1},
        .expectation = {.step = true},
    },
    {
        .name = "a far plate after a miss and 2 steps is thrown at",
        .input = {.plate = {0.53, 0.40}, .misses = 1, .steps = 2},
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
    for (const SettleCase& item : SETTLE_CASES){
        database.add<Test_SettledPartySlots>(item);
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
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
