/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's data the story's attacks are chosen from, generated from
 *  pret/pokefirered by devtools/maps/frlg_battle_data.py into BattleData/
 *  and built into the program: the type chart, every species' types and
 *  base stats, every move's power, type, PP, accuracy, priority and
 *  effect, and every trainer's AI flags, items and party. Also which
 *  trainer each of the story's battle steps fights, and how carefully
 *  (BattleRisk).
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


//  Type enum imported from Pokemon navigation.
using Pokemon::PokemonType;

//  Data attributes of a battle move.
struct MoveData{
    //  Moves whose damage doesn't come from their power (Flail, Dragon
    //  Rage) have 1; moves that do no damage 0.
    uint8_t power = 0;
    //  NONE for Curse's "???".
    PokemonType type = PokemonType::NONE;
    uint8_t pp = 0;
    //  In percent; 0 never misses.
    uint8_t accuracy = 0;
    int8_t priority = 0;
    //  pret's EFFECT_X as a slug: "hit", "flinch-hit", "sleep",
    //  "attack-down", "dragon-rage".
    std::string effect = {};
    //  The secondary effect's chance in percent (Bite's flinch: 30).
    uint8_t effect_chance = 0;
};

//  The base stats, in the game's order.
struct BaseStats{
    uint8_t hp = 0;
    uint8_t attack = 0;
    uint8_t defense = 0;
    uint8_t sp_attack = 0;
    uint8_t sp_defense = 0;
    uint8_t speed = 0;
};

//  Species battle base attributes and typing.
struct SpeciesData{
    //  The same type twice for a species with one.
    PokemonType first = PokemonType::NONE;
    PokemonType second = PokemonType::NONE;
    BaseStats base = {};
};

//  A trainer party Pokemon entry.
struct TrainerMon{
    std::string species;
    uint8_t level = 0;
    //  pret's iv, 0 to 255: every IV is iv * 31 / 255 (CreateNPCTrainerParty).
    uint8_t iv = 0;
    //  The nature's index (0 Hardy, 1 Lonely, ... 24 Quirky): the one the
    //  game's personality gives it (see frlg_battle_data.py).
    uint8_t nature = 0;
    //  The moves the party gives it, else the level-up moves the game gives.
    std::vector<std::string> moves;
};

//  Trainer AI flags and team definition.
struct TrainerData{
    //  pret's AI flags as slugs: "check-bad-move", "try-to-faint", ...
    std::vector<std::string> ai = {};
    //  The items the trainer can use, in order, each once ("super-potion").
    std::vector<std::string> items = {};
    std::vector<TrainerMon> party = {};
};

//  None for a move or species the data doesn't have.
const MoveData* move_data(std::string_view move);
//  Base stats and typing for species; nullptr if unknown.
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

//  The multipliers (in tenths) a move of type `attack` meets on a Pokemon
//  of types `first` and `second`, in the type chart's order, as the game
//  applies them one after another (pret Cmd_typecalc); none for 1x.
std::vector<size_t> type_multipliers(PokemonType attack, PokemonType first, PokemonType second);

//  Gen 3 splits physical and special by type: Fire, Water, Grass,
//  Electric, Psychic, Ice, Dragon and Dark are special.
bool special_type(PokemonType type);

//  A trainer's party in order; empty for a trainer the data doesn't have.
std::vector<TrainerMon> trainer_mons(std::string_view trainer);

//  A trainer's AI flags, items and party; none for one the data doesn't have.
std::optional<TrainerData> trainer_data(std::string_view trainer);

//  The Pokemon a trainer's battle shows on `screen`, a battle menu: the
//  name its HP box shows (WildEncounterReader), among `party`'s species.
//  Empty if not read. Logs what it read.
std::string read_trainer_foe(Logger& logger, Language language, const ImageViewRGB32& screen, const std::vector<std::string>& party);

//  The trainer the story's battle step `battle` fights on `map` (the map
//  it leaves the player on). None for a battle the table doesn't name.
std::optional<std::string_view> story_trainer(std::string_view map, std::string_view battle);

//  How carefully a battle step is fought.
enum class BattleRisk{
    //  The move with the best expected damage against the foe out.
    LOW,
    //  The battles the simulation's plan is for (the rival, gym leaders,
    //  bosses); fought as LOW until the checkpoint carries a plan.
    HIGH,
};
//  String representation of battle risk level.
std::string_view battle_risk_name(BattleRisk risk);

//  A trainer's risk by its kind: HIGH for the rival, gym leaders, bosses
//  (Giovanni, the Rocket admins), the Elite Four and the Champion; LOW
//  for the rest.
BattleRisk default_battle_risk(std::string_view trainer);

//  The risk of the story's battle step `battle` on `map`: its table row's,
//  else its trainer's default. LOW for a battle the table doesn't name.
BattleRisk story_battle_risk(std::string_view map, std::string_view battle);


//  (name, JSON text) of each file in BattleData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_battle_json();


//  Registers AutoStory battle data unit tests.
void add_tests_AutoStoryBattleData(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
