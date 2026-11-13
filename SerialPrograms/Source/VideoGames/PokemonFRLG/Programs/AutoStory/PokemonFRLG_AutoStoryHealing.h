/*  AutoStory Healing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lead's HP between battles, as far as the story can know it: the HP
 *  bar's share as last measured on a battle screen, the lead's maximum HP,
 *  and the HP of the items used since. From them, the HP to heal before a
 *  battle that wants full HP (Koga, 12.02), the items that heal it, and the
 *  items to buy so the bag covers a worst case.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryHealing_H
#define PokemonAutomation_PokemonFRLG_AutoStoryHealing_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The HP `item` (a bag slug) heals: a Potion 20, a Super Potion 50
//  (pokefirered items.json); 0 for another item.
int healing_hp(const std::string& item);

//  The lead's HP bar showed `fill` (1 after the nurse): the measurement
//  missing_hp() counts from, with no item used since.
void measured_hp(Supplies& supplies, double fill);

//  One of `item` was used on the lead: its count spent, and its HP
//  (healing_hp()) or, for another item, its cure taken into `supplies`.
void took_recovery_item(Supplies& supplies, const std::string& item);

//  The HP the lead misses: the bar's empty share of its maximum HP (the
//  summary's, else `fallback_max_hp`), rounded up by a bar pixel (the bar
//  is 48 pixels) unless the bar was full, less the HP healed since.
//  Nothing if no bar was measured since the checkpoint loaded.
std::optional<int> missing_hp(const Supplies& supplies, int fallback_max_hp);

//  Uses healing items with `use` (which returns whether the item was used,
//  having called took_recovery_item()) until missing_hp() is 0 or less:
//  a Super Potion while more than a Potion's HP is missing, else a Potion,
//  else a Super Potion. Stops when none is left or one couldn't be used.
//  Returns missing_hp() at the end.
std::optional<int> heal_to_full(
    Supplies& supplies, int fallback_max_hp, const std::function<bool(const std::string& item)>& use
);

//  How many of `item` to buy so the bag's Potions and Super Potions, with
//  them, heal at least `cover_hp`: 0 if they already do.
int hp_cover_shortfall(const Supplies& supplies, int cover_hp, const std::string& item);

//  How many more of `item` (a Potion or Super Potion) the bag needs to hold
//  `target` of it: 0 if it already does.
int fill_shortfall(const Supplies& supplies, int target, const std::string& item);

//  Registers the healing tests.
void add_tests_AutoStoryHealing(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
