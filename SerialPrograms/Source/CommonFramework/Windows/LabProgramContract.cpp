/*  Lab Program Contract
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <limits>
#include <string>
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

//  Fills in "type" and the fields of that type.
void describe_option(const ConfigOption& option, JsonObject& out){
    if (const GroupOption* group = dynamic_cast<const GroupOption*>(&option)){
        out["type"] = "group";
        out["toggleable"] = group->toggleable();
        out["options"] = lab_option_contract(*group);
        return;
    }
    if (const BatchOption* batch = dynamic_cast<const BatchOption*>(&option)){
        out["type"] = "group";
        out["toggleable"] = false;
        out["options"] = lab_option_contract(*batch);
        return;
    }
    if (const BooleanCheckBoxCell* cell = dynamic_cast<const BooleanCheckBoxCell*>(&option)){
        out["type"] = "boolean";
        out["default"] = cell->default_value();
        return;
    }
    if (const SimpleIntegerCellBase* cell = dynamic_cast<const SimpleIntegerCellBase*>(&option)){
        out["type"] = "integer";
        out["default"] = cell->default_value();
        out["min"] = cell->min_value();
        out["max"] = cell->max_value();
        return;
    }
    if (const FloatingPointCell* cell = dynamic_cast<const FloatingPointCell*>(&option)){
        out["type"] = "number";
        out["default"] = cell->default_value();
        out["min"] = number_bound(cell->min_value());
        out["max"] = number_bound(cell->max_value());
        return;
    }
    if (const StringCell* cell = dynamic_cast<const StringCell*>(&option)){
        out["type"] = "string";
        out["default"] = cell->default_value();
        return;
    }
    if (const IntegerEnumDropdownCell* cell = dynamic_cast<const IntegerEnumDropdownCell*>(&option)){
        const IntegerEnumDropdownDatabase& database = cell->database();
        out["type"] = "enum";
        out["default"] = database.find(cell->default_value())->slug;
        out["entries"] = enum_entries(database);
        return;
    }
    if (const StringSelectCell* cell = dynamic_cast<const StringSelectCell*>(&option)){
        out["type"] = "select";
        out["default"] = cell->default_slug();
        out["entries"] = select_entries(cell->database());
        return;
    }
    out["type"] = "other";
}

}



JsonArray lab_option_contract(const BatchOption& options){
    JsonArray contract;
    for (const auto& item : options.keyed_options()){
        if (item.second.empty()){
            continue;
        }
        JsonObject entry;
        entry["key"] = item.second;
        describe_option(*item.first, entry);
        contract.push_back(std::move(entry));
    }
    return contract;
}

JsonObject lab_program_contract(std::string_view id, const BatchOption& options){
    JsonObject contract;
    contract["schema"] = LAB_PROGRAM_CONTRACT_SCHEMA;
    contract["id"] = std::string(id);
    contract["options"] = lab_option_contract(options);
    contract["prechecks"] = "unsupported";
    return contract;
}



}
