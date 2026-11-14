/*  Sim Faults Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The fault counters on their own: which chances a counter faults, what
 *  it counts, that each class counts apart, and the classes' names.
 *
 */

#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_SimFaults.h"
#include "Pokemon_SimFaults_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct SimFaultsTestCase{
    struct Input{
        UnitTestResult (*body)();
    };

    struct Expectation{
        std::string_view description;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_SimFaults : public UnitTest{
public:
    explicit Test_SimFaults(SimFaultsTestCase scenario)
        : UnitTest("Pokemon::Navigation - Sim faults: " + scenario.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.body();
    }

private:
    SimFaultsTestCase m_scenario;
};

//  What `chances` chances of FaultClass::LOST_TAP did with `faulted`
//  named, taken as a game's sim takes them, a chance of
//  FaultClass::WARP_NO_OP (none named) after each.
struct Counted{
    std::vector<int> faulted_at;
    int chances = 0;
    int faulted = 0;
    int other_chances = 0;
    int other_faulted = 0;
    int never_reached = 0;
};

Counted count(const std::set<int>& faulted, int chances){
    SimFaults faults({{FaultClass::LOST_TAP, faulted}});
    Counted out;
    for (int chance = 1; chance <= chances; chance++){
        if (faults.next(FaultClass::LOST_TAP)){
            out.faulted_at.push_back(chance);
        }
        faults.next(FaultClass::WARP_NO_OP);
    }
    out.chances = faults.chances(FaultClass::LOST_TAP);
    out.faulted = faults.faulted(FaultClass::LOST_TAP);
    out.other_chances = faults.chances(FaultClass::WARP_NO_OP);
    out.other_faulted = faults.faulted(FaultClass::WARP_NO_OP);
    out.never_reached = faults.chances(FaultClass::DARK_CAVE);
    return out;
}

//  "[1, 3]".
std::string numbers_text(const std::vector<int>& numbers){
    std::string out;
    for (int number : numbers){
        if (!out.empty()){
            out += ", ";
        }
        out += std::format("{}", number);
    }
    return std::format("[{}]", out);
}

UnitTestResult expect_counted(const std::set<int>& faulted, int chances, const std::vector<int>& faulted_at){
    const Counted counted = count(faulted, chances);
    if (counted.faulted_at != faulted_at){
        return std::format("Faulted chances {}, expected {}.", numbers_text(counted.faulted_at), numbers_text(faulted_at));
    }
    if (counted.chances != chances || counted.faulted != static_cast<int>(faulted_at.size())){
        return std::format("Counted {} chances, {} faulted; expected {}, {}.", counted.chances, counted.faulted, chances, faulted_at.size());
    }
    if (counted.other_chances != chances || counted.other_faulted != 0){
        return std::format("The other class counted {} chances, {} faulted; expected {}, 0.", counted.other_chances, counted.other_faulted, chances);
    }
    if (counted.never_reached != 0){
        return std::format("A class never reached counted {} chances.", counted.never_reached);
    }
    return true;
}

UnitTestResult test_none_named(){
    return expect_counted({}, 3, {});
}

UnitTestResult test_named_from_one(){
    return expect_counted({1, 3}, 4, {1, 3});
}

UnitTestResult test_never_reached(){
    return expect_counted({2, 9}, 3, {2});
}

UnitTestResult test_no_chances(){
    return expect_counted({1}, 0, {});
}

UnitTestResult test_class_names(){
    std::map<std::string_view, FaultClass> seen;
    for (FaultClass fault : fault_classes()){
        const std::string_view name = fault_class_name(fault);
        if (name == "?" || !seen.emplace(name, fault).second){
            return std::format("Fault class {} has no name of its own: {}", static_cast<int>(fault), name);
        }
    }
    return true;
}

//  fault_classes() restates the enum, so it must list every value: the
//  names stop at the first value past the enum's last (fault_class_name()
//  is a switch, so a value with no name fails to build).
UnitTestResult test_classes_match_enum(){
    const std::vector<FaultClass>& classes = fault_classes();
    size_t named = 0;
    while (fault_class_name(static_cast<FaultClass>(named)) != "?"){
        named++;
    }
    if (named != classes.size()){
        return std::format("The enum has {} classes, fault_classes() lists {}.", named, classes.size());
    }
    for (size_t c = 0; c < classes.size(); c++){
        if (classes[c] != static_cast<FaultClass>(c)){
            return std::format("fault_classes() lists {} at position {}, not in the enum's order.", static_cast<int>(classes[c]), c);
        }
    }
    return true;
}

const SimFaultsTestCase SIM_FAULTS_CASES[] = {
    {.name = "none named faults none", .input = {.body = test_none_named}, .expectation = {.description = "3 chances, none faulted"}},
    {.name = "the named chances are faulted, from 1", .input = {.body = test_named_from_one}, .expectation = {.description = "1 and 3 of 4 faulted"}},
    {.name = "a chance never reached is not faulted", .input = {.body = test_never_reached}, .expectation = {.description = "2 of 3 faulted, 9 never reached"}},
    {.name = "no chances, nothing counted", .input = {.body = test_no_chances}, .expectation = {.description = "0 chances, none faulted"}},
    {.name = "each class has its own name", .input = {.body = test_class_names}, .expectation = {.description = "no two classes share a name"}},
    {.name = "fault_classes() lists every class in order", .input = {.body = test_classes_match_enum}, .expectation = {.description = "the list is the enum's values, in order"}},
};

} // namespace


void add_tests_SimFaults(UnitTestDatabase& database){
    for (const SimFaultsTestCase& scenario : SIM_FAULTS_CASES){
        database.add<Test_SimFaults>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
