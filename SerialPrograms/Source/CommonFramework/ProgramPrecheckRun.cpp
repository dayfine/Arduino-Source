/*  Program Precheck Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <expected>
#include <format>
#include <exception>
#include <string_view>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
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


//  `current` (a program's option JSON) with the values of `candidate` put in.
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

//  The first option of `candidate` whose value `loaded` (the options read
//  back after loading the merged candidate) does not hold.
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





//  The top-level keys of `x` and `y` (option JSON objects) whose values
//  differ or that only one has, in key order.
std::vector<std::string> changed_keys(const JsonValue& x, const JsonValue& y){
    std::vector<std::string> keys;
    const JsonObject* a = x.to_object();
    const JsonObject* b = y.to_object();
    if (a == nullptr || b == nullptr){
        return keys;
    }
    for (const auto& item : *a){
        const JsonValue* other = b->get_value(item.first);
        if (other == nullptr || *other != item.second){
            keys.push_back(item.first);
        }
    }
    for (const auto& item : *b){
        if (a->get_value(item.first) == nullptr){
            keys.push_back(item.first);
        }
    }
    return keys;
}

std::string key_list(const JsonValue& candidate){
    std::string keys;
    const JsonObject* object = candidate.to_object();
    if (object == nullptr){
        return keys;
    }
    for (const auto& item : *object){
        keys += keys.empty() ? item.first : std::format(", {}", item.first);
    }
    return keys;
}

//  The options a checks-only run staged and must put back.
struct StagedOptions{
    std::optional<JsonValue> prior = {};
    std::optional<JsonValue> staged = {};
};

//  Step 1 of run_prechecks_only(), on the options thread.
void stage_options(const JsonValue& candidate, PrecheckOnlyTarget& target, StagedOptions& options, PrecheckOnlyRun& run){
    options.prior = target.options();
    std::expected<JsonValue, RefusedOption> merged = merge_candidate_options(*options.prior, candidate);
    if (!merged){
        run.refused = std::move(merged.error());
        return;
    }
    target.load_options(*merged);
    options.staged = target.options();
    run.refused = find_refused_option(candidate, *options.staged);
    if (run.refused){
        return;
    }
    std::string error = target.check_validity();
    if (!error.empty()){
        run.refused = RefusedOption{"", std::move(error)};
    }
}

//  Step 3 of run_prechecks_only(). Never throws.
void restore_options(
    const StagedOptions& options, const OptionsThreadCall& on_options_thread,
    PrecheckOnlyTarget& target, Logger& logger, PrecheckOnlyRun& run
){
    run.restored = false;
    if (!options.prior){
        return;
    }
    try{
        on_options_thread([&]{
            const JsonValue current = target.options();
            if (options.staged && current != *options.staged){
                run.edited_keys = changed_keys(*options.staged, current);
                return;
            }
            target.load_options(*options.prior);
            run.restored = target.options() == *options.prior;
        });
    }catch (Exception& e){
        logger.log(std::format("Start checks only: the prior options did not load back: {}", e.message()), COLOR_RED);
        return;
    }catch (std::exception& e){
        logger.log(std::format("Start checks only: the prior options did not load back: {}", e.what()), COLOR_RED);
        return;
    }
    if (!run.edited_keys.empty()){
        logger.log("Start checks only: options changed during the checks, kept as they are.", COLOR_RED);
    }else if (!run.restored){
        logger.log("Start checks only: the prior options did not load back.", COLOR_RED);
    }
}


}  // namespace


PrecheckOnlyRun run_prechecks_only(
    const JsonValue& candidate,
    const OptionsThreadCall& on_options_thread,
    PrecheckOnlyTarget& target,
    Logger& logger
){
    logger.log(std::format("Start checks only, with options: {}", key_list(candidate)));
    PrecheckOnlyRun run;
    StagedOptions options;
    try{
        on_options_thread([&]{
            stage_options(candidate, target, options, run);
        });
        if (!run.refused){
            run.checks = target.run_checks();
        }
    }catch (OperationCancelledException&){
        logger.log("Start checks only: cancelled.", COLOR_RED);
        run.checks.reset();
        run.cancelled = true;
    }catch (...){
        restore_options(options, on_options_thread, target, logger, run);
        throw;
    }
    if (!options.staged){
        //  Refused before anything was loaded.
        run.restored = true;
        return run;
    }
    restore_options(options, on_options_thread, target, logger, run);
    return run;
}



}  // namespace PokemonAutomation
