/*  Story Fault Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The fault sweeps every game runs on its sim: each class of failure the
 *  consoles showed (Pokemon_StoryFailureClasses.h) injected at every row
 *  of a game's story, not only where it was seen. A game gives its sweep as
 *  data: its rows, an injector per sim fault its sim can inject, and the
 *  sweeps it fails today. This makes one test per class and row, named by
 *  the class's id, and judges it the same way for every game.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFaultSweep_H
#define PokemonAutomation_Pokemon_StoryFaultSweep_H

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimFaults.h"
#include "Pokemon_StoryFailureClasses.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One injected variant that failed its row.
struct FaultSweepFailure{
    //  The row's step the fault was injected at.
    std::string step;
    //  Which variant of the fault: "warp 1", "seed 16".
    std::string variant;
    //  How the row stopped or where it ended instead.
    std::string stopped;
};

//  What injecting one fault over one row found.
struct FaultSweepResult{
    size_t variants = 0;
    std::vector<FaultSweepFailure> failures = {};
    //  Why the row was not swept (it fails clean, the fault has no chance
    //  in it); nothing when it was.
    std::optional<std::string> not_swept = std::nullopt;
};

//  A game's injector of one sim fault: runs every variant of it over the
//  row named `row`.
using FaultInjector = std::function<FaultSweepResult(Logger& logger, const std::string& row)>;

//  A game's fault sweep, as data.
struct StoryFaultSweep{
    //  The start of every test's name: "PokemonFRLG::AutoStory".
    std::string test_prefix;
    //  The story's rows to sweep, by id, in story order.
    std::vector<std::string> rows;
    //  An injector per sim fault the game's sim can inject.
    std::map<FaultClass, FaultInjector> injectors;
    //  The sweeps the program fails today, by test name, with the likely
    //  fix: such a test passes while it fails, and fails once it passes.
    std::map<std::string, std::string> known_failing = {};
};

//  One test of a sweep: one sim fault over one row.
struct FaultSweepCell{
    StoryFailureClass failure_class;
    FaultClass fault;
    std::string row;
};

//  Every test of `sweep`: the classes in the shared list's order, each
//  over every row.
std::vector<FaultSweepCell> fault_sweep_cells(const StoryFaultSweep& sweep);
//  "<prefix> - Fault sweep: 11.01, warp_no_op".
std::string fault_sweep_test_name(std::string_view test_prefix, const FaultSweepCell& cell);

//  The classes the shared list injects by a sim fault that `sweep` has no
//  injector for, in the list's order: what the game cannot sweep yet.
std::vector<StoryFailureClass> classes_not_swept(const StoryFaultSweep& sweep);
//  The names on `sweep`'s known-failing list that no test has, as after a
//  rename.
std::vector<std::string> known_failing_without_a_test(const StoryFaultSweep& sweep);

//  Adds a test per cell of `sweep`, and one that its known-failing list
//  names only its tests.
void add_fault_sweep_tests(UnitTestDatabase& database, std::shared_ptr<const StoryFaultSweep> sweep);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
