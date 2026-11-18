/*  AutoStory Field Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The HMs the story uses away from what the player faces: Fly to a town,
 *  and Surf onto the water. Both are GAME steps named by their variant.
 *  The legs and routes after a Surf start on the water and are walked
 *  surfing (fixed_route(), plan_overworld_route()), until a step onto
 *  land gets the player off.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFieldMoves_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFieldMoves_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryMemberInfo.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that flies to a town, outdoors: START, POKEMON, the Fly
//  user (`slot`, from 1; or, with `species`, the first member whose menu
//  lists FLY, FLY's row read there), FLY on its menu (`menu_row`, from 1: SUMMARY,
//  then its field moves in move order, so 2 when Fly is its only one;
//  pokefirered SetPartyMonFieldSelectionActions()), and `town` on the fly
//  map (kanto_fly_towns()). It ends on the town's landing tile. With
//  `safari`, START opens the Safari Zone's menu (StartMenu::SAFARI).
inline const std::string FLY_VARIANT = "fly";
//  The GAME step that surfs onto the water `direction` of the player, on
//  ground beside it: A facing it, YES to "Would you like to SURF?", and
//  the jump onto it. It ends on that water tile.
inline const std::string SURF_VARIANT = "surf";

//  A fly step's choices, counted from 0.
struct FlyPlan{
    size_t slot = 0;
    //  The Fly user's species ("pidgey"), when the step names it instead
    //  of a slot: party_slot() finds its slot in the party.
    std::string species = {};
    size_t menu_row = 1;
    //  In kanto_fly_towns().
    size_t town = 0;
    //  The START menu the party opens from: the Safari Zone's when the
    //  step's "safari" arg says so (step_start_menu()).
    StartMenu start_menu = StartMenu::FULL;

    //  The party slot to fly with: `slot`, or the first member `reader`
    //  reads as `species`; nothing if none does.
    std::optional<size_t> party_slot(const MemberReader& reader) const;
};

//  The fly step's choices, or nothing if `step` isn't one or names no town
//  Fly goes to.
std::optional<FlyPlan> parse_fly_step(const Pokemon::StoryStep& step);
//  The way a surf step faces the water, or nothing if it isn't one.
std::optional<Pokemon::Direction> parse_surf_step(const Pokemon::StoryStep& step);
//  Whether `step` is one of this file's steps.
bool is_field_move_step(const Pokemon::StoryStep& step);
//  This file's GAME actions and their args (field_action_specs()).
std::vector<Pokemon::StoryActionSpec> field_move_specs();

//  On the console, a step of this file's; nothing for another step. The
//  step ends on its pose_after, if it has one. A Fly naming a species opens
//  the party once and the members' menus down to the first that lists FLY
//  (use_field_move()); `party` records that slot, so a later Fly opens
//  only its menu. A Fly ends once the player has landed
//  (judge_fly_landing()), or fails as fly_no_takeoff or fly_no_landing.
std::optional<Pokemon::Outcome> run_field_move_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, PlayerGender player,
    const Pokemon::StoryStep& step, PartyCache& party
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
