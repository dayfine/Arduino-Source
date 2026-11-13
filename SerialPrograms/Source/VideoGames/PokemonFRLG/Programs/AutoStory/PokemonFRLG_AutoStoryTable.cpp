/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_AuthoredCheckpointJson.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
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

//  The wire names of the gifts Gift RNG's handoff takes.
PokemonFRLG_RngTarget read_gift(std::string_view name, std::string_view source){
    if (name == "magikarp"){
        return PokemonFRLG_RngTarget::magikarp;
    }
    if (name == "eevee"){
        return PokemonFRLG_RngTarget::eevee;
    }
    throw ParseException(std::format("{}: unknown gift {}", source, name));
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

//  Checks the metadata's "postconditions": what the postcondition check
//  after a run reads. "position" where the player ends, as a step's
//  target; "items" the bag's counts by item; "flags" the game's flags
//  that moved.
void check_postconditions(const JsonObject& postconditions, std::string_view source){
    only_fields(postconditions, {"position", "items", "flags"}, source);
    if (const JsonValue* position = postconditions.get_value("position")){
        const JsonObject& pose = position->to_object_throw(std::string(source));
        only_fields(pose, {"map", "tiles", "facing"}, source);
        pose.get_string_throw("map", std::string(source));
        pose.get_array_throw("tiles", std::string(source));
    }
    if (const JsonValue* items = postconditions.get_value("items")){
        for (const auto& [item, count] : items->to_object_throw(std::string(source))){
            if (count.to_integer_throw(std::string(source)) < 0){
                throw ParseException(std::format("{}: negative count of {}", source, item));
            }
        }
    }
    if (const JsonValue* flags = postconditions.get_value("flags")){
        for (const JsonValue& flag : flags->to_array_throw(std::string(source))){
            flag.to_string_throw(std::string(source));
        }
    }
}

//  The file without its "metadata", which the shared reader doesn't take
//  (yet): {"phase": testing, verification or reproduction}, how far the
//  checkpoint is trusted to save (the engine's save policy reads it), and
//  optionally "postconditions" (check_postconditions()).
JsonValue without_metadata(const JsonValue& value, std::string_view source){
    JsonObject stripped;
    for (const auto& [key, field] : value.to_object_throw(std::string(source))){
        if (key != "metadata"){
            stripped[key] = field.clone();
            continue;
        }
        const JsonObject& metadata = field.to_object_throw(std::string(source));
        for (const auto& [metadata_key, unused] : metadata){
            if (metadata_key != "phase" && metadata_key != "postconditions"){
                throw ParseException(std::format("{}: unsupported metadata field {}", source, metadata_key));
            }
        }
        if (const JsonValue* postconditions = metadata.get_value("postconditions")){
            check_postconditions(postconditions->to_object_throw(std::string(source)), source);
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
        only_fields(execution, {"menu", "supplies", "lead", "handoff", "gift", "live_start", "notes"}, where);
        checkpoint.menu = read_start_menu(execution.get_string_throw("menu", std::string(where)), where);
        checkpoint.supplies = read_supplies(execution, where);
        if (execution.get_value("handoff") != nullptr){
            checkpoint.handoff = read_handoff(execution.get_string_throw("handoff", std::string(where)), where);
        }
        if (execution.get_value("gift") != nullptr){
            checkpoint.gift = read_gift(execution.get_string_throw("gift", std::string(where)), where);
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
    //  A Mart spends money, a TEACH a TM, and the Bicycle's registration is
    //  saved with the game: once per save. A step with a skip_if is already
    //  conditional (the two together are refused).
    for (StoryStep& step : checkpoint.steps){
        const bool spends = step.action == MART_ACTION || step.action == TEACH_ACTION || step.action == REGISTER_BIKE_VARIANT;
        step.once_per_save = spends && step.skip_if.empty();
    }
    return checkpoint;
}

namespace{

//  The checkpoints' ids in the order the story plays them. Ids are labels,
//  not positions: a gym's rows may be numbered before rows that come first
//  in the story (the table is not sorted by id). Every file in StoryData/
//  is listed once; a new checkpoint is added at its place in the story.
constexpr std::string_view STORY_ORDER[] = {
    "01.01", "01.02", "01.03", "01.04", "01.05", "01.06", "02.01", "02.02", "02.03", "03.01",
    "03.02", "03.03", "04.01", "04.02", "04.03", "04.04", "04.05", "04.06", "05.01", "05.02",
    "05.03", "05.04", "05.05", "05.06", "05.07", "05.08", "05.09", "05.10", "06.01", "06.02",
    "06.03", "06.04", "07.01", "07.02", "08.01", "08.02", "08.03", "09.01", "09.02", "09.03",
    "09.04", "10.01", "10.02", "10.03", "10.04", "10.05", "10.06", "11.01", "11.02", "11.03",
    "12.01", "12.02", "12.03", "14.03", "14.04", "15.01", "16.01", "18.01", "18.02", "17.01", "13.01", "13.02", "13.03", "13.04", "14.01", "14.02", "14.02b",
    "17.02", "17.03", "17.04",
};

//  `checkpoints` in STORY_ORDER's order. Throws ParseException for a
//  checkpoint that is not listed there, or listed twice.
std::vector<StoryCheckpoint> in_story_order(std::vector<StoryCheckpoint> checkpoints){
    std::vector<StoryCheckpoint> ordered;
    for (std::string_view id : STORY_ORDER){
        for (StoryCheckpoint& checkpoint : checkpoints){
            if (checkpoint.id == id){
                ordered.push_back(std::move(checkpoint));
                checkpoint.id = "";
            }
        }
    }
    if (ordered.size() != checkpoints.size()){
        throw ParseException("Every checkpoint file needs its id once in STORY_ORDER (PokemonFRLG_AutoStoryTable.cpp).");
    }
    return ordered;
}

} // namespace

const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = []{
        std::vector<StoryCheckpoint> result;
        for (const auto& [name, text] : autostory_checkpoint_json()){
            result.push_back(read_story_checkpoint_file(parse_json(text), name));
        }
        return in_story_order(std::move(result));
    }();
    return checkpoints;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
