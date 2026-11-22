/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
#include <map>
#include <set>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

StartMenu read_start_menu(std::string_view name, std::string_view source){
    if (name == "before_pokemon"){
        return StartMenu::BEFORE_POKEMON;
    }
    if (name == "before_pokedex"){
        return StartMenu::BEFORE_POKEDEX;
    }
    if (name == "full"){
        return StartMenu::FULL;
    }
    throw ParseException(std::format("{}: unknown START menu {}", source, name));
}

StoryHandoff read_handoff(std::string_view name, std::string_view source){
    if (name == "sid_helper"){
        return StoryHandoff::SID_HELPER;
    }
    if (name == "starter_rng"){
        return StoryHandoff::STARTER_RNG;
    }
    if (name == "gift_rng"){
        return StoryHandoff::GIFT_RNG;
    }
    throw ParseException(std::format("{}: unknown handoff {}", source, name));
}

PokemonFRLG_RngTarget read_gift(std::string_view name, std::string_view source){
    if (name == "magikarp"){
        return PokemonFRLG_RngTarget::magikarp;
    }
    if (name == "eevee"){
        return PokemonFRLG_RngTarget::eevee;
    }
    if (name == "lapras"){
        return PokemonFRLG_RngTarget::lapras;
    }
    throw ParseException(std::format("{}: unknown gift {}", source, name));
}

void only_fields(const JsonObject& object, std::initializer_list<std::string_view> keys, std::string_view source){
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("{}: unsupported field \"{}\"", source, key));
        }
    }
}

int read_count(const JsonObject& object, const std::string& key, std::string_view source){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return 0;
    }
    const int64_t count = value->to_integer_throw(std::string(source));
    if (count < 0 || count > 999){
        throw ParseException(std::format("{}: {} out of range", source, key));
    }
    return static_cast<int>(count);
}

Supplies read_supplies(const JsonObject& execution, std::string_view source){
    Supplies supplies;
    if (const JsonObject* items = execution.get_object("supplies")){
        only_fields(*items, {"potions", "antidotes", "parlyz_heals"}, source);
        supplies.potions = read_count(*items, "potions", source);
        supplies.antidotes = read_count(*items, "antidotes", source);
        supplies.parlyz_heals = read_count(*items, "parlyz_heals", source);
    }
    if (const std::string* lead = execution.get_string("lead")){
        if (*lead != "after_brock"){
            throw ParseException(std::format("{}: unknown lead {}", source, *lead));
        }
        supplies.lead = story_lead_after_brock();
    }else if (execution.get_value("lead") != nullptr){
        throw ParseException(std::format("{}: lead must be a string", source));
    }
    if (const JsonValue* slots = execution.get_value("protected_slots")){
        for (const JsonValue& slot : slots->to_array_throw(std::string(source))){
            const int64_t number = slot.to_integer_throw(std::string(source));
            if (number < 2 || number > 6){
                throw ParseException(std::format("{}: protected slot {} out of range (2-6)", source, number));
            }
            supplies.protected_slots.push_back(static_cast<size_t>(number) - 1);
        }
    }
    return supplies;
}

std::set<std::string> read_note_keys(const JsonObject& execution, std::string_view source){
    std::set<std::string> keys;
    const JsonValue* notes = execution.get_value("notes");
    if (notes == nullptr){
        return keys;
    }
    for (const auto& [key, lines] : notes->to_object_throw(std::string(source))){
        for (const JsonValue& line : lines.to_array_throw(std::string(source))){
            line.to_string_throw(std::string(source));
        }
        keys.insert(key);
    }
    return keys;
}

Pokemon::StoryVerify read_verify(const JsonObject& verify, std::string_view source){
    only_fields(verify, {"tries", "min_pass", "reset_budget"}, source);
    const int64_t tries = verify.get_integer_throw("tries", std::string(source));
    const int64_t min_pass = verify.get_integer_throw("min_pass", std::string(source));
    const int64_t reset_budget = verify.get_integer_throw("reset_budget", std::string(source));
    if (tries < 0 || min_pass < 0 || min_pass > tries || (tries > 0 && min_pass == 0) || reset_budget <= 0){
        throw ParseException(std::format(
            "{}: invalid verify policy (tries={}, min_pass={}, reset_budget={})",
            source, tries, min_pass, reset_budget
        ));
    }
    return Pokemon::StoryVerify{
        .tries = static_cast<size_t>(tries),
        .min_pass = static_cast<size_t>(min_pass),
        .reset_budget = static_cast<size_t>(reset_budget),
    };
}

