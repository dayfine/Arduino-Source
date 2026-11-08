/*  FRLG Authored Checkpoint Export
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include <initializer_list>
#include <limits>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "PokemonFRLG_AutoStoryExport.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace{
//  The envelope constants the export writes and the reader requires.
constexpr std::string_view FORMAT = "pokesim-native-checkpoint-export/1";
constexpr std::string_view GAME = "frlg";
constexpr std::string_view STATUS = "native-table-export";
constexpr std::string_view STATE_BASIS = "native-table-bookkeeping";
const std::vector<std::string>& unbound_fields(){
    static const std::vector<std::string> UNBOUND{
        "runtime_configuration", "map_pins", "executor_bindings", "entry_evidence", "effective_save_retry_policy",
    };
    return UNBOUND;
}

const char* menu_name(StartMenu menu){
    switch (menu){
    case StartMenu::BEFORE_POKEMON: return "before_pokemon";
    case StartMenu::BEFORE_POKEDEX: return "before_pokedex";
    case StartMenu::FULL: return "full";
    }
    throw ParseException("Unsupported FRLG START menu.");
}
JsonArray strings(const std::vector<std::string>& values){
    JsonArray result;
    for (const std::string& value : values){
        result.push_back(value);
    }
    return result;
}
int64_t count(size_t value){
    if (value > (size_t)std::numeric_limits<int64_t>::max()){
        throw ParseException("FRLG checkpoint count exceeds JSON integer range.");
    }
    return (int64_t)value;
}
JsonObject stats_json(const Stats& stats){
    JsonObject result;
    result["hp"] = stats.hp;
    result["attack"] = stats.attack;
    result["defense"] = stats.defense;
    result["sp_attack"] = stats.sp_attack;
    result["sp_defense"] = stats.sp_defense;
    result["speed"] = stats.speed;
    return result;
}
JsonObject member_json(const PartyMember& member){
    JsonObject result;
    result["species"] = member.species;
    JsonArray moves;
    for (const KnownMove& move : member.moves){
        JsonObject entry;
        entry["move"] = move.move;
        entry["pp"] = move.pp;
        moves.push_back(std::move(entry));
    }
    result["moves"] = std::move(moves);
    result["moves_through_level"] = member.moves_through_level;
    result["moves_read"] = member.moves_read;
    result["level"] = member.level;
    result["stats"] = member.stats ? JsonValue(stats_json(*member.stats)) : JsonValue();
    return result;
}
JsonObject supplies_json(const Supplies& supplies){
    JsonObject result;
    result["potions"] = supplies.potions;
    result["antidotes"] = supplies.antidotes;
    result["parlyz_heals"] = supplies.parlyz_heals;
    result["pocket"] = strings(supplies.pocket);
    result["status"] = status_name(supplies.status);
    result["hurt"] = supplies.hurt;
    result["hp_red"] = supplies.hp_red;
    result["hp_fill"] = supplies.hp_fill;
    result["lead"] = member_json(supplies.lead);
    JsonArray protected_slots;
    for (size_t slot : supplies.protected_slots){
        protected_slots.push_back(count(slot));
    }
    result["protected_slots"] = std::move(protected_slots);
    result["foes"] = strings(supplies.foes);
    result["trainer"] = supplies.trainer;
    result["risk"] = battle_risk_name(supplies.risk);
    result["foe_out"] = supplies.foe_out ? JsonValue(count(*supplies.foe_out)) : JsonValue();
    result["wild_foes"] = strings(supplies.wild_foes);
    return result;
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
    expect_string(root, "format", FORMAT, source);
    expect_string(root, "game", GAME, source);
    expect_string(root, "status", STATUS, source);
    expect_string(root, "state_basis", STATE_BASIS, source);
    if (read_strings(root.get_value_throw("unbound"), source) != unbound_fields()){
        throw ParseException(std::format("{}: unbound is not the export's list", source));
    }
    if (root.get_string_throw("source_revision").empty()){
        throw ParseException(std::format("{}: source_revision names no revision", source));
    }
}
} // namespace
JsonValue story_checkpoint_export(const StoryCheckpoint& checkpoint){
    JsonObject result;
    result["format"] = std::string(FORMAT);
    result["game"] = std::string(GAME);
    result["status"] = std::string(STATUS);
    result["checkpoint"] = Pokemon::story_checkpoint_json(checkpoint);
    JsonObject native;
    native["menu"] = menu_name(checkpoint.menu);
    native["supplies"] = supplies_json(checkpoint.supplies);
    result["native_fields"] = std::move(native);
    result["state_basis"] = std::string(STATE_BASIS);
    result["unbound"] = strings(unbound_fields());
    return result;
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
        menu_name, native.get_string_throw("menu"), source);
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
