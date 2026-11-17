/*  AutoStory League Foes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view FOES_FILE = "frlg-league-foes";
constexpr std::string_view FOES_FORMAT = "frlg-league-foes/1";

uint16_t stat_of(const JsonObject& stats, const std::string& key){
    return static_cast<uint16_t>(stats.get_integer_throw(key));
}

LeagueMoveData move_of(const JsonObject& object){
    LeagueMoveData move{
        .type = Pokemon::POKEMON_TYPE_SLUGS().get_enum(object.get_string_throw("type"), Pokemon::PokemonType::NONE),
        .power = static_cast<int>(object.get_integer_throw("power")),
        .priority = static_cast<int>(object.get_integer_default("priority", 0)),
        .physical = object.get_boolean_default("physical", false),
        .two_turn = object.get_boolean_default("two_turn", false),
    };
    //  The export writes 101 for a move that never misses.
    const int accuracy = static_cast<int>(object.get_integer_throw("accuracy"));
    if (accuracy <= 100){
        move.accuracy = accuracy;
    }
    if (const JsonValue* fixed = object.get_value("fixed_damage")){
        move.fixed_damage = static_cast<int>(fixed->to_integer_throw());
    }
    if (const JsonArray* hits = object.get_array("hits")){
        move.hits = std::pair<int, int>(static_cast<int>((*hits)[0].to_integer_throw()), static_cast<int>((*hits)[1].to_integer_throw()));
    }
    return move;
}

LeagueFoeData foe_of(const JsonObject& object){
    const JsonObject& stats = object.get_object_throw("stats");
    LeagueFoeData foe{
        .species = object.get_string_throw("species"),
        .level = static_cast<int>(object.get_integer_throw("level")),
        .ability = object.get_string_default("ability"),
        .stats = Stats{
            .hp = stat_of(stats, "hp"), .attack = stat_of(stats, "atk"), .defense = stat_of(stats, "def"),
            .sp_attack = stat_of(stats, "spa"), .sp_defense = stat_of(stats, "spd"), .speed = stat_of(stats, "spe"),
        },
    };
    if (const std::string* item = object.get_string("held_item")){
        foe.held_item = *item;
    }
    for (const JsonValue& move : object.get_array_throw("moves")){
        foe.moves.push_back(move.to_string_throw());
    }
    return foe;
}

LeagueTrainerData trainer_of(const JsonObject& object){
    LeagueTrainerData trainer;
    for (const auto& [item, count] : object.get_object_throw("items")){
        trainer.items[item] = static_cast<int>(count.to_integer_throw());
    }
    for (const JsonValue& foe : object.get_array_throw("party")){
        trainer.party.push_back(foe_of(foe.to_object_throw()));
    }
    return trainer;
}

} // namespace


LeagueFoesData parse_league_foes(std::string_view json){
    const JsonValue value = parse_json(std::string(json));
    const JsonObject& root = value.to_object_throw();
    if (root.get_string_default("format") != FOES_FORMAT){
        throw ParseException(std::format("{}.json: not a {} file.", FOES_FILE, FOES_FORMAT));
    }
    LeagueFoesData data;
    for (const auto& [name, move] : root.get_object_throw("moves")){
        data.moves[name] = move_of(move.to_object_throw());
    }
    for (const auto& [name, trainer] : root.get_object_throw("trainers")){
        data.trainers[name] = trainer_of(trainer.to_object_throw());
    }
    return data;
}

const LeagueFoesData& league_foes_data(){
    static const LeagueFoesData DATA = []{
        for (const auto& [file, text] : autostory_checkpoint_json()){
            if (file == FOES_FILE){
                return parse_league_foes(text);
            }
        }
        throw ParseException(std::format("StoryData/{}.json is not built into the program.", FOES_FILE));
    }();
    return DATA;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
