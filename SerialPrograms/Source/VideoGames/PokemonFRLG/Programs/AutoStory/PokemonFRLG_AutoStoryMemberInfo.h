/*  AutoStory Member Info
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A party member as its summary shows it, and the reader the story's
 *  steps take it from.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMemberInfo_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMemberInfo_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"
#include "PokemonFRLG_AutoStoryStats.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Total number of party slots.
constexpr size_t PARTY_SLOTS = 6;
//  Total number of move slots per Pokemon.
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

//  The console's reader: the member's summary, opened from `menu`'s
//  POKEMON, in a game in `language`. The steps that read members come
//  after the Pokedex, so `menu` is the full one or the Safari Zone's. A
//  member whose species, level or moves aren't all read is nothing.
MemberReader console_member_reader(ConsoleHandle& console, ProControllerContext& context, Language language, StartMenu menu);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
