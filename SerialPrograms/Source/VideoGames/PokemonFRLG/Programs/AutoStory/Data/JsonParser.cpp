/*  FRLG Checkpoint JSON Parser
 *  From: https://github.com/PokemonAutomation/
 */

#include <format>
#include <initializer_list>
#include <limits>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "LegacyJsonFormat.h"
#include "JsonParser.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

void exact_fields(const JsonObject& value, std::initializer_list<std::string_view> names){
    if (value.size() != names.size()){
        throw ParseException("Authored checkpoint: missing or extra fields.");
    }
    for (std::string_view name : names){
        value.get_value_throw(std::string(name));
    }
}

//  Reading. Every field is required and no other is allowed, as in the
//  shared codec: the record says what the table said, not what it left out.
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

void expect_string(const JsonObject& root, const std::string& key, std::string_view expected, std::string_view source){
    const std::string& actual = root.get_string_throw(key, std::string(source));
    if (actual != expected){
        throw ParseException(std::format("{}: {} is {}, not {}", source, key, actual, expected));
    }
}

//  The value of `values` that `name` names `label`.
template <typename Enum>
Enum read_named(std::initializer_list<Enum> values, const char* (*name)(Enum), const std::string& label, std::string_view source){
    for (Enum value : values){
        if (label == name(value)){
            return value;
        }
    }
    throw ParseException(std::format("{}: unsupported value {}", source, label));
}

//  An integer within [`low`, `high`], as the native field's type holds it.
int64_t bounded(const JsonValue& value, int64_t low, int64_t high, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < low || number > high){
        throw ParseException(std::format("{}: {} is out of range", source, number));
    }
    return number;
}

int read_int(const JsonValue& value, std::string_view source){
    return (int)bounded(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), source);
}

uint8_t read_byte(const JsonValue& value, std::string_view source){
    return (uint8_t)bounded(value, 0, std::numeric_limits<uint8_t>::max(), source);
}

uint16_t read_word(const JsonValue& value, std::string_view source){
    return (uint16_t)bounded(value, 0, std::numeric_limits<uint16_t>::max(), source);
}

size_t read_count(const JsonValue& value, std::string_view source){
    return (size_t)bounded(value, 0, std::numeric_limits<int64_t>::max(), source);
}

std::vector<std::string> read_strings(const JsonValue& value, std::string_view source){
    std::vector<std::string> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(item.to_string_throw(std::string(source)));
    }
    return result;
}

std::optional<Stats> read_stats(const JsonValue& value, std::string_view source){
    if (value.is_null()){
        return std::nullopt;
    }
    const JsonObject& root = fields(value, {"hp", "attack", "defense", "sp_attack", "sp_defense", "speed"}, source);
    return Stats{
        .hp = read_word(root.get_value_throw("hp"), source),
        .attack = read_word(root.get_value_throw("attack"), source),
        .defense = read_word(root.get_value_throw("defense"), source),
        .sp_attack = read_word(root.get_value_throw("sp_attack"), source),
        .sp_defense = read_word(root.get_value_throw("sp_defense"), source),
        .speed = read_word(root.get_value_throw("speed"), source),
    };
}

std::vector<KnownMove> read_moves(const JsonValue& value, std::string_view source){
    std::vector<KnownMove> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = fields(item, {"move", "pp"}, source);
        result.push_back({.move = row.get_string_throw("move"), .pp = read_byte(row.get_value_throw("pp"), source)});
    }
    return result;
}

PartyMember read_member(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"species", "moves", "moves_through_level", "moves_read", "level", "stats"}, source);
    return PartyMember{
        .species = root.get_string_throw("species"),
        .moves = read_moves(root.get_value_throw("moves"), source),
        .moves_through_level = read_byte(root.get_value_throw("moves_through_level"), source),
        .moves_read = root.get_boolean_throw("moves_read"),
        .level = read_byte(root.get_value_throw("level"), source),
        .stats = read_stats(root.get_value_throw("stats"), source),
    };
}

std::vector<size_t> read_slots(const JsonValue& value, std::string_view source){
    std::vector<size_t> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(read_count(item, source));
    }
    return result;
}

std::optional<size_t> read_foe_out(const JsonValue& value, std::string_view source){
    if (value.is_null()){
        return std::nullopt;
    }
    return read_count(value, source);
}