void read_postconditions(const JsonObject& postconditions, std::string_view source, StoryCheckpoint& checkpoint){
    only_fields(postconditions, {"position", "items", "flags", "poison_survival"}, source);
    if (const JsonValue* position = postconditions.get_value("position")){
        checkpoint.postconditions.position = Pokemon::read_grid_pose(*position, source);
    }
    if (const JsonValue* items = postconditions.get_value("items")){
        for (const auto& [item, count] : items->to_object_throw(std::string(source))){
            const int64_t n = count.to_integer_throw(std::string(source));
            if (n < 0){
                throw ParseException(std::format("{}: negative count of {}", source, item));
            }
            checkpoint.postconditions.items.push_back(StoryItem{
                .item = item,
                .quantity = static_cast<size_t>(n),
            });
        }
    }
    if (const JsonValue* flags = postconditions.get_value("flags")){
        for (const JsonValue& flag : flags->to_array_throw(std::string(source))){
            checkpoint.postconditions.flags.push_back(flag.to_string_throw(std::string(source)));
        }
    }
    if (const JsonValue* poison = postconditions.get_value("poison_survival")){
        const JsonObject& ps = poison->to_object_throw(std::string(source));
        only_fields(ps, {"steps", "max_hp"}, source);
        const int64_t steps = ps.get_integer_throw("steps", std::string(source));
        const int64_t max_hp = ps.get_integer_throw("max_hp", std::string(source));
        if (steps <= 0 || max_hp <= 0){
            throw ParseException(std::format("{}: poison_survival steps and max_hp must be positive", source));
        }
        checkpoint.poison_survival = PoisonSurvivalPostcondition{
            .steps = static_cast<int>(steps),
            .max_hp = static_cast<int>(max_hp),
        };
    }
}

void read_metadata(const JsonObject& cp_obj, std::string_view source, StoryCheckpoint& checkpoint){
    const JsonValue* meta_val = cp_obj.get_value("metadata");
    if (meta_val == nullptr){
        return;
    }
    const JsonObject& metadata = meta_val->to_object_throw(std::string(source));
    only_fields(metadata, {"phase", "postconditions"}, source);
    if (const JsonValue* post = metadata.get_value("postconditions")){
        read_postconditions(post->to_object_throw(std::string(source)), source, checkpoint);
    }
    if (const JsonValue* phase_val = metadata.get_value("phase")){
        const std::string& phase = phase_val->to_string_throw(std::string(source));
        try{
            checkpoint.phase = Pokemon::CHECKPOINT_TESTING_STATUS_NAMES().get_enum(phase);
        }catch (const ParseException&){
            throw ParseException(std::format("{}: unknown phase {}", source, phase));
        }
    }
}

//  The checkpoints' ids in the order the story plays them.
constexpr std::string_view STORY_ORDER[] = {
    "01.01", "01.02", "01.03", "01.04", "01.05", "01.06", "02.01", "02.02", "02.03", "03.01",
    "03.02", "03.03", "04.01", "04.02", "04.03", "04.04", "04.05", "04.06", "05.01", "05.02", "05.02b", "05.02c",
    "05.03", "05.04", "05.05", "05.06", "05.07", "05.08", "05.09", "05.10", "06.01", "06.02",
    "06.03", "06.04", "07.01", "07.02", "08.01", "08.02", "08.03", "09.01", "09.02", "09.03",
    "09.04", "10.01", "10.02", "10.03", "10.04", "10.05", "10.06", "11.01", "11.02", "11.03",
    "12.01", "12.02", "12.02b", "12.03",
    "13.01", "13.02", "13.03", "13.04", "14.01", "14.02", "14.02b",
    "14.03", "14.04", "14.04b", "15.01", "16.01", "16.02", "16.03", "16.03b", "16.04", "16.05", "16.05b", "16.06", "16.06b",
    "18.01", "18.02", "18.02b", "18.03",
    "17.02", "17.03", "17.04", "17.04b", "19.01a", "19.01", "19.01b", "19.02", "19.03", "19.04", "19.04b", "19.04c", "19.04d", "19.05",
    "20.01a", "20.01", "20.02", "20.03", "20.04", "20.05",
};

