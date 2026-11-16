/*  Program Precheck Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checks-only run of a program (ProgramSession::Prechecks::run_only()):
 *  candidate options are loaded over the current ones, the program's
 *  prechecks run without starting it, and the prior options are loaded back.
 *  The lab's "preflight" command uses it to find a setup mistake before a run
 *  resets or saves anything.
 *
 */

#ifndef PokemonAutomation_CommonFramework_ProgramPrecheckRun_H
#define PokemonAutomation_CommonFramework_ProgramPrecheckRun_H

#include <expected>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Panels/ProgramDescriptor.h"

namespace PokemonAutomation{


//  A candidate option the program refused, and why. The key is a dotted key
//  path ("NOTIFICATIONS.Enabled"); it is empty when the program refused the
//  combination of options (check_validity()), which names no key.
struct RefusedOption{
    std::string key = {};
    std::string message = {};
};

//  What a checks-only run found.
struct PrecheckOnlyRun{
    //  Set when the program refused the candidate options; no check ran.
    std::optional<RefusedOption> refused = {};
    //  The checks in the order they ran, up to the first FAILED one. Nothing
    //  when the program declares no precheck list, or refused the options.
    std::optional<std::vector<PrecheckReport>> checks = {};
    //  Whether the program holds the prior options again afterwards.
    bool restored = false;
};


//  `current` (a program's option JSON) with the values of `candidate` put in.
//  A candidate is an object of option keys; a group's value is an object of
//  the group's own keys, of which only those given change. Refuses a key that
//  `current` does not have, or a candidate that is not an object.
std::expected<JsonValue, RefusedOption> merge_candidate_options(
    const JsonValue& current, const JsonValue& candidate
);

//  The first option of `candidate` whose value `loaded` (the options read
//  back after loading the merged candidate) does not hold: a number clamped
//  to its bounds, a disabled or unknown choice ignored. Numbers compare by
//  value, so 2 and 2.0 are the same.
std::optional<RefusedOption> find_refused_option(
    const JsonValue& candidate, const JsonValue& loaded
);



}  // namespace PokemonAutomation
#endif
