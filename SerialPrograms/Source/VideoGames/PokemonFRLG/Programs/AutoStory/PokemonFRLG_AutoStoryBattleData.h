/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's data the story's attacks are chosen from, generated from
 *  pret/pokefirered by devtools/maps/frlg_battle_data.py into BattleData/
 *  and built into the program: the type chart, every species' types and
 *  base Attack and Sp. Atk, every move's power, type and PP,
 *  every trainer's party, and the trainers each map battles. Also which
 *  trainer each of the story's battle steps fights.
 *
 *  Names are the program's slugs ("water-gun", "mr-mime"); trainers are
 *  pret's constants ("TRAINER_SUPER_NERD_MIGUEL").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "Pokemon/Pokemon_Types.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::PokemonType;

struct MoveData{
    //  Moves whose damage doesn't come from their power (Flail, Dragon
    //  Rage) have 1; moves that do no damage 0.
    uint8_t power = 0;
    //  NONE for Curse's "???".
    PokemonType type = PokemonType::NONE;
    uint8_t pp = 0;
};

struct SpeciesData{
    //  The same type twice for a species with one.
    PokemonType first = PokemonType::NONE;
    PokemonType second = PokemonType::NONE;
    uint8_t attack = 0;
    uint8_t sp_attack = 0;
};

struct TrainerMon{
    std::string species;
    uint8_t level = 0;
    //  The moves the party gives it; empty for its level-up moves.
    std::vector<std::string> moves;
};

//  None for a move or species the data doesn't have.
const MoveData* move_data(std::string_view move);
const SpeciesData* species_data(std::string_view species);

//  Every species, for reading a wild Pokemon's name.
const std::set<std::string>& battle_species();

//  The foes of a wild battle whose Pokemon was read as `species`: it, or
//  none when it wasn't read ("") or the data doesn't have it.
std::vector<std::string> wild_foes(std::string_view species);

//  The foes of the wild battle on `screen`, a battle menu: the Pokemon its
//  HP box names (WildEncounterReader, among battle_species()), as
//  wild_foes() takes it. Logs what it read.
std::vector<std::string> read_wild_foes(Logger& logger, Language language, const ImageViewRGB32& screen);

//  How well `attack` hits `defend`, in tenths: 20 is 2x, 5 is 0.5x.
size_t type_effectiveness(PokemonType attack, PokemonType defend);

//  Gen 3 splits physical and special by type: Fire, Water, Grass,
//  Electric, Psychic, Ice, Dragon and Dark are special.
bool special_type(PokemonType type);

//  A trainer's party in order; empty for a trainer the data doesn't have.
std::vector<TrainerMon> trainer_mons(std::string_view trainer);

//  The trainers `map`'s scripts and objects battle.
std::vector<std::string> map_trainers(std::string_view map);

//  The trainer the story's battle step `battle` fights on `map` (the map
//  it leaves the player on). None for a battle the table doesn't name.
std::optional<std::string_view> story_trainer(std::string_view map, std::string_view battle);


//  (name, JSON text) of each file in BattleData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_battle_json();


void add_tests_AutoStoryBattleData(UnitTestDatabase& database);



}
}
}
#endif
