/*  Story Step JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_StoryStepJson.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The wire names of the step kinds. They are not story_step_kind_name()'s
//  log names: the JSON names a MASH step "scene" and a GAME step
//  "game_action", after what the story author writes.
constexpr std::array<std::pair<StoryStepKind, std::string_view>, 15> KINDS{{
    {StoryStepKind::NEW_GAME, "new_game"},
    {StoryStepKind::OPTIONS, "options"},
    {StoryStepKind::ROUTE, "route"},
    {StoryStepKind::LEGS, "legs"},
    {StoryStepKind::STEP, "step"},
    {StoryStepKind::TALK, "talk"},
    {StoryStepKind::ANSWER, "answer"},
    {StoryStepKind::MASH, "scene"},
    {StoryStepKind::BATTLE, "battle"},
    {StoryStepKind::TRAIN, "train"},
    {StoryStepKind::HEAL, "heal"},
    {StoryStepKind::SHOP, "shop"},
    {StoryStepKind::SAVE, "save"},
    {StoryStepKind::READY, "ready"},
    {StoryStepKind::GAME, "game_action"},
}};



//  `value` as an object with no field outside `keys`.
const JsonObject& fields(const JsonValue& value, std::initializer_list<std::string_view> keys, std::string_view source){
    const JsonObject& object = value.to_object_throw(std::string(source));
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("{}: unsupported field {}", source, key));
        }
    }
    return object;
}

template <typename Enum, size_t N>
Enum read_enum(const std::array<std::pair<Enum, std::string_view>, N>& names, std::string_view name, std::string_view source){
    for (const auto& [value, label] : names){
        if (label == name){
            return value;
        }
    }
    throw ParseException(std::format("{}: unsupported value {}", source, name));
}

int integer(const JsonValue& value, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException(std::format("{}: coordinate exceeds native int", source));
    }
    return (int)number;
}

std::vector<std::string> read_strings(const JsonValue& value, std::string_view source){
    std::vector<std::string> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(item.to_string_throw(std::string(source)));
    }
    return result;
}

std::vector<std::vector<int64_t>> read_integer_rows(const JsonValue& value, std::string_view source){
    std::vector<std::vector<int64_t>> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        std::vector<int64_t> row;
        for (const JsonValue& number : item.to_array_throw(std::string(source))){
            row.push_back(number.to_integer_throw(std::string(source)));
        }
        result.push_back(std::move(row));
    }
    return result;
}

std::vector<StoryTimedLeg> read_timed_legs(const JsonValue& value, std::string_view source){
    std::vector<StoryTimedLeg> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonArray& pair = item.to_array_throw(std::string(source));
        if (pair.size() != 2){
            throw ParseException(std::format("{}: a timed leg is [turn degrees, ms]", source));
        }
        result.push_back({.turn = integer(pair[0], source), .ms = integer(pair[1], source)});
    }
    return result;
}

//  One arg's value read as `type`.
StoryArgValue read_arg(const JsonValue& value, StoryArgType type, std::string_view source){
    switch (type){
    case StoryArgType::BOOLEAN:
        return value.to_boolean_throw(std::string(source));
    case StoryArgType::INTEGER:
        return value.to_integer_throw(std::string(source));
    case StoryArgType::WORD:
        return value.to_string_throw(std::string(source));
    case StoryArgType::WORDS:
        return read_strings(value, source);
    case StoryArgType::INTEGER_ROWS:
        return read_integer_rows(value, source);
    case StoryArgType::TIMED_LEGS:
        return read_timed_legs(value, source);
    }
    throw ParseException(std::format("{}: unsupported story arg type", source));
}

//  The step's action among `actions`: the named one, or the kind's default.
//  Null for a kind with no action that names none.
const StoryActionSpec* step_action(
    const StoryStep& step, std::span<const StoryActionSpec> actions, std::string_view source
){
    if (step.kind == StoryStepKind::GAME && step.action.empty()){
        throw ParseException(std::format("{}: game_action step {} names no action", source, step.name));
    }
    const StoryActionSpec* spec = find_story_action(actions, step.kind, step.action);
    if (spec == nullptr && !step.action.empty()){
        throw ParseException(std::format("{}: step {}: unknown action {}", source, step.name, step.action));
    }
    return spec;
}

//  The step's args, read against its action's spec, with the fallbacks
//  filled in.
StoryActionArgs read_args(
    const JsonValue* value, const StoryActionSpec* spec, const StoryStep& step, std::string_view source
){
    const StoryActionSpec none{.kind = step.kind, .name = "", .args = {}};
    const StoryActionSpec& action = spec ? *spec : none;
    StoryActionArgs result;
    if (value != nullptr){
        for (const auto& [key, item] : value->to_object_throw(std::string(source))){
            const auto arg = std::find_if(action.args.begin(), action.args.end(), [&](const StoryArgSpec& candidate){
                return candidate.key == key;
            });
            if (arg == action.args.end()){
                throw ParseException(std::format("{}: step {}: unknown argument \"{}\"", source, step.name, key));
            }
            result[key] = read_arg(item, arg->type, source);
        }
    }
    const std::vector<std::string> errors = story_args_errors(action, result);
    if (!errors.empty()){
        throw ParseException(std::format("{}: step {}: {}", source, step.name, errors.front()));
    }
    return with_fallbacks(action, std::move(result));
}


//  Reads "metadata" into `checkpoint`. A missing phase is testing.
void read_metadata(const JsonValue& value, std::string_view source, StoryCheckpoint& checkpoint){
    const JsonObject& root = fields(value, {"phase", "source"}, source);
    if (const JsonValue* field = root.get_value("phase")){
        const std::string name = field->to_string_throw();
        const std::optional<StoryPhase> phase = read_story_phase(name);
        if (!phase){
            throw ParseException(std::format("{}: unknown phase {}", source, name));
        }
        checkpoint.metadata.phase = *phase;
    }
    if (const JsonValue* field = root.get_value("source")){
        checkpoint.metadata.source = field->to_string_throw();
    }
}


} // namespace


DeclaredPose read_declared_pose(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"place", "spot", "heading"}, source);
    ZoneDeclared result;
    result.place = root.get_string_throw("place");
    if (const JsonValue* field = root.get_value("spot")){
        result.spot = field->to_string_throw(std::string(source));
    }
    if (const JsonValue* field = root.get_value("heading")){
        result.heading = integer(*field, source);
    }
    return result;
}

StoryStep read_story_step(const JsonValue& value, std::string_view source, std::span<const StoryActionSpec> actions){
    const JsonObject& root = fields(
        value, {"id", "kind", "why", "action", "args"}, source
    );
    StoryStep result;
    result.name = root.get_string_throw("id");
    if (result.name.empty()){
        throw ParseException("Empty story step id.");
    }
    result.kind = read_enum(KINDS, root.get_string_throw("kind"), source);
    if (const JsonValue* field = root.get_value("why")){
        result.why = field->to_string_throw();
    }
    if (const JsonValue* field = root.get_value("action")){
        result.action = field->to_string_throw();
    }
    result.args = read_args(root.get_value("args"), step_action(result, actions, source), result, source);
    return result;
}

StoryCheckpoint read_story_checkpoint(const JsonValue& value, std::string_view source, std::span<const StoryActionSpec> actions){
    const JsonObject& root = fields(value, {"id", "title", "start", "events", "metadata"}, source);
    StoryCheckpoint result;
    result.id = root.get_string_throw("id");
    if (result.id.empty()){
        throw ParseException("Empty story checkpoint id.");
    }
    result.title = root.get_string_throw("title");
    if (const JsonValue* start = root.get_value("start")){
        result.start = read_declared_pose(*start, source);
    }
    std::set<std::string> names;
    for (const JsonValue& event : root.get_array_throw("events")){
        StoryStep step = read_story_step(event, source, actions);
        if (step.name.empty() || !names.insert(step.name).second){
            throw ParseException("Empty or duplicate story event id.");
        }
        result.steps.push_back(std::move(step));
    }
    if (const JsonValue* metadata = root.get_value("metadata")){
        read_metadata(*metadata, source, result);
    }
    return result;
}



} // namespace Pokemon
} // namespace PokemonAutomation
