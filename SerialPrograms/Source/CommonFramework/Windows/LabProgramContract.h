/*  Lab Program Contract
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What "describe_program" tells the lab server about a program without
 *  touching its console: its id and the static contract of its options.
 *  Each option with a key gets its key (the one "set" takes), its type, its
 *  default, and for numbers its bounds, for choices its entries and which of
 *  them are enabled. What depends on other options' values (visibility,
 *  check_validity()) is not part of it; the lab server checks that later on
 *  the loaded options.
 *
 *  The shape is version 1 of the lab server's program contract, in
 *  docs/design/Agent-Integration.md ("Program contract and preflight").
 *
 */

#ifndef PokemonAutomation_LabProgramContract_H
#define PokemonAutomation_LabProgramContract_H

#include <optional>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Options/BatchOption.h"
#include "CommonFramework/Panels/ProgramDescriptor.h"
#include "CommonFramework/ProgramPrecheckRun.h"

namespace PokemonAutomation{


//  The version of the reply below.
constexpr int LAB_PROGRAM_CONTRACT_SCHEMA = 1;

//  One entry per option of `options` that has a key, in the order they were
//  added: {"key", "type", ...}. Types: "boolean", "integer" and "number"
//  (with "min" and "max", null when unbounded), "string", "enum" and
//  "select" (with "entries": {"slug", "enabled"}), "group" (with its own
//  "options", and "toggleable"), and "other" for a type the contract does
//  not describe yet. Every type but "group" and "other" has a "default".
JsonArray lab_option_contract(const BatchOption& options);

//  The whole reply: {"schema", "id", "options", "prechecks"}. `prechecks`
//  lists the program's prechecks in order, each {"name", "needs",
//  "skippable"}, or is "unsupported" when the program declares no list.
JsonObject lab_program_contract(
    std::string_view id,
    const BatchOption& options,
    const std::optional<std::vector<PrecheckSummary>>& prechecks
);


//  The hash of a program's option JSON (its BatchOption::to_json()): the
//  SHA-256, in hex, of its compact text with the keys sorted. Any change of
//  any option changes it; the same options give the same hash in every app
//  instance. Lab/state.json and the "preflight" reply carry it.
std::string lab_options_hash(const JsonValue& options);

//  The reply of "preflight", version 1 like the contract above:
//  {"schema", "id", "verdict", "checks", "failure", "invalid", "restored",
//  "edited_keys", "options_hash"}. "verdict" is "invalid_options" when the program refused
//  the candidate ("invalid": {"key", "message"}, the key "" for a refused
//  combination), "unsupported" when it declares no precheck list, "failed"
//  at the first FAILED check ("failure": {"check", "message"}), else
//  "passed". "checks" lists {"name", "status", "message"} in the order they
//  ran, "status" one of "passed", "failed", "skipped". "restored" is
//  whether the program holds the prior options again; "edited_keys" lists
//  the top-level keys a user changed during the checks, which the run kept
//  (then "restored" is false). "options_hash" is lab_options_hash() of the
//  options the program holds once the run is over, read after the restore:
//  the prior options when "restored", not the candidate (null if unknown).
JsonObject lab_preflight_reply(
    std::string_view id,
    const PrecheckOnlyRun& run,
    const std::optional<std::string>& options_hash
);



}  // namespace PokemonAutomation
#endif
