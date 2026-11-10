/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_AuthoredCheckpointJson.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The wire names of the START menus.
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

//  The wire names of the handoffs.
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

//  `object`'s fields, each of which must be one of `keys`.
void only_fields(const JsonObject& object, std::initializer_list<std::string_view> keys, std::string_view source){
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("{}: unsupported execution field {}", source, key));
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
    return (int)count;
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
    return supplies;
}

//  The keys of the notes: "checkpoint" or a step's id, each a list of lines.
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

//  The file without its "metadata", which the shared reader doesn't take
//  (yet): {"phase": testing, verification or reproduction}, how far the
//  checkpoint is trusted to save (the engine's save policy reads it).
JsonValue without_metadata(const JsonValue& value, std::string_view source){
    JsonObject stripped;
    for (const auto& [key, field] : value.to_object_throw(std::string(source))){
        if (key != "metadata"){
            stripped[key] = field.clone();
            continue;
        }
        const JsonObject& metadata = field.to_object_throw(std::string(source));
        for (const auto& [metadata_key, unused] : metadata){
            if (metadata_key != "phase"){
                throw ParseException(std::format("{}: unsupported metadata field {}", source, metadata_key));
            }
        }
        const std::string& phase = metadata.get_string_throw("phase", std::string(source));
        if (phase != "testing" && phase != "verification" && phase != "reproduction"){
            throw ParseException(std::format("{}: unknown phase {}", source, phase));
        }
    }
    return JsonValue(std::move(stripped));
}

} // namespace


StoryCheckpoint read_story_checkpoint_file(const JsonValue& value, std::string_view source){
    StoryCheckpoint checkpoint;
    std::set<std::string> note_keys;
    const Pokemon::ExecutionReader read_execution = [&](const JsonObject& execution, std::string_view where){
        only_fields(execution, {"menu", "supplies", "lead", "handoff", "live_start", "notes"}, where);
        checkpoint.menu = read_start_menu(execution.get_string_throw("menu", std::string(where)), where);
        checkpoint.supplies = read_supplies(execution, where);
        if (execution.get_value("handoff") != nullptr){
            checkpoint.handoff = read_handoff(execution.get_string_throw("handoff", std::string(where)), where);
        }
        if (execution.get_value("live_start") != nullptr){
            checkpoint.live_start = execution.get_boolean_throw("live_start", std::string(where));
        }
        note_keys = read_note_keys(execution, where);
    };
    static_cast<Pokemon::StoryCheckpoint&>(checkpoint) = Pokemon::read_authored_checkpoint(
        without_metadata(value, source), "frlg", source, read_execution, story_action_specs()
    );
    for (const std::string& key : note_keys){
        bool found = key == "checkpoint";
        for (const StoryStep& step : checkpoint.steps){
            found |= step.name == key;
        }
        if (!found){
            throw ParseException(std::format("{}: notes for unknown step {}", source, key));
        }
    }
    //  A Mart spends money and a TEACH a TM: once per save.
    for (StoryStep& step : checkpoint.steps){
        step.once_per_save = step.action == MART_ACTION || step.action == TEACH_ACTION;
    }
    return checkpoint;
}

const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = []{
        std::vector<StoryCheckpoint> result;
        for (const auto& [name, text] : autostory_checkpoint_json()){
            result.push_back(read_story_checkpoint_file(parse_json(text), name));
        }
        return result;
    }();
    return checkpoints;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