size_t story_position(std::string_view id){
    return static_cast<size_t>(std::find(std::begin(STORY_ORDER), std::end(STORY_ORDER), id) - std::begin(STORY_ORDER));
}

const std::vector<std::pair<std::string, std::string>>& checkpoint_files(){
    static const std::vector<std::pair<std::string, std::string>> FILES = []{
        std::vector<std::pair<std::string, std::string>> files;
        for (const auto& file : autostory_checkpoint_json()){
            if (file.first.ends_with(".plan")){
                files.push_back(file);
            }
        }
        return files;
    }();
    return FILES;
}

} // namespace


std::vector<StoryCheckpoint> frlg_rows_for_mode(const FrlgStoryData& data, bool developer_mode){
    std::vector<StoryCheckpoint> rows;
    for (const StoryCheckpoint& row : data.checkpoints){
        if (developer_mode || !row.dev_only){
            rows.push_back(row);
        }
    }
    return rows;
}

StoryCheckpoint read_story_checkpoint_file(const JsonValue& value, std::string_view source){
    std::vector<StoryStep> parsed_steps;
    Pokemon::StoryActions parsers = make_frlg_action_parsers(
        [&parsed_steps](StoryStep parsed) -> Pokemon::StoryStepRun{
            parsed_steps.push_back(std::move(parsed));
            return {};
        }
    );
    //  Parse and validate the S10/S11 envelope and every step through #648.
    const Pokemon::StoryCheckpoint core = Pokemon::read_story_checkpoint(value, "frlg", source, parsers);

    const JsonObject& root = value.to_object_throw(std::string(source));
    const JsonObject& cp_obj = root.get_object_throw("checkpoint", std::string(source));
    const JsonObject& execution = root.get_object_throw("execution", std::string(source));

    StoryCheckpoint checkpoint;
    checkpoint.id = core.id;
    checkpoint.title = core.title;
    if (parsed_steps.size() != core.steps.size()){
        throw ParseException(std::format("{}: parsed step count mismatch", source));
    }
    for (size_t i = 0; i < core.steps.size(); i++){
        StoryStep& step = parsed_steps[i];
        step.name = core.steps[i].name;
        step.why = core.steps[i].why;
        const bool spends = step.action == MART_ACTION
            || step.action == TEACH_ACTION
            || step.action == "register_bike";
        step.once_per_save = spends && step.skip_if.empty();
    }
    checkpoint.steps = std::move(parsed_steps);

    if (const JsonValue* start_val = cp_obj.get_value("start")){
        checkpoint.start = Pokemon::read_grid_pose(*start_val, source);
    }
    read_metadata(cp_obj, source, checkpoint);

    only_fields(execution, {
        "menu", "supplies", "lead", "protected_slots", "handoff", "gift",
        "live_start", "notes", "verify", "dev_only", "save", "reload_when_chained",
        "start_flags", "start_gates",
    }, source);
    checkpoint.menu = read_start_menu(execution.get_string_throw("menu", std::string(source)), source);
    checkpoint.supplies = read_supplies(execution, source);
    if (execution.get_value("handoff") != nullptr){
        checkpoint.handoff = read_handoff(execution.get_string_throw("handoff", std::string(source)), source);
    }
    if (execution.get_value("gift") != nullptr){
        checkpoint.gift = read_gift(execution.get_string_throw("gift", std::string(source)), source);
    }
    if (execution.get_value("live_start") != nullptr){
        checkpoint.live_start = execution.get_boolean_throw("live_start", std::string(source));
    }
    if (execution.get_value("dev_only") != nullptr){
        checkpoint.dev_only = execution.get_boolean_throw("dev_only", std::string(source));
    }
    if (execution.get_value("save") != nullptr){
        const std::string& save = execution.get_string_throw("save", std::string(source));
        if (save == "autosave"){
            checkpoint.autosave = true;
        }else if (save != "manual"){
            throw ParseException(std::format("{}: unknown save kind \"{}\"", source, save));
        }
    }
    if (execution.get_value("reload_when_chained") != nullptr){
        checkpoint.reload_when_chained = execution.get_boolean_throw("reload_when_chained", std::string(source));
    }
    if (const JsonValue* verify_val = execution.get_value("verify")){
        checkpoint.verify = read_verify(verify_val->to_object_throw(std::string(source)), source);
    }
    if (const JsonValue* flags_val = execution.get_value("start_flags")){
        for (const JsonValue& flag : flags_val->to_array_throw(std::string(source))){
            checkpoint.start_flags.insert(flag.to_string_throw(std::string(source)));
        }
    }
    if (const JsonValue* gates_val = execution.get_value("start_gates")){
        Pokemon::apply_gate_change(checkpoint.start_gates, read_gate_change(*gates_val, source));
    }

    const std::set<std::string> note_keys = read_note_keys(execution, source);
    for (const std::string& key : note_keys){
        bool found = key == "checkpoint";
        for (const StoryStep& step : checkpoint.steps){
            found |= step.name == key;
        }
        if (!found){
            throw ParseException(std::format("{}: notes for unknown step {}", source, key));
        }
    }
    return checkpoint;
}

