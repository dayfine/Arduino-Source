/*  Program Precheck Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string_view>
#include "Common/Cpp/Json/JsonObject.h"
#include "ProgramPrecheckRun.h"

namespace PokemonAutomation{

namespace{


std::string key_path(std::string_view prefix, std::string_view key){
    return prefix.empty() ? std::string(key) : std::format("{}.{}", prefix, key);
}

//  Puts the values of `candidate` into `target`, an object, recursing into
//  groups. `prefix` is the key path of `target`.
std::optional<RefusedOption> merge_into(const JsonObject& candidate, std::string_view prefix, JsonObject& target){
    for (const auto& item : candidate){
        const std::string path = key_path(prefix, item.first);
        JsonValue* slot = target.get_value(item.first);
        if (slot == nullptr){
            return RefusedOption{path, std::format("The program has no option {}.", path)};
        }
        JsonObject* group = slot->to_object();
        const JsonObject* values = item.second.to_object();
        if (group != nullptr && values != nullptr){
            std::optional<RefusedOption> refused = merge_into(*values, path, *group);
            if (refused){
                return refused;
            }
            continue;
        }
        if (group != nullptr || values != nullptr){
            return RefusedOption{path, std::format("Option {} is a group: its value is an object of its own options.", path)};
        }
        *slot = item.second.clone();
    }
    return std::nullopt;
}

bool same_value(const JsonValue& x, const JsonValue& y){
    if (x.is_float() && y.is_float()){
        return x.to_double_default() == y.to_double_default();
    }
    return x == y;
}

std::optional<RefusedOption> find_refused_in(const JsonObject& candidate, const JsonObject& loaded, std::string_view prefix){
    for (const auto& item : candidate){
        const std::string path = key_path(prefix, item.first);
        const JsonValue* now = loaded.get_value(item.first);
        if (now == nullptr){
            return RefusedOption{path, std::format("The program has no option {}.", path)};
        }
        const JsonObject* values = item.second.to_object();
        const JsonObject* group = now->to_object();
        if (values != nullptr && group != nullptr){
            std::optional<RefusedOption> refused = find_refused_in(*values, *group, path);
            if (refused){
                return refused;
            }
            continue;
        }
        if (!same_value(item.second, *now)){
            return RefusedOption{
                path,
                std::format("The program holds {} for {}, not {}.", now->dump(-1), path, item.second.dump(-1))
            };
        }
    }
    return std::nullopt;
}


}  // namespace



std::expected<JsonValue, RefusedOption> merge_candidate_options(
    const JsonValue& current, const JsonValue& candidate
){
    const JsonObject* values = candidate.to_object();
    if (values == nullptr){
        return std::unexpected(RefusedOption{"", "The options to check must be a JSON object."});
    }
    JsonValue merged = current.clone();
    JsonObject* target = merged.to_object();
    if (target == nullptr){
        return std::unexpected(RefusedOption{"", "The program's options are not a JSON object."});
    }
    std::optional<RefusedOption> refused = merge_into(*values, "", *target);
    if (refused){
        return std::unexpected(std::move(*refused));
    }
    return merged;
}

std::optional<RefusedOption> find_refused_option(
    const JsonValue& candidate, const JsonValue& loaded
){
    const JsonObject* values = candidate.to_object();
    const JsonObject* now = loaded.to_object();
    if (values == nullptr || now == nullptr){
        return RefusedOption{"", "The options to check must be a JSON object."};
    }
    return find_refused_in(*values, *now, "");
}



}  // namespace PokemonAutomation
