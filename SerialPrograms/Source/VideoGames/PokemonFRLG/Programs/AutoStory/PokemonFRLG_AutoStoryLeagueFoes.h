/*  AutoStory League Foes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League trainers' parties as PokemonSimulation's League sim fights
 *  them, built into the program from StoryData/frlg-league-foes.json
 *  (format "frlg-league-foes/1"): each slot's species, level, ability,
 *  raw stats, held item and moves, each trainer's items, and the moves'
 *  data the planned policy's damage needs. The file is exported from the
 *  sim's data/runs/pokeguy-leafgreen-15934/battle-reference.json with the
 *  stats its Showdown Gen 3 engine gives (the file's "source"); it is not
 *  edited by hand.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueFoes_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueFoes_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/Pokemon_Types.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryStats.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A move as the League sim's damage reads it.
struct LeagueMoveData{
    Pokemon::PokemonType type = Pokemon::PokemonType::NONE;
    int power = 0;
    //  Percent; none for a move that never misses.
    std::optional<int> accuracy = std::nullopt;
    int priority = 0;
    //  Gen 3's split by type.
    bool physical = false;
    bool two_turn = false;
    //  Damage that doesn't depend on stats (Dragon Rage: 40).
    std::optional<int> fixed_damage = std::nullopt;
    //  Hits per use, fewest and most (Double Slap: 2 to 5).
    std::optional<std::pair<int, int>> hits = std::nullopt;
};

//  One Pokemon of a League trainer's party.
struct LeagueFoeData{
    std::string species = {};
    int level = 1;
    //  Showdown's ability id ("waterabsorb", "thickfat").
    std::string ability = {};
    Stats stats = {};
    std::optional<std::string> held_item = std::nullopt;
    std::vector<std::string> moves = {};
};

//  A League trainer: its items and its party, in slot order.
struct LeagueTrainerData{
    //  Its items by slug ("full-restore": 2).
    SlugCounts items = {};
    std::vector<LeagueFoeData> party = {};
};

//  The whole file: the moves' data and every League trainer.
struct LeagueFoesData{
    std::map<std::string, LeagueMoveData, std::less<>> moves = {};
    //  By trainer ("TRAINER_ELITE_FOUR_LORELEI").
    std::map<std::string, LeagueTrainerData, std::less<>> trainers = {};
};

//  The data in `json`; throws ParseException on another format.
LeagueFoesData parse_league_foes(std::string_view json);

//  The data built into the program (StoryData/frlg-league-foes.json).
const LeagueFoesData& league_foes_data();



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