FrlgStoryData load_frlg_story(std::span<const Pokemon::StoryDevFile> dev_files){
    const auto& embedded = checkpoint_files();
    std::map<std::string, std::string> by_name;
    for (const auto& [name, text] : embedded){
        const std::string key = name.ends_with(".json") ? std::string(name) : std::string(name) + ".json";
        by_name.emplace(key, text);
    }

    FrlgStoryData data;
    std::map<std::string, std::string> dev_by_name;
    for (const Pokemon::StoryDevFile& file : dev_files){
        if (!by_name.contains(file.name)){
            throw ParseException(std::format("{}: unknown checkpoint file \"{}\"", file.path, file.name));
        }
        if (!dev_by_name.emplace(file.name, file.path).second){
            throw ParseException(std::format("{}: duplicate dev checkpoint \"{}\"", file.path, file.name));
        }
        const StoryCheckpoint orig = read_story_checkpoint_file(parse_json(by_name.at(file.name)), file.name);
        const StoryCheckpoint override_cp = read_story_checkpoint_file(parse_json(file.text), file.path);
        if (override_cp.id != orig.id){
            throw ParseException(std::format(
                "{}: dev checkpoint changed id from {} to {}", file.path, orig.id, override_cp.id
            ));
        }
        by_name[file.name] = file.text;
        data.dev_files.emplace_back(file.name, file.path);
    }

    for (const auto& [name, text] : by_name){
        const auto dev_it = dev_by_name.find(name);
        const std::string& source = dev_it != dev_by_name.end() ? dev_it->second : name;
        StoryCheckpoint cp = read_story_checkpoint_file(parse_json(text), source);
        if (story_position(cp.id) >= std::size(STORY_ORDER)){
            throw ParseException(std::format(
                "{}: checkpoint {} is not listed in STORY_ORDER (PokemonFRLG_AutoStoryTable.cpp).", source, cp.id
            ));
        }
        data.checkpoints.push_back(std::move(cp));
    }

    std::sort(data.checkpoints.begin(), data.checkpoints.end(), [](const StoryCheckpoint& a, const StoryCheckpoint& b){
        return story_position(a.id) < story_position(b.id);
    });
    for (size_t c = 0; c < data.checkpoints.size(); c++){
        const StoryCheckpoint& row = data.checkpoints[c];
        if (!row.reload_when_chained){
            continue;
        }
        if (c == 0 || !data.checkpoints[c - 1].autosave){
            throw ParseException(std::format(
                "{}: reload_when_chained requires the previous checkpoint to end on autosave", row.id
            ));
        }
    }
    for (size_t c = 1; c < data.checkpoints.size(); c++){
        if (data.checkpoints[c].id == data.checkpoints[c - 1].id){
            throw ParseException(std::format(
                "Checkpoint {} needs its id once in STORY_ORDER (PokemonFRLG_AutoStoryTable.cpp).", data.checkpoints[c].id
            ));
        }
    }
    return data;
}

const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = load_frlg_story({}).checkpoints;
    return checkpoints;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
