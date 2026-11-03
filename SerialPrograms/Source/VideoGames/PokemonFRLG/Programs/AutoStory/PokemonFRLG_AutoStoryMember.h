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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonFRLG_AutoStoryDamage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


constexpr size_t PARTY_SLOTS = 6;
constexpr size_t MOVE_SLOTS = 4;

//  A party member as its summary shows it.
struct MemberInfo{
    //  The species' slug, e.g. "squirtle"; empty if not read.
    std::string species = {};
    //  1-100; nothing if not read.
    std::optional<unsigned> level = std::nullopt;
    //  Its moves' slugs in the summary's order (e.g. "water-gun"), one to
    //  four; empty if not read.
    std::vector<std::string> moves = {};
    //  POKEMON SKILLS (HP as its maximum); none if not read.
    std::optional<Stats> stats = std::nullopt;
};

//  Reads party slot `slot` (0-5), from the overworld back to the
//  overworld: its summary, read; nothing if the slot is empty or the
//  summary couldn't be read.
using MemberReader = std::function<std::optional<MemberInfo>(size_t slot)>;

//  The console's reader: the member's summary, opened from the START
//  menu's POKEMON, in a game in `language`. The steps that read members
//  come after the Pokedex, so the START menu is the full one. A member
//  whose species, level or moves aren't all read is nothing.
MemberReader console_member_reader(ConsoleHandle& console, ProControllerContext& context, Language language);


//  What a LEVEL step checks: party slot `slot` (0-5) is at least `level`.
struct LevelCheck{
    size_t slot = 0;
    unsigned level = 1;
};

//  The prefix of a GAME step's variant that names a LEVEL step.
inline constexpr std::string_view LEVEL_VARIANT_PREFIX = "level ";

//  `check` as a variant: "level slot=1 min=16", the slot counted from 1.
std::string level_variant(const LevelCheck& check);
//  A variant from level_variant(); nothing if it isn't one.
std::optional<LevelCheck> parse_level_variant(std::string_view variant);

//  The table's LEVEL step.
Pokemon::StoryStep level_step(std::string name, std::string why, const LevelCheck& check);

enum class LevelEnd{
    REACHED,
    BELOW,
    //  The member couldn't be read: nothing is known.
    UNREAD,
};
struct LevelResult{
    LevelEnd end = LevelEnd::UNREAD;
    std::optional<unsigned> level = std::nullopt;
};

//  Reads the member and compares.
LevelResult run_level_check(Logger& logger, const MemberReader& reader, const LevelCheck& check);



}
}
}
#endif
