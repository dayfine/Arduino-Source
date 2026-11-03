/*  AutoStory Battle Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Gen 3 damage calculator against numbers worked by hand and the
 *  Elite Four Farmer's Starmie table, the move chosen against the foe out,
 *  the trainer AI's items and next Pokemon, and the win chance before the
 *  Cerulean rival.
 *
 */

#include <cmath>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBattleSim.h"
#include "PokemonFRLG_AutoStoryFoeTracker.h"
#include "PokemonFRLG_AutoStoryWinChance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStoryBattleSim : public UnitTest{
public:
    Test_AutoStoryBattleSim(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStoryBattleSim - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};

constexpr std::string_view RIVAL = "TRAINER_RIVAL_CERULEAN_BULBASAUR";
constexpr std::string_view MISTY = "TRAINER_LEADER_MISTY";

std::string stats_text(const Stats& s){
    return std::format("{}/{}/{}/{}/{}/{}", s.hp, s.attack, s.defense, s.sp_attack, s.sp_defense, s.speed);
}

std::string range_text(const DamageRange& r){
    return std::format("{}-{}", r.min, r.max);
}

//  The story's Wartortle at Lv22, OWN_GENES.
Combatant wartortle22(){
    return *combatant("wartortle", 22, OWN_GENES);
}

//  The rival's Bulbasaur: Lv18, pret iv 100 (IVs 12), Hardy.
Combatant rival_bulbasaur(){
    return *combatant("bulbasaur", 18, Genes{.iv = trainer_iv(100), .ev = 0, .nature = 0});
}


//  Worked by hand from CalculateMonStats and CalculateBaseDamage:
//  Wartortle Lv22 IV15: HP (2*59+15)*22/100+32 = 61, Attack 141*22/100+5 = 36,
//  ... Bite on the Bulbasaur (Sp. Def 142*18/100+5 = 30): 36*60*10/30/50+2
//  = 16, no STAB, neutral: 13 to 16; a crit doubles the 16 before the
//  roll: 27 to 32. Water Gun: 36*40*10/30/50+2 = 11, STAB 16, Grass 0.5x
//  8: 6 to 8. Its Vine Whip back: 30*35*9/43/50+2 = 6, STAB 9, Water 2x
//  18: 15 to 18.
UnitTestResult test_hand_numbers(){
    const Combatant own = wartortle22();
    const Combatant foe = rival_bulbasaur();
    if (stats_text(own.stats) != "61/36/43/36/43/33" || stats_text(foe.stats) != "46/24/24/30/30/23"){
        return "Stats: expected 61/36/43/36/43/33 and 46/24/24/30/30/23, got " + stats_text(own.stats) + " and " + stats_text(foe.stats) + ".";
    }
    const std::vector<std::tuple<std::string, DamageRange, std::string>> cases = {
        {"Bite", damage_range(own, foe, *move_data("bite"), false), "13-16"},
        {"Bite, crit", damage_range(own, foe, *move_data("bite"), true), "27-32"},
        {"Water Gun", damage_range(own, foe, *move_data("water-gun"), false), "6-8"},
        {"Vine Whip", damage_range(foe, own, *move_data("vine-whip"), false), "15-18"},
        {"Dragon Rage", damage_range(own, foe, *move_data("dragon-rage"), false), "40-40"},
    };
    for (const auto& [name, range, expected] : cases){
        if (range_text(range) != expected){
            return std::format("{}: expected {}, got {}.", name, expected, range_text(range));
        }
    }
    //  Tail Whip's -1: Defense 24 * 10 / 15 = 16; Tackle 36*35*10/16/50+2 = 17.
    Combatant whipped = foe;
    whipped.stages[static_cast<size_t>(Stage::DEFENSE)] = -1;
    const DamageRange tackle = damage_range(own, whipped, *move_data("tackle"), false);
    if (range_text(tackle) != "14-17"){
        return "Tackle after Tail Whip: expected 14-17, got " + range_text(tackle) + ".";
    }
    return true;
}


//  The Elite Four Farmer's STARMIE_PLAN (PokemonFRLG_EliteFourFarmer.cpp):
//  each pick "one-hit KOs each rematch opponent (min damage roll, no crit)"
//  at Sp. Atk 302, Lv100. Every rematch party's Pokemon of that species.
const std::map<std::string, std::string>& starmie_plan(){
    static const std::map<std::string, std::string> PLAN = {
        {"dewgong", "thunderbolt"}, {"cloyster", "thunderbolt"}, {"piloswine", "surf"},
        {"jynx", "surf"}, {"lapras", "thunderbolt"}, {"steelix", "surf"},
        {"hitmonchan", "psychic"}, {"hitmonlee", "psychic"}, {"machamp", "psychic"},
        {"gengar", "psychic"}, {"crobat", "thunderbolt"}, {"misdreavus", "surf"},
        {"arbok", "psychic"}, {"gyarados", "thunderbolt"}, {"dragonite", "ice-beam"},
        {"kingdra", "psychic"}, {"aerodactyl", "surf"}, {"heracross", "psychic"},
        {"alakazam", "surf"}, {"tyranitar", "surf"}, {"arcanine", "surf"},
        {"exeggutor", "ice-beam"}, {"blastoise", "thunderbolt"}, {"venusaur", "psychic"},
        {"charizard", "thunderbolt"},
    };
    return PLAN;
}

