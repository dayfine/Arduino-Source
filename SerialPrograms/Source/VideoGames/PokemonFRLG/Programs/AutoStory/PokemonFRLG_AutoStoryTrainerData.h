/*  AutoStory Trainer Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Trainers' parties, items and AI flags as pret/pokefirered has them
 *  (BattleData/trainers.json), and the trainer's Pokemon out as read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrainerData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrainerData_H

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
#include "PokemonFRLG_AutoStoryMoveData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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
//  A trainer's party in order; empty for a trainer the data doesn't have.
std::vector<TrainerMon> trainer_mons(std::string_view trainer);

//  A trainer's AI flags, items and party; none for one the data doesn't have.
std::optional<TrainerData> trainer_data(std::string_view trainer);

//  The Pokemon a trainer's battle shows on `screen`, a battle menu: the
//  name its HP box shows (WildEncounterReader), among `party`'s species.
//  Empty if not read. Logs what it read.
std::string read_trainer_foe(Logger& logger, Language language, const ImageViewRGB32& screen, const std::vector<std::string>& party);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
