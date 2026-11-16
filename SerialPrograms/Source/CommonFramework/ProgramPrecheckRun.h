/*  Program Precheck Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checks-only run of a program (ProgramSession::Prechecks::run_only()):
 *  candidate options are loaded over the current ones, the program's
 *  prechecks run without starting it, and the prior options are loaded back
 *  unless a user changed them meanwhile. Options are loaded and read back on
 *  the thread that owns them (the UI thread in the app); the checks run on
 *  the calling thread.
 *  The lab's "preflight" command uses it to find a setup mistake before a run
 *  resets or saves anything.
 *
 */

#ifndef PokemonAutomation_CommonFramework_ProgramPrecheckRun_H
#define PokemonAutomation_CommonFramework_ProgramPrecheckRun_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
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
    //  The top-level option keys a user changed while the checks ran. The
    //  run then leaves the options as the user left them, and `restored` is
    //  false.
    std::vector<std::string> edited_keys = {};
};


//  What a checks-only run acts on: a program's options and its prechecks.
class PrecheckOnlyTarget{
public:
    virtual ~PrecheckOnlyTarget() = default;

    //  The program's option JSON. Called on the options thread.
    virtual JsonValue options() const = 0;
    //  Loads option JSON into the program. Called on the options thread.
    virtual void load_options(const JsonValue& options) = 0;
    //  Why the program refuses the loaded options as a whole, or "" when it
    //  takes them (ProgramInstance::check_validity()). Called on the options
    //  thread.
    virtual std::string check_validity() const = 0;
    //  Runs the program's prechecks on the loaded options, on the calling
    //  thread. Nothing when the program declares no precheck list.
    virtual std::optional<std::vector<PrecheckReport>> run_checks() = 0;
};

//  Runs `body` on the thread that owns the program's options and returns
//  once it has run. Throws what `body` throws, or when it cannot run it.
using OptionsThreadCall = std::function<void(const std::function<void()>& body)>;

//  A checks-only run of `target` with `candidate`, an object of option keys
//  to change; a group's value is an object of the group's own keys, of which
//  only those given change:
//    1.  On the options thread: loads the candidate over the current options.
//        Refuses a key the program does not have, a candidate that is not an
//        object, a value the program does not hold once loaded (a number
//        clamped to its bounds, a disabled or unknown choice ignored; 2 and
//        2.0 are the same) and a combination it refuses (check_validity()).
//        A refused key is a dotted key path.
//    2.  Unless refused, runs the checks on the calling thread.
//    3.  On the options thread: loads the prior options back, also when a
//        check throws, unless the options differ from what step 1 loaded (a
//        user edited them): those are kept and named in `edited_keys`.
//  A failed restore is logged and leaves `restored` false; it never throws.
PrecheckOnlyRun run_prechecks_only(
    const JsonValue& candidate,
    const OptionsThreadCall& on_options_thread,
    PrecheckOnlyTarget& target,
    Logger& logger
);





}  // namespace PokemonAutomation
#endif