Combatant starmie(uint16_t sp_attack){
    Combatant body = *combatant("starmie", 100, OWN_GENES);
    body.stats.sp_attack = sp_attack;
    body.volcano_badge = true;
    return body;
}

//  The species whose pick misses the OHKO at `sp_attack`; "" for none.
std::string first_miss(uint16_t sp_attack, std::string_view only = ""){
    for (std::string_view trainer : {
        "TRAINER_ELITE_FOUR_LORELEI_2", "TRAINER_ELITE_FOUR_BRUNO_2", "TRAINER_ELITE_FOUR_AGATHA_2", "TRAINER_ELITE_FOUR_LANCE_2",
        "TRAINER_CHAMPION_REMATCH_SQUIRTLE", "TRAINER_CHAMPION_REMATCH_BULBASAUR", "TRAINER_CHAMPION_REMATCH_CHARMANDER",
    }){
        for (const TrainerMon& mon : trainer_mons(trainer)){
            const auto pick = starmie_plan().find(mon.species);
            if (pick == starmie_plan().end() || (!only.empty() && mon.species != only)){
                continue;
            }
            const Combatant foe = *combatant(mon.species, mon.level, Genes{.iv = trainer_iv(mon.iv), .ev = 0, .nature = mon.nature});
            const DamageRange range = damage_range(starmie(sp_attack), foe, *move_data(pick->second), false);
            if (range.min < foe.stats.hp){
                return std::format("{} {} Lv{} ({} HP, {} {})", trainer, mon.species, mon.level, foe.stats.hp, pick->second, range_text(range));
            }
        }
    }
    return "";
}

UnitTestResult test_elite_four_table(){
    const std::string miss = first_miss(302);
    if (!miss.empty()){
        return "At Sp. Atk 302, no OHKO at the lowest roll: " + miss + ".";
    }
    if (first_miss(301, "kingdra").empty()){
        return "Expected Kingdra to set the 302 Sp. Atk requirement: Psychic OHKOs it at 301 too.";
    }
    return true;
}


//  Misty's Staryu (Water) and Starmie (Water/Psychic): a lead with
//  Thundershock and Bite thunders the Staryu (2x against 1x) and bites the
//  Starmie (both 2x, Bite stronger). As the battle loop asks, with the
//  foe out known; summing over the party picked one move for both.
UnitTestResult test_misty_per_foe(){
    const PartyMember lead = member_from_summary("wartortle", 22, {
        {.move = "tackle", .pp = 35, .max_pp = 35}, {.move = "bite", .pp = 25, .max_pp = 25},
        {.move = "thunder-shock", .pp = 30, .max_pp = 30}, {.move = "withdraw", .pp = 40, .max_pp = 40},
    });
    for (const BattleRisk risk : {BattleRisk::LOW, BattleRisk::HIGH}){
        for (const auto& [out, expected] : {std::pair<size_t, size_t>{0, 2}, {1, 1}}){
            Supplies supplies;
            supplies.lead = lead;
            supplies.potions = 0;
            supplies.trainer = std::string(MISTY);
            supplies.foes = trainer_party(MISTY);
            supplies.risk = risk;
            supplies.foe_out = out;
            BattleDecisions decisions(supplies, BattleKind::TRAINER);
            const PartyStatus green{.hp_seen = true, .hp_yellow = false, .hp_red = false, .hp_fill = 1, .status = StatusCondition::NONE};
            const size_t slot = decisions.turn(green).slot;
            if (slot != expected){
                return std::format("Risk {}, foe {}: expected slot {}, got {}.", battle_risk_name(risk), out, expected, slot);
            }
        }
    }
    return true;
}


//  ShouldUseItem for Misty's Super Potion on her Starmie (56 HP): below
//  1/4 (14) or more than 50 missing; a Full Restore only below 1/4. A
//  second item waits until at most itemsNo - 1 + 1 Pokemon are left.
UnitTestResult test_ai_heal_rule(){
    SimBattler starmie = *trainer_battler(trainer_mons(MISTY)[1]);
    if (starmie.body.stats.hp != 56){
        return std::format("Expected Misty's Starmie at 56 HP, got {}.", starmie.body.stats.hp);
    }
    const std::vector<std::string> super = {"super-potion"};
    const std::vector<std::tuple<uint16_t, std::vector<std::string>, size_t, bool>> cases = {
        {56, super, 1, false}, {20, super, 1, false}, {13, super, 1, true}, {5, super, 1, true},
        {20, {"full-restore"}, 1, false}, {13, {"full-restore"}, 1, true},
        {13, {"", "hyper-potion"}, 3, false}, {13, {"", "hyper-potion"}, 2, true},
    };
    for (const auto& [hp, items, left, uses] : cases){
        starmie.hp = hp;
        if (ai_item_choice(starmie, items, left).has_value() != uses){
            return std::format("At {} HP with {} items, {} left: expected {}.", hp, items.size(), left, uses ? "an item" : "none");
        }
    }
    //  Each item once: the heal it would use is gone after.
    starmie.hp = 6;
    std::vector<std::string> once = super;
    once[*ai_item_choice(starmie, once, 1)].clear();
    if (ai_item_choice(starmie, once, 1)){
        return "Expected the Super Potion used only once.";
    }
    return true;
}


