/*  Lab Program Contract
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Cryptography/SHA256.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOptionBase.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "LabProgramContract.h"

namespace PokemonAutomation{


namespace{

//  A floating-point bound, or null for the "no bound" default.
JsonValue number_bound(double bound){
    if (bound <= -std::numeric_limits<double>::max() || bound >= std::numeric_limits<double>::max()){
        return JsonValue();
    }
    return bound;
}

JsonArray enum_entries(const IntegerEnumDropdownDatabase& database){
    JsonArray entries;
    for (size_t value : database.all_values()){
        const EnumEntry* entry = database.find(value);
        JsonObject item;
        item["slug"] = entry->slug;
        item["enabled"] = entry->enabled;
        entries.push_back(std::move(item));
    }
    return entries;
}

JsonArray select_entries(const StringSelectDatabase& database){
    JsonArray entries;
    for (const StringSelectEntry& entry : database.case_list()){
        JsonObject item;
        item["slug"] = entry.slug;
        item["enabled"] = true;
        entries.push_back(std::move(item));
    }
    return entries;
}

//  A group's "type" and options. Nothing if `option` is not a group.
std::optional<JsonObject> describe_group(const ConfigOption& option){
    JsonObject out;
    const BatchOption* batch = dynamic_cast<const BatchOption*>(&option);
    if (batch == nullptr){
        return std::nullopt;
    }
    const GroupOption* group = dynamic_cast<const GroupOption*>(&option);
    out["type"] = "group";
    out["toggleable"] = group != nullptr && group->toggleable();
    out["options"] = lab_option_contract(*batch);
    return out;
}

//  A boolean's, number's or string's "type", "default" and bounds. Nothing
//  if `option` is none of them.
std::optional<JsonObject> describe_value(const ConfigOption& option){
    JsonObject out;
    if (const BooleanCheckBoxCell* cell = dynamic_cast<const BooleanCheckBoxCell*>(&option)){
        out["type"] = "boolean";
        out["default"] = cell->default_value();
        return out;
    }
    if (const SimpleIntegerCellBase* cell = dynamic_cast<const SimpleIntegerCellBase*>(&option)){
        out["type"] = "integer";
        out["default"] = cell->default_value();
        out["min"] = cell->min_value();
        out["max"] = cell->max_value();
        return out;
    }
    if (const FloatingPointCell* cell = dynamic_cast<const FloatingPointCell*>(&option)){
        out["type"] = "number";
        out["default"] = cell->default_value();
        out["min"] = number_bound(cell->min_value());
        out["max"] = number_bound(cell->max_value());
        return out;
    }
    if (const StringCell* cell = dynamic_cast<const StringCell*>(&option)){
        out["type"] = "string";
        out["default"] = cell->default_value();
        return out;
    }
    return std::nullopt;
}

//  A choice's "type", "default" and "entries". Nothing if `option` is not a
//  choice.
std::optional<JsonObject> describe_choice(const ConfigOption& option){
    JsonObject out;
    if (const IntegerEnumDropdownCell* cell = dynamic_cast<const IntegerEnumDropdownCell*>(&option)){
        const IntegerEnumDropdownDatabase& database = cell->database();
        out["type"] = "enum";
        out["default"] = database.find(cell->default_value())->slug;
        out["entries"] = enum_entries(database);
        return out;
    }
    if (const StringSelectCell* cell = dynamic_cast<const StringSelectCell*>(&option)){
        out["type"] = "select";
        out["default"] = cell->default_slug();
        out["entries"] = select_entries(cell->database());
        return out;
    }
    return std::nullopt;
}

JsonArray precheck_list(const std::vector<PrecheckSummary>& prechecks){
    JsonArray list;
    for (const PrecheckSummary& check : prechecks){
        JsonObject item;
        item["name"] = check.name;
        item["needs"] = check.needs;
        item["skippable"] = check.skippable;
        list.push_back(std::move(item));
    }
    return list;
}

std::string_view precheck_status_name(PrecheckReportStatus status){
    switch (status){
    case PrecheckReportStatus::PASSED:
        return "passed";
    case PrecheckReportStatus::FAILED:
        return "failed";
    case PrecheckReportStatus::SKIPPED:
        return "skipped";
    }
    return "failed";
}

//  The "checks" of a preflight reply.
JsonArray precheck_reports(const std::vector<PrecheckReport>& checks){
    JsonArray list;
    for (const PrecheckReport& check : checks){
        JsonObject item;
        item["name"] = check.name;
        item["status"] = std::string(precheck_status_name(check.status));
        item["message"] = check.message;
        list.push_back(std::move(item));
    }
    return list;
}

//  The fields of `option` for its entry: "type" and those of that type.
JsonObject describe_option(const ConfigOption& option){
    std::optional<JsonObject> described = describe_group(option);
    if (!described){
        described = describe_value(option);
    }
    if (!described){
        described = describe_choice(option);
    }
    if (described){
        return std::move(*described);
    }
    JsonObject other;
    other["type"] = "other";
    return other;
}

}  // namespace



JsonArray lab_option_contract(const BatchOption& options){
    JsonArray contract;
    for (const auto& item : keyed_options(options)){
        if (item.second.empty()){
            continue;
        }
        JsonObject entry = describe_option(*item.first);
        entry["key"] = item.second;
        contract.push_back(std::move(entry));
    }
    return contract;
}

JsonObject lab_program_contract(
    std::string_view id,
    const BatchOption& options,
    const std::optional<std::vector<PrecheckSummary>>& prechecks
){
    JsonObject contract;
    contract["schema"] = LAB_PROGRAM_CONTRACT_SCHEMA;
    contract["id"] = std::string(id);
    contract["options"] = lab_option_contract(options);
    if (prechecks){
        contract["prechecks"] = precheck_list(*prechecks);
    }else{
        contract["prechecks"] = "unsupported";
    }
    return contract;
}


std::string lab_options_hash(const JsonValue& options){
    const std::string text = options.dump(-1);
    SHA256 sha;
    sha.push(text.data(), text.size());
    sha.finish();
    return sha.get_hash_hex();
}

JsonObject lab_preflight_reply(
    std::string_view id,
    const PrecheckOnlyRun& run,
    const std::optional<std::string>& options_hash
){
    JsonObject reply;
    reply["schema"] = LAB_PROGRAM_CONTRACT_SCHEMA;
    reply["id"] = std::string(id);
    reply["checks"] = JsonArray();
    reply["failure"] = JsonValue();
    reply["invalid"] = JsonValue();
    reply["restored"] = run.restored;
    reply["options_hash"] = options_hash ? JsonValue(*options_hash) : JsonValue();
    if (run.refused){
        JsonObject invalid;
        invalid["key"] = run.refused->key;
        invalid["message"] = run.refused->message;
        reply["invalid"] = std::move(invalid);
        reply["verdict"] = "invalid_options";
        return reply;
    }
    if (!run.checks){
        reply["verdict"] = "unsupported";
        return reply;
    }
    reply["checks"] = precheck_reports(*run.checks);
    const auto failed = std::find_if(run.checks->begin(), run.checks->end(), [](const PrecheckReport& check){
        return check.status == PrecheckReportStatus::FAILED;
    });
    if (failed == run.checks->end()){
        reply["verdict"] = "passed";
        return reply;
    }
    JsonObject failure;
    failure["check"] = failed->name;
    failure["message"] = failed->message;
    reply["failure"] = std::move(failure);
    reply["verdict"] = "failed";
    return reply;
}



}  // namespace PokemonAutomation
