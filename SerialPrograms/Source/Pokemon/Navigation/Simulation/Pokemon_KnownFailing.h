/*  Known Failing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The list of sim tests the program fails today, each with the change
 *  likely to fix it, shared by the games' fault sweeps and console-failure
 *  cases. A test on the list passes while it fails, so the suite stays
 *  green with the failure logged, and fails once it passes, so the entry
 *  comes off. stale() finds the entries that name no test: a renamed test
 *  would otherwise drop off the list silently and fail the suite.
 *
 */

#ifndef PokemonAutomation_Pokemon_KnownFailing_H
#define PokemonAutomation_Pokemon_KnownFailing_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The tests known to fail, by name, with their likely fixes.
class KnownFailing{
public:
    //  `fixes`: by test name (or case name, as the game keys it), the
    //  change likely to fix it.
    explicit KnownFailing(std::map<std::string, std::string> fixes);

    //  Runs test `name`'s `body`, an Exception it throws being its failure,
    //  and gives its result as the list sees it: as is if `name` is not
    //  listed; a pass that logs the failure if it is and fails; a failure
    //  "on KNOWN_FAILING (<fix>) but passes now: take it off the list." if
    //  it is and passes.
    UnitTestResult run(Logger& logger, std::string_view name, const std::function<UnitTestResult()>& body) const;

    //  The listed names not in `names` (the tests that exist), in order.
    std::vector<std::string> stale(const std::set<std::string>& names) const;

private:
    std::map<std::string, std::string, std::less<>> m_fixes;
};



} // namespace Pokemon



} // namespace PokemonAutomation
#endif