//  GetMostSuitableMonToSwitchInto against a Wartortle after the rival's
//  Pidgeotto: Water hits Abra and Rattata 1x and Bulbasaur 0.25x, but
//  only Bulbasaur has a move super effective on Water (Vine Whip): it
//  comes next, not Abra. The tracker, with no name read, follows that.
UnitTestResult test_next_foe(){
    const PartyMember lead = grown_to(new_member("squirtle", 5), 22);
    TrainerFoeTracker tracker(RIVAL, lead);
    if (tracker.out() != 0){
        return "Expected the rival's Pidgeotto out first.";
    }
    for (int i = 0; i < 3; i++){
        tracker.boxes_seen(BattleBoxes{.lead = true, .foe = false});
    }
    tracker.at_menu("");
    if (tracker.out() != 3){
        return std::format("Expected Bulbasaur (slot 3) after Pidgeotto, got {}.", tracker.out() ? std::to_string(*tracker.out()) : "none");
    }
    tracker.at_menu("abra");
    if (tracker.out() != 1){
        return "Expected the name read (Abra) over the AI's choice.";
    }
    return true;
}


//  The default risks: the rival and gym leaders HIGH, route trainers and
//  grunts LOW.
UnitTestResult test_risks(){
    const std::vector<std::tuple<std::string_view, std::string_view, BattleRisk>> cases = {
        {"CeruleanCity", "rival_cerulean", BattleRisk::HIGH},
        {"CeruleanCity_Gym", "misty", BattleRisk::HIGH},
        {"PewterCity_Gym", "brock", BattleRisk::HIGH},
        {"Route24", "lass_ali", BattleRisk::LOW},
        {"MtMoon_B2F", "rocket_grunt", BattleRisk::LOW},
        {"Route1", "nobody", BattleRisk::LOW},
    };
    for (const auto& [map, battle, risk] : cases){
        if (story_battle_risk(map, battle) != risk){
            return std::format("{} on {}: expected {}.", battle, map, battle_risk_name(risk));
        }
    }
    return true;
}


//  The Cerulean rival (Pidgeotto, Abra, Rattata, then a Bulbasaur with
//  Sleep Powder, Poison Powder and Leech Seed): the Lv22 Wartortle with 5
//  Potions that 00.15's GRIND plans for wins about a quarter of the sim's
//  battles, and is refused with the reason; at Lv30 it wins over 90%.
UnitTestResult test_rival_win_chance(){
    const std::optional<WinEstimate> planned = story_win_chance(grown_to(new_member("squirtle", 5), 22), 5, 1.0, RIVAL);
    const std::optional<std::string> refusal = planned ? win_chance_refusal(*planned) : std::nullopt;
    if (!planned || planned->chance < 0.15 || planned->chance > 0.40){
        return std::format("Lv22, 5 Potions: expected 15% to 40%, got {:.1f}%.", planned ? 100 * planned->chance : 0.0);
    }
    if (refusal != std::format("win chance {:.0f}% < 70%: grind first", std::floor(100 * planned->chance))){
        return "Lv22, 5 Potions: expected a refusal with the chance, got \"" + refusal.value_or("") + "\".";
    }
    const std::optional<WinEstimate> ground = story_win_chance(grown_to(new_member("squirtle", 5), 30), 5, 1.0, RIVAL);
    if (!ground || ground->chance < 0.90 || win_chance_refusal(*ground)){
        return std::format("Lv30, 5 Potions: expected over 90% and no refusal, got {:.1f}%.", ground ? 100 * ground->chance : 0.0);
    }
    return true;
}

}


void add_tests_AutoStoryBattleSim(UnitTestDatabase& database){
    database.add<Test_AutoStoryBattleSim>("Gen 3 stats and damage worked by hand", test_hand_numbers);
    database.add<Test_AutoStoryBattleSim>("the Elite Four Farmer's Starmie picks OHKO at Sp. Atk 302", test_elite_four_table);
    database.add<Test_AutoStoryBattleSim>("Misty: Thundershock on Staryu, Bite on Starmie", test_misty_per_foe);
    database.add<Test_AutoStoryBattleSim>("the trainer AI's healing items", test_ai_heal_rule);
    database.add<Test_AutoStoryBattleSim>("the rival's next Pokemon when no name is read", test_next_foe);
    database.add<Test_AutoStoryBattleSim>("battle risks by trainer", test_risks);
    database.add<Test_AutoStoryBattleSim>("Cerulean rival: refused at Lv22, fought at Lv30", test_rival_win_chance);
}



}
}
}
