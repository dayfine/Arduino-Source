/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <map>
#include <tuple>
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

//  The trainer each of the story's battle steps fights, by the map the
//  step leaves the player on and the step's name. The rival's teams are
//  those against a player who took Squirtle. On a map with more than one
//  trainer of a kind, the object's tile and script (pret
//  data/maps/<map>/map.json, scripts.inc) say which. A row's `risk`, when
//  set, overrides its trainer's default_battle_risk(): tune it here.
struct StoryTrainer{
    std::string_view map;
    std::string_view battle;
    std::string_view trainer;
    std::optional<BattleRisk> risk = std::nullopt;
};
constexpr StoryTrainer STORY_TRAINERS[] = {
    {.map = "PalletTown_ProfessorOaksLab", .battle = "rival_battle", .trainer = "TRAINER_RIVAL_OAKS_LAB_BULBASAUR"},
    {.map = "PewterCity_Gym", .battle = "camper_liam", .trainer = "TRAINER_CAMPER_LIAM"},
    {.map = "PewterCity_Gym", .battle = "brock", .trainer = "TRAINER_LEADER_BROCK"},
    {.map = "Route3", .battle = "bug_catcher_colton", .trainer = "TRAINER_BUG_CATCHER_COLTON"},
    {.map = "Route3", .battle = "bug_catcher_greg", .trainer = "TRAINER_BUG_CATCHER_GREG"},
    {.map = "Route3", .battle = "bug_catcher_james", .trainer = "TRAINER_BUG_CATCHER_JAMES"},
    //  Grunt1 at (12, 20), MtMoon_B2F_EventScript_Grunt1.
    {.map = "MtMoon_B2F", .battle = "rocket_grunt", .trainer = "TRAINER_TEAM_ROCKET_GRUNT"},
    {.map = "MtMoon_B2F", .battle = "super_nerd_miguel", .trainer = "TRAINER_SUPER_NERD_MIGUEL"},
    {.map = "CeruleanCity", .battle = "rival_cerulean", .trainer = "TRAINER_RIVAL_CERULEAN_BULBASAUR"},
    {.map = "Route24", .battle = "bug_catcher_cale", .trainer = "TRAINER_BUG_CATCHER_CALE"},
    {.map = "Route24", .battle = "lass_ali", .trainer = "TRAINER_LASS_ALI"},
    {.map = "Route24", .battle = "youngster_timmy", .trainer = "TRAINER_YOUNGSTER_TIMMY"},
    {.map = "Route24", .battle = "lass_reli", .trainer = "TRAINER_LASS_RELI"},
    {.map = "Route24", .battle = "camper_ethan", .trainer = "TRAINER_CAMPER_ETHAN"},
    //  The Nugget Bridge Rocket, Route24_EventScript_Rocket.
    {.map = "Route24", .battle = "rocket_grunt", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_6"},
    {.map = "CeruleanCity_Gym", .battle = "misty", .trainer = "TRAINER_LEADER_MISTY"},
    //  Route 25's four the way can't walk around (Route25 scripts.inc):
    //  Franklin (11, 4), Kelsey (22, 8), Nob (27, 9) and Haley (42, 5).
    {.map = "Route25", .battle = "hiker_franklin", .trainer = "TRAINER_HIKER_FRANKLIN"},
    {.map = "Route25", .battle = "picnicker_kelsey", .trainer = "TRAINER_PICNICKER_KELSEY"},
    {.map = "Route25", .battle = "hiker_nob", .trainer = "TRAINER_HIKER_NOB"},
    {.map = "Route25", .battle = "lass_haley", .trainer = "TRAINER_LASS_HALEY"},
    //  The burgled house's Rocket (33, 6), CeruleanCity_EventScript_GruntTrigger.
    {.map = "CeruleanCity", .battle = "rocket_grunt", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_5"},
    //  Route 6's Jeff (13, 33), across the only way south.
    {.map = "Route6", .battle = "camper_jeff", .trainer = "TRAINER_CAMPER_JEFF"},
    {.map = "SSAnne_2F_Corridor", .battle = "rival_ss_anne", .trainer = "TRAINER_RIVAL_SS_ANNE_BULBASAUR"},
    //  Vermilion Gym's three, before the trash cans, and Lt. Surge.
    {.map = "VermilionCity_Gym", .battle = "sailor_dwayne", .trainer = "TRAINER_SAILOR_DWAYNE"},
    {.map = "VermilionCity_Gym", .battle = "engineer_baily", .trainer = "TRAINER_ENGINEER_BAILY"},
    {.map = "VermilionCity_Gym", .battle = "gentleman_tucker", .trainer = "TRAINER_GENTLEMAN_TUCKER"},
    {.map = "VermilionCity_Gym", .battle = "lt_surge", .trainer = "TRAINER_LEADER_LT_SURGE"},
    //  Route 9's three the way can't walk around (Route9 scripts.inc):
    //  Alicia (14, 10), Jeremy (20, 17) and Conner (49, 7).
    {.map = "Route9", .battle = "picnicker_alicia", .trainer = "TRAINER_PICNICKER_ALICIA"},
    {.map = "Route9", .battle = "hiker_jeremy", .trainer = "TRAINER_HIKER_JEREMY"},
    {.map = "Route9", .battle = "bug_catcher_conner", .trainer = "TRAINER_BUG_CATCHER_CONNER"},
    //  Rock Tunnel's nine the way can't walk around (RockTunnel_1F,
    //  RockTunnel_B1F scripts.inc).
    {.map = "RockTunnel_1F", .battle = "pokemaniac_ashton", .trainer = "TRAINER_POKEMANIAC_ASHTON"},
    {.map = "RockTunnel_B1F", .battle = "pokemaniac_winston", .trainer = "TRAINER_POKEMANIAC_WINSTON"},
    {.map = "RockTunnel_B1F", .battle = "picnicker_martha", .trainer = "TRAINER_PICNICKER_MARTHA"},
    {.map = "RockTunnel_B1F", .battle = "hiker_allen", .trainer = "TRAINER_HIKER_ALLEN"},
    {.map = "RockTunnel_1F", .battle = "hiker_lenny", .trainer = "TRAINER_HIKER_LENNY"},
    {.map = "RockTunnel_B1F", .battle = "picnicker_sofia", .trainer = "TRAINER_PICNICKER_SOFIA"},
    {.map = "RockTunnel_B1F", .battle = "hiker_dudley", .trainer = "TRAINER_HIKER_DUDLEY"},
    {.map = "RockTunnel_B1F", .battle = "pokemaniac_cooper", .trainer = "TRAINER_POKEMANIAC_COOPER"},
    {.map = "RockTunnel_1F", .battle = "picnicker_dana", .trainer = "TRAINER_PICNICKER_DANA"},
    {.map = "Route8", .battle = "gamer_rich", .trainer = "TRAINER_GAMER_RICH"},
    //  The Rocket Hideout, as PokeGuy's run fights it (the 09.01 and 09.02
    //  drafts): the Grunt at the Game Corner's poster (11, 2)
    //  (CeladonCity_GameCorner_EventScript_RocketGrunt); on B4F, Grunt1 at
    //  (4, 2), who drops the LIFT KEY, the door guards Grunt2 (16, 14) and
    //  Grunt3 (19, 14), and Giovanni (19, 4) (RocketHideout_B4F scripts.inc).
    {.map = "CeladonCity_GameCorner", .battle = "rocket_grunt", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_7"},
    {.map = "RocketHideout_B4F", .battle = "lift_key_grunt", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_18"},
    {.map = "RocketHideout_B4F", .battle = "left_guard", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_16"},
    {.map = "RocketHideout_B4F", .battle = "right_guard", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_17"},
    {.map = "RocketHideout_B4F", .battle = "giovanni", .trainer = "TRAINER_BOSS_GIOVANNI"},
    //  The Pokemon Tower (10.02, 10.03; pret PokemonTower_*F scripts.inc):
    //  the rival on 2F, the Channelers in the only ways up on 5F and 6F,
    //  and 7F's three Grunts.
    {.map = "PokemonTower_2F", .battle = "rival_pokemon_tower", .trainer = "TRAINER_RIVAL_POKEMON_TOWER_BULBASAUR"},
    {.map = "PokemonTower_5F", .battle = "channeler_ruth", .trainer = "TRAINER_CHANNELER_RUTH"},
    {.map = "PokemonTower_5F", .battle = "channeler_tammy", .trainer = "TRAINER_CHANNELER_TAMMY"},
    {.map = "PokemonTower_6F", .battle = "channeler_jennifer", .trainer = "TRAINER_CHANNELER_JENNIFER"},
    {.map = "PokemonTower_6F", .battle = "channeler_emilia", .trainer = "TRAINER_CHANNELER_EMILIA"},
    {.map = "PokemonTower_7F", .battle = "tower_grunt1", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_19"},
    {.map = "PokemonTower_7F", .battle = "tower_grunt2", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_20"},
    {.map = "PokemonTower_7F", .battle = "tower_grunt3", .trainer = "TRAINER_TEAM_ROCKET_GRUNT_21"},
};

//  The story's scripted wild battles, which no trainer fights: the battle
//  step's map and name, and the species (pret setwildbattle).
struct StoryWildBattle{
    std::string_view map;
    std::string_view battle;
    std::string_view species;
};
constexpr StoryWildBattle STORY_WILD_BATTLES[] = {
    //  PokemonTower_6F_EventScript_MarowakGhost: the Silph Scope shows it.
    {.map = "PokemonTower_6F", .battle = "marowak_ghost", .species = "marowak"},
};

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
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "BattleData/" + std::string(name) + ".json is not built into the program.");
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

//  [power, type, pp, accuracy, priority, effect, chance]
const Table<MoveData>& moves(){
    static const Table<MoveData> MOVES = []{
        Table<MoveData> moves;
        for (const auto& [name, value] : file_json("moves").to_object_throw()){
            const JsonArray& entry = value.to_array_throw();
            moves.emplace(name, MoveData{
                .power = byte_of(entry[0]), .type = type_of(entry[1]), .pp = byte_of(entry[2]),
                .accuracy = byte_of(entry[3]), .priority = static_cast<int8_t>(entry[4].to_integer_throw()),
                .effect = entry[5].to_string_throw(), .effect_chance = byte_of(entry[6]),
            });
        }
        return moves;
    }();
    return MOVES;
}

//  [type, type, HP, Attack, Defense, Sp. Atk, Sp. Def, Speed]
const Table<SpeciesData>& species(){
    static const Table<SpeciesData> SPECIES = []{
        Table<SpeciesData> species;
        for (const auto& [name, value] : file_json("species").to_object_throw()){
            const JsonArray& entry = value.to_array_throw();
            species.emplace(name, SpeciesData{
                .first = type_of(entry[0]), .second = type_of(entry[1]),
                .base = BaseStats{
                    .hp = byte_of(entry[2]), .attack = byte_of(entry[3]), .defense = byte_of(entry[4]),
                    .sp_attack = byte_of(entry[5]), .sp_defense = byte_of(entry[6]), .speed = byte_of(entry[7]),
                },
            });
        }
        return species;
    }();
    return SPECIES;
}

//  The type chart's rows in its order: (attack, defend, tenths).
const std::vector<std::tuple<PokemonType, PokemonType, size_t>>& chart_rows(){
    static const std::vector<std::tuple<PokemonType, PokemonType, size_t>> ROWS = []{
        std::vector<std::tuple<PokemonType, PokemonType, size_t>> rows;
        for (const JsonValue& value : file_json("type_chart").to_array_throw()){
            const JsonArray& entry = value.to_array_throw();
            rows.emplace_back(type_of(entry[0]), type_of(entry[1]), static_cast<size_t>(entry[2].to_integer_throw()));
        }
        return rows;
    }();
    return ROWS;
}

std::vector<std::string> strings_of(const JsonValue& value){
    std::vector<std::string> strings;
    for (const JsonValue& item : value.to_array_throw()){
        strings.push_back(item.to_string_throw());
    }
    return strings;
}

//  [species, level, iv, nature, [moves]]
TrainerMon trainer_mon(const JsonArray& entry){
    return TrainerMon{
        .species = entry[0].to_string_throw(), .level = byte_of(entry[1]),
        .iv = byte_of(entry[2]), .nature = byte_of(entry[3]), .moves = strings_of(entry[4]),
    };
}

//  {"ai": [...], "items": [...], "party": [...]}
TrainerData trainer_entry(const JsonObject& entry){
    TrainerData data{.ai = strings_of(entry.get_value_throw("ai")), .items = strings_of(entry.get_value_throw("items")), .party = {}};
    for (const JsonValue& value : entry.get_value_throw("party").to_array_throw()){
        data.party.push_back(trainer_mon(value.to_array_throw()));
    }
    return data;
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
        : "Battle: a wild " + species + "; attacks are chosen against it."
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

std::vector<size_t> type_multipliers(PokemonType attack, PokemonType first, PokemonType second){
    std::vector<size_t> multipliers;
    for (const auto& [row_attack, defend, tenths] : chart_rows()){
        if (row_attack != attack){
            continue;
        }
        if (defend == first){
            multipliers.push_back(tenths);
        }
        if (defend == second && first != second){
            multipliers.push_back(tenths);
        }
    }
    return multipliers;
}

bool special_type(PokemonType type){
    switch (type){
    case PokemonType::FIRE:
    case PokemonType::WATER:
    case PokemonType::GRASS:
    case PokemonType::ELECTRIC:
    case PokemonType::PSYCHIC:
    case PokemonType::ICE:
    case PokemonType::DRAGON:
    case PokemonType::DARK:
        return true;
    default:
        return false;
    }
}

std::vector<TrainerMon> trainer_mons(std::string_view trainer){
    std::optional<TrainerData> data = trainer_data(trainer);
    return data ? std::move(data->party) : std::vector<TrainerMon>{};
}

std::optional<TrainerData> trainer_data(std::string_view trainer){
    const JsonObject& trainers = file_json("trainers").to_object_throw();
    const auto it = trainers.find(std::string(trainer));
    if (it == trainers.end()){
        return std::nullopt;
    }
    return trainer_entry(it->second.to_object_throw());
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
    logger.log(read.empty() ? "Battle: the trainer's Pokemon wasn't read." : "Battle: the trainer's " + read + " is out.");
    return read;
}

std::vector<std::string> map_trainers(std::string_view map){
    const JsonObject& maps = file_json("map_trainers").to_object_throw();
    const auto it = maps.find(std::string(map));
    return it == maps.end() ? std::vector<std::string>{} : strings_of(it->second);
}

std::optional<std::string_view> story_trainer(std::string_view map, std::string_view battle){
    for (const StoryTrainer& entry : STORY_TRAINERS){
        if (entry.map == map && entry.battle == battle){
            return entry.trainer;
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> story_wild_battle(std::string_view map, std::string_view battle){
    for (const StoryWildBattle& entry : STORY_WILD_BATTLES){
        if (entry.map == map && entry.battle == battle){
            return entry.species;
        }
    }
    return std::nullopt;
}

const char* battle_risk_name(BattleRisk risk){
    return risk == BattleRisk::HIGH ? "high" : "low";
}

BattleRisk default_battle_risk(std::string_view trainer){
    const bool big = starts_with_any(trainer, {
        "TRAINER_RIVAL_", "TRAINER_CHAMPION_", "TRAINER_LEADER_", "TRAINER_ELITE_FOUR_", "TRAINER_BOSS_", "TRAINER_TEAM_ROCKET_ADMIN",
    });
    return big ? BattleRisk::HIGH : BattleRisk::LOW;
}

BattleRisk story_battle_risk(std::string_view map, std::string_view battle){
    for (const StoryTrainer& entry : STORY_TRAINERS){
        if (entry.map == map && entry.battle == battle){
            return entry.risk ? *entry.risk : default_battle_risk(entry.trainer);
        }
    }
    return BattleRisk::LOW;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
