/*  AutoStory Summary
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading a party member's summary from the overworld: its species,
 *  level and moves, for the battles' move plan and the steps that check a
 *  level.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySummary_H
#define PokemonAutomation_PokemonFRLG_AutoStorySummary_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryStats.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A party member as its summary shows it.
struct SummaryRead{
    //  The species' slug, empty if not read.
    std::string species = {};
    std::optional<unsigned> level = std::nullopt;
    //  KNOWN MOVES, from the top, up to the first name not read.
    std::vector<SummaryMove> moves = {};
    //  POKEMON SKILLS (HP as its maximum), none unless all six were read.
    std::optional<Stats> stats = std::nullopt;
};

//  From the overworld: the party menu, party slot `slot`'s SUMMARY, its
//  first page (species, level), POKEMON SKILLS (stats) and KNOWN MOVES,
//  then back to the overworld.
//  Each screen is waited for before the next press: a press sent before a
//  menu is drawn goes to what was there before (console B, 2026-11-02, the
//  PC's menus). Nothing if a screen didn't show, or before the starter
//  (`menu` has no POKEMON).
std::optional<SummaryRead> read_party_summary(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language, size_t slot
);


//  The console's screens for a PartyRoster scan, from the overworld with
//  START menu `menu`: the party opened once, the summaries paged with DOWN,
//  the member's menu left open (see PokemonFRLG_AutoStoryPartyRoster.h).
//  The summary's member switch (DOWN) is waited out with a fixed settle,
//  not measured on the console.
std::unique_ptr<PartyScreens> console_party_screens(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language
);

//  Where a TEACH finds a species' slot from the full START menu: from
//  `party` if it knows it; else a scan, with the party closed after.
SpeciesFinder console_species_finder(
    ConsoleHandle& console, ProControllerContext& context, Language language, PartyCache& party
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
