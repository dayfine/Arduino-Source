/*  AutoStory Member
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story reads about one party member: its species, level, stats
 *  and moves, as the summary screen shows them (page 1: level; page 2:
 *  stats; page 3: the four moves). The story's steps take them from a MemberReader, so their
 *  decisions run in tests against a fake party; on the console they read
 *  the member's summary (read_party_summary(), PokemonFRLG_AutoStorySummary.h).
 *  A member that can't be read stops every step that needs its level or
 *  moves instead of guessing.
 *
 *  Also the LEVEL step: a check, for the table, that a member has reached
 *  a level, before a fight that needs it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMember_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMember_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonFRLG_AutoStoryStats.h"
#include "PokemonFRLG_AutoStoryMemberInfo.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a LEVEL step checks: party slot `slot` (0-5) is at least `level`.
struct LevelCheck{
    size_t slot = 0;
    unsigned level = 1;
};

//  The GAME action of a LEVEL step: slot INTEGER (required, from 1), min
//  INTEGER (required, 1-100).
inline const std::string LEVEL_ACTION = "level";
//  `check` as its action's args.
Pokemon::StoryActionArgs level_args(const LevelCheck& check);
//  The check a LEVEL_ACTION step's args give; nothing if one is out of range.
std::optional<LevelCheck> level_check_of(const Pokemon::StoryActionArgs& args);

//  The table's LEVEL step.
Pokemon::StoryStep level_step(std::string name, std::string why, const LevelCheck& check);

//  Outcome of checking a party member level.
enum class LevelEnd{
    REACHED,
    BELOW,
    //  The member couldn't be read: nothing is known.
    UNREAD,
};
//  The result of a party member level check.
struct LevelResult{
    LevelEnd end = LevelEnd::UNREAD;
    std::optional<unsigned> level = std::nullopt;
};

//  Reads the member and compares.
LevelResult run_level_check(Logger& logger, const MemberReader& reader, const LevelCheck& check);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
