/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <initializer_list>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "PokemonFRLG_AutoStoryBattleData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using JsonTable = std::map<std::string, JsonValue, std::less<>>;

const JsonValue& file_json(std::string_view name){
    static const JsonTable FILES = []{
        JsonTable files;
        for (const auto& [file, text] : autostory_battle_json()){
            files.emplace(file, parse_json(text));
        }
        return files;
    }();
    const auto it = FILES.find(name);
    if (it == FILES.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("BattleData/{}.json is not built into the program.", name));
    }
    return it->second;
}

PokemonType type_of(const JsonValue& value){
    return Pokemon::POKEMON_TYPE_SLUGS().get_enum(value.to_string_throw(), PokemonType::NONE);
}

uint8_t byte_of(const JsonValue& value){
    return static_cast<uint8_t>(std::clamp<int64_t>(value.to_integer_throw(), 0, 255));
}

template <typename Data>
using Table = std::map<std::string, Data, std::less<>>;

//  [type, damaging (0 or 1), pp]
const Table<MoveData>& moves(){
    static const Table<MoveData> MOVES = []{
        Table<MoveData> moves;
        for (const auto& [name, value] : file_json("moves").to_object_throw()){
            const JsonArray& entry = value.to_array_throw();
            moves.emplace(name, MoveData{
                .type = type_of(entry[0]), .damaging = entry[1].to_integer_throw() != 0, .pp = byte_of(entry[2]),
            });
        }
        return moves;
    }();
    return MOVES;
}

//  [type, type]
const Table<SpeciesData>& species(){
    static const Table<SpeciesData> SPECIES = []{
        Table<SpeciesData> species;
        for (const auto& [name, value] : file_json("species").to_object_throw()){
            const JsonArray& entry = value.to_array_throw();
            species.emplace(name, SpeciesData{.first = type_of(entry[0]), .second = type_of(entry[1])});
        }
        return species;
    }();
    return SPECIES;
}

bool starts_with_any(std::string_view text, std::initializer_list<std::string_view> prefixes){
    return std::ranges::any_of(prefixes, [text](std::string_view prefix){
        return text.starts_with(prefix);
    });
}

} // namespace


const MoveData* move_data(std::string_view move){
    const auto it = moves().find(move);
    return it == moves().end() ? nullptr : &it->second;
}

const SpeciesData* species_data(std::string_view name){
    const auto it = species().find(name);
    return it == species().end() ? nullptr : &it->second;
}

const std::set<std::string>& battle_species(){
    static const std::set<std::string> NAMES = []{
        std::set<std::string> names;
        for (const auto& item : species()){
            names.insert(item.first);
        }
        return names;
    }();
    return NAMES;
}

std::vector<std::string> wild_foes(std::string_view species){
    if (species_data(species) == nullptr){
        return {};
    }
    return {std::string(species)};
}

std::vector<std::string> read_wild_foes(Logger& logger, Language language, const ImageViewRGB32& screen){
    if (language == Language::None){
        return {};
    }
    const std::string species = WildEncounterReader().read_encounter(logger, language, screen, battle_species()).name;
    std::vector<std::string> foes = wild_foes(species);
    logger.log(foes.empty()
        ? "Battle: the wild Pokemon wasn't read; attacks in the plan's order."
        : std::format("Battle: a wild {}; attacks are chosen against it.", species)
    );
    return foes;
}

size_t type_effectiveness(PokemonType attack, PokemonType defend){
    static const std::map<std::pair<PokemonType, PokemonType>, size_t> CHART = []{
        std::map<std::pair<PokemonType, PokemonType>, size_t> chart;
        for (const JsonValue& value : file_json("type_chart").to_array_throw()){
            const JsonArray& entry = value.to_array_throw();
            chart[{type_of(entry[0]), type_of(entry[1])}] = static_cast<size_t>(entry[2].to_integer_throw());
        }
        return chart;
    }();
    const auto it = CHART.find({attack, defend});
    return it == CHART.end() ? 10 : it->second;
}

std::string read_trainer_foe(Logger& logger, Language language, const ImageViewRGB32& screen, const std::vector<std::string>& party){
    if (language == Language::None || party.empty()){
        return "";
    }
    const std::set<std::string> species(party.begin(), party.end());
    std::string read = WildEncounterReader().read_encounter(logger, language, screen, species).name;
    if (!species.contains(read)){
        read.clear();
    }
    logger.log(read.empty() ? "Battle: the trainer's Pokemon wasn't read." : std::format("Battle: the trainer's {} is out.", read));
    return read;
}

std::string_view battle_risk_name(BattleRisk risk){
    return risk == BattleRisk::HIGH ? "high" : "low";
}

BattleRisk default_battle_risk(std::string_view trainer){
    const bool big = starts_with_any(trainer, {
        "TRAINER_RIVAL_", "TRAINER_CHAMPION_", "TRAINER_LEADER_", "TRAINER_ELITE_FOUR_", "TRAINER_BOSS_", "TRAINER_TEAM_ROCKET_ADMIN",
    });
    return big ? BattleRisk::HIGH : BattleRisk::LOW;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
