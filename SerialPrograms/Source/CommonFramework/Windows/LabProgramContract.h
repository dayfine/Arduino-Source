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

#include <string_view>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Options/BatchOption.h"

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
//  is "unsupported" until the program declares a precheck list.
JsonObject lab_program_contract(std::string_view id, const BatchOption& options);



}  // namespace PokemonAutomation
#endif
