/*  Mail Glitch Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  GAME actions specific to the Mail Glitch Activation program: buying Game
 *  Corner coins, redeeming a prize Pokemon, in-game trades (CH'DING / MIMIEN),
 *  party slot swapping, giving Items/Berries/Retro Mail (with Easy Chat),
 *  switch-grinding on Pokemon Tower 5F, faint+Revive Berry HP setup, executing
 *  the Knock Off + Recycle double battle, and verifying the active Mail Glitch.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchActions_H
#define PokemonAutomation_PokemonFRLG_MailGlitchActions_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline constexpr std::string_view BUY_COINS_ACTION = "buy_coins";
inline constexpr std::string_view REDEEM_PRIZE_MON_ACTION = "redeem_prize_mon";
inline constexpr std::string_view INGAME_TRADE_ACTION = "ingame_trade";
inline constexpr std::string_view PARTY_SWAP_ACTION = "party_swap";
inline constexpr std::string_view GIVE_BAG_ITEM_ACTION = "give_bag_item";
inline constexpr std::string_view SWITCH_GRIND_ACTION = "switch_grind";
inline constexpr std::string_view CLEAR_PURIFIED_ZONE_ACTION = "clear_purified_zone";
inline constexpr std::string_view PREPARE_BERRY_HP_ACTION = "prepare_berry_hp";
inline constexpr std::string_view MAIL_GLITCH_DOUBLE_BATTLE_ACTION = "mail_glitch_double_battle";
inline constexpr std::string_view VERIFY_MAIL_GLITCH_ACTION = "verify_mail_glitch";

//  Action specs for all Mail Glitch GAME steps (registered in story_action_specs()).
std::vector<Pokemon::StoryActionSpec> mail_glitch_action_specs();

//  Swaps two 0-based party slots from the overworld using PartyMemberMenuReader
//  to locate SWITCH dynamically regardless of HM count. Returns false on failure.
bool swap_party_slots(
    StoryEnv& env,
    size_t from_slot,
    size_t to_slot
);

//  Finds a party member's 0-based slot by species slug using summary reads.
std::optional<size_t> find_party_slot_by_species(
    StoryEnv& env,
    std::string_view species
);

//  Executes a Mail Glitch GAME step on the console; returns std::nullopt if
//  `step.action` is not a Mail Glitch action.
std::optional<Pokemon::Outcome> run_mail_glitch_game_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