Supplies read_supplies(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"potions", "antidotes", "parlyz_heals", "pocket", "status", "hurt", "hp_red",
        "hp_fill", "lead", "protected_slots", "foes", "trainer", "risk", "foe_out", "wild_foes"}, source);
    Supplies result;
    result.potions = read_int(root.get_value_throw("potions"), source);
    result.antidotes = read_int(root.get_value_throw("antidotes"), source);
    result.parlyz_heals = read_int(root.get_value_throw("parlyz_heals"), source);
    result.pocket = read_strings(root.get_value_throw("pocket"), source);
    result.status = read_named({StatusCondition::NONE, StatusCondition::POISON, StatusCondition::PARALYSIS, StatusCondition::OTHER},
        status_name, root.get_string_throw("status"), source);
    result.hurt = root.get_boolean_throw("hurt");
    result.hp_red = root.get_boolean_throw("hp_red");
    result.hp_fill = root.get_double_throw("hp_fill");
    result.lead = read_member(root.get_value_throw("lead"), source);
    result.protected_slots = read_slots(root.get_value_throw("protected_slots"), source);
    result.foes = read_strings(root.get_value_throw("foes"), source);
    result.trainer = root.get_string_throw("trainer");
    result.risk = read_named({BattleRisk::LOW, BattleRisk::HIGH}, battle_risk_name, root.get_string_throw("risk"), source);
    result.foe_out = read_foe_out(root.get_value_throw("foe_out"), source);
    result.wild_foes = read_strings(root.get_value_throw("wild_foes"), source);
    return result;
}

void check_envelope(const JsonObject& root, std::string_view source){
    expect_string(root, "format", StoryJsonInternal::FORMAT, source);
    expect_string(root, "game", StoryJsonInternal::GAME, source);
    expect_string(root, "status", StoryJsonInternal::STATUS, source);
    expect_string(root, "state_basis", StoryJsonInternal::STATE_BASIS, source);
    if (read_strings(root.get_value_throw("unbound"), source) != StoryJsonInternal::unbound_fields()){
        throw ParseException(std::format("{}: unbound is not the export's list", source));
    }
    if (root.get_string_throw("source_revision").empty()){
        throw ParseException(std::format("{}: source_revision names no revision", source));
    }
}

} // namespace

StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    const JsonObject& root = value.to_object_throw(std::string(source));
    exact_fields(root, {"format", "game", "checkpoint", "execution"});
    if (root.get_string_throw("format") != "pokesim-authored-checkpoint/1" || root.get_string_throw("game") != "frlg"){
        throw ParseException("Unsupported authored checkpoint format/game.");
    }
    const JsonObject& execution = root.get_object_throw("execution");
    exact_fields(execution, {"menu"});
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), source);
    const std::string& menu = execution.get_string_throw("menu");
    if (menu == "before_pokemon"){
        result.menu = StartMenu::BEFORE_POKEMON;
    }else if (menu == "before_pokedex"){
        result.menu = StartMenu::BEFORE_POKEDEX;
    }else if (menu == "full"){
        result.menu = StartMenu::FULL;
    }else{
        throw ParseException("Unsupported authored checkpoint menu.");
    }
    return result;
}

StoryCheckpoint load_authored_checkpoint(std::string_view file){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == file){
            return read_authored_checkpoint(parse_json(text), file);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Authored checkpoint is not embedded: " + std::string(file));
}

StoryCheckpoint read_story_checkpoint_export(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value,
        {"format", "game", "status", "checkpoint", "native_fields", "state_basis", "unbound", "source_revision"}, source);
    check_envelope(root, source);
    StoryCheckpoint result;
    Pokemon::StoryCheckpoint& common = result;
    common = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), source);
    const JsonObject& native = fields(root.get_value_throw("native_fields"), {"menu", "supplies"}, source);
    result.menu = read_named({StartMenu::BEFORE_POKEMON, StartMenu::BEFORE_POKEDEX, StartMenu::FULL},
        StoryJsonInternal::menu_name, native.get_string_throw("menu"), source);
    result.supplies = read_supplies(native.get_value_throw("supplies"), source);
    return result;
}

StoryCheckpoint load_story_checkpoint(std::string_view file){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == file){
            return read_story_checkpoint_export(parse_json(text), std::format("StoryData/{}.json", file));
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("StoryData/{}.json is not built into the program.", file));
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
