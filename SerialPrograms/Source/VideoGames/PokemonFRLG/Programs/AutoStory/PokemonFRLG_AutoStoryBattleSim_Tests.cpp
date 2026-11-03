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
    const std::optional<WinEstimate> planned = story_win_chance(grown_to(new_member("squirtle", 5), 22), SimItems{.potions = 5}, 1.0, RIVAL);
    const std::optional<std::string> refusal = planned ? win_chance_refusal(*planned) : std::nullopt;
    if (!planned || planned->chance < 0.15 || planned->chance > 0.40){
        return std::format("Lv22, 5 Potions: expected 15% to 40%, got {:.1f}%.", planned ? 100 * planned->chance : 0.0);
    }
    if (refusal != std::format("win chance {:.0f}% < 70% with {}: grind first", std::floor(100 * planned->chance), planned->inputs)){
        return "Lv22, 5 Potions: expected a refusal with the chance, got \"" + refusal.value_or("") + "\".";
    }
    const std::optional<WinEstimate> ground = story_win_chance(grown_to(new_member("squirtle", 5), 30), SimItems{.potions = 5}, 1.0, RIVAL);
    if (!ground || ground->chance < 0.90 || win_chance_refusal(*ground)){
        return std::format("Lv30, 5 Potions: expected over 90% and no refusal, got {:.1f}%.", ground ? 100 * ground->chance : 0.0);
    }
    return true;
}


//  Console B, 2026-11-03 (run m51), Misty: the Wartortle's moves, Bite,
//  Tail Whip, Bubble, Withdraw (Water Gun was lost earlier).
std::vector<SummaryMove> m51_moves(){
    return {
        {.move = "bite", .pp = 25, .max_pp = 25}, {.move = "tail-whip", .pp = 30, .max_pp = 30},
        {.move = "bubble", .pp = 30, .max_pp = 30}, {.move = "withdraw", .pp = 40, .max_pp = 40},
    };
}

//  m51's Wartortle as its summary would read: `level`, 60 HP at Lv20,
//  the other stats at OWN_GENES.
PartyMember m51_lead(uint8_t level){
    Stats stats = combatant("wartortle", 20, OWN_GENES)->stats;
    stats.hp = 60;
    PartyMember lead = member_from_summary("wartortle", 20, m51_moves(), stats);
    lead.moves_through_level = level;
    return lead;
}

//  m51's Wartortle Lv20 at `hp` of 60.
SimBattler m51_wartortle(uint16_t hp){
    SimBattler own = *own_battler(m51_lead(20));
    own.hp = hp;
    return own;
}

//  m51, turns 8 to 13: at 8/60 HP against Misty's Starmie, which hit
//  about 20 a turn, the HIGH tier's Potion (+20) was undone by each hit,
//  five times over; then Bite once with no Potions left, and it fainted.
//  The loop replayed at either tier: once a Potion is seen undone by the
//  next hit, no more, but Bite, which also flinches.
UnitTestResult test_m51_potion_loop(){
    for (const BattleRisk risk : {BattleRisk::LOW, BattleRisk::HIGH}){
        Supplies supplies;
        supplies.lead = m51_lead(20);
        supplies.potions = 5;
        supplies.trainer = std::string(MISTY);
        supplies.foes = trainer_party(MISTY);
        supplies.risk = risk;
        supplies.foe_out = 1;
        BattleDecisions decisions(supplies, BattleKind::TRAINER);
        const PartyStatus red{.hp_seen = true, .hp_yellow = false, .hp_red = true, .hp_fill = 8.0 / 60, .status = StatusCondition::NONE};
        int potions = 0;
        for (int turn = 8; turn <= 13; turn++){
            const TurnAction action = decisions.turn(red);
            if (action.choice == TurnChoice::POTION){
                potions++;
            }else if (action.choice != TurnChoice::MOVE || action.slot != 0){
                return std::format("Risk {}, turn {}: expected a Potion or Bite (slot 0), got slot {}.", battle_risk_name(risk), turn, action.slot);
            }
            decisions.turn_done(action.choice, TurnResult::DONE);
        }
        if (potions > 1){
            return std::format("Risk {}: {} Potions at 8/60 HP over turns 8 to 13, each undone by the next hit; expected at most 1.", battle_risk_name(risk), potions);
        }
    }
    return true;
}

//  The sim's own choice by the same rule: at 8/60 a Potion against a foe
//  whose hit undoes it, no Potion; against one whose hit could faint the
//  lead but heals less than 20, a Potion.
UnitTestResult test_sim_potion_rule(){
    const SimTrainer misty = *sim_trainer(MISTY);
    SimBattler starmie = misty.party[1];
    const SimBattler own = m51_wartortle(8);
    //  Starmie as m51 saw it: at least the Potion's 20 a hit.
    starmie.body.stats.attack = static_cast<uint16_t>(starmie.body.stats.attack * 2);
    starmie.body.stats.sp_attack = static_cast<uint16_t>(starmie.body.stats.sp_attack * 2);
    if (potion_case(own, starmie).expected_hit < POTION_HP){
        return std::format("Expected Starmie's mean hit at 20 or more, got {:.1f}.", potion_case(own, starmie).expected_hit);
    }
    const OwnTurn sim = own_turn(own, SimItems{.potions = 5}, starmie, misty.items, 1);
    if (sim.potion || sim.slot != size_t(0)){
        return std::format(
            "Sim at 8/60 against Starmie (max hit {}): expected Bite (slot 0), got {}.",
            foe_max_hit(starmie, own), sim.potion ? "a Potion" : "slot " + std::to_string(sim.slot.value_or(9))
        );
    }
    const SimBattler& staryu = misty.party[0];
    const SimBattler low = m51_wartortle(6);
    const uint32_t hit = foe_max_hit(staryu, low);
    if (hit < 6 || hit >= POTION_HP){
        return std::format("Expected Staryu's best hit on the Wartortle from 6 to 19, got {}.", hit);
    }
    if (!own_turn(low, SimItems{.potions = 5}, staryu, misty.items, 2).potion){
        return std::format("At 6/60 against Staryu (max hit {}): expected a Potion.", hit);
    }
    return true;
}

//  The sim plays the lead the summary showed, not the one the story
//  counts: m51's Wartortle (Lv20, 60 HP, Bite, Tail Whip, Bubble,
//  Withdraw) with 5 Potions wins about 96% against Misty; the story's own
//  Wartortle Lv20 (Tackle, Bite, Water Gun, Withdraw; 56 HP at OWN_GENES)
//  about 94%, from other inputs. m51 lost: the sim leaves out Water
//  Pulse's confusion, among others (PokemonFRLG_AutoStoryWinChance.h).
UnitTestResult test_misty_real_lead(){
    const std::optional<WinEstimate> real = story_win_chance(m51_lead(20), SimItems{.potions = 5}, 1.0, MISTY);
    const std::optional<WinEstimate> story = story_win_chance(grown_to(new_member("squirtle", 5), 20), SimItems{.potions = 5}, 1.0, MISTY);
    if (!real || !story){
        return "Expected win chances for both leads.";
    }
    const std::string expected = "wartortle Lv20 60/60 HP: bite 25/25, tail-whip 30/30, bubble 30/30, withdraw 40/40; 5 Potions";
    if (real->inputs != expected){
        return "m51's lead: expected the inputs \"" + expected + "\", got \"" + real->inputs + "\".";
    }
    if (real->chance < 0.90 || story->chance < 0.85 || real->chance == story->chance || story->inputs == real->inputs){
        return std::format(
            "Expected m51's lead ({:.1f}%) and the story's ({:.1f}%, {}) to differ, both over 85%.",
            100 * real->chance, 100 * story->chance, story->inputs
        );
    }
    return true;
}

//  The Cerulean rival against m51's moves (no Water Gun), the levels
//  since the summary added to its stats: refused at Lv20, 22 and 25 (about
//  32%, 37%, 66%), fought at Lv30 (about 95%). The refusal names the
//  chance and the lead it was played with.
UnitTestResult test_rival_real_moves(){
    const std::vector<std::tuple<uint8_t, double, double>> cases = {
        {20, 0.20, 0.45}, {22, 0.25, 0.50}, {25, 0.50, 0.70}, {30, 0.90, 1.00},
    };
    for (const auto& [level, low, high] : cases){
        const std::optional<WinEstimate> estimate = story_win_chance(m51_lead(level), SimItems{.potions = 5}, 1.0, RIVAL);
        if (!estimate || estimate->chance < low || estimate->chance >= high){
            return std::format("Lv{}: expected {:.0f}% to {:.0f}%, got {:.1f}%.", level, 100 * low, 100 * high, estimate ? 100 * estimate->chance : 0.0);
        }
        const bool refused = win_chance_refusal(*estimate).has_value();
        if (refused != (level < 30)){
            return std::format("Lv{} at {:.1f}%: expected {}.", level, 100 * estimate->chance, level < 30 ? "a refusal" : "no refusal");
        }
    }
    const std::optional<WinEstimate> lv22 = story_win_chance(m51_lead(22), SimItems{.potions = 5}, 1.0, RIVAL);
    const std::string refusal = win_chance_refusal(*lv22).value_or("");
    if (refusal.find("wartortle Lv22 65/65 HP: bite") == std::string::npos || refusal.find("; 5 Potions") == std::string::npos){
        return "Lv22: expected the refusal to name the lead's level, HP, moves and Potions, got \"" + refusal + "\".";
    }
    return true;
}


//  Console B, 2026-11-03 (m56, 00.15b, run 2): the Wartortle Lv22, 65 HP,
//  Bite, Water Pulse, Bubble and Withdraw.
PartyMember m56_lead(){
    Stats stats = combatant("wartortle", 22, OWN_GENES)->stats;
    stats.hp = 65;
    std::vector<SummaryMove> moves;
    for (const char* move : {"bite", "water-pulse", "bubble", "withdraw"}){
        moves.push_back(SummaryMove{.move = move, .pp = max_pp(move), .max_pp = max_pp(move)});
    }
    return member_from_summary("wartortle", 22, moves, stats);
}

//  m56's supplies against the rival's Bulbasaur (slot 3), at HIGH risk.
Supplies m56_supplies(int awakenings){
    Supplies supplies;
    supplies.lead = m56_lead();
    supplies.potions = 15;
    supplies.parlyz_heals = 1;
    supplies.awakenings = awakenings;
    supplies.trainer = std::string(RIVAL);
    supplies.foes = trainer_party(RIVAL);
    supplies.risk = BattleRisk::HIGH;
    supplies.foe_out = 3;
    return supplies;
}

//  m56's bar at `hp` of 65, with `status`.
PartyStatus m56_bar(int hp, StatusCondition status){
    const double fill = hp / 65.0;
    return PartyStatus{.hp_seen = true, .hp_yellow = fill <= 0.5 && fill > 0.2, .hp_red = fill <= 0.2, .hp_fill = fill, .status = status};
}

//  m56 run 2 against the rival's Bulbasaur (Sleep Powder, Leech Seed,
//  Vine Whip): 49, 42, then 34/65 asleep, then 13/65 asleep, Bite every
//  turn and no Potion; it fainted. The 21 HP of turn 6 were Vine Whip and
//  Leech Seed, more than a Potion's 20, and an item's turn doesn't count
//  sleep down (pret battle_util.c AtkCanceller_UnableToUseMove), so a
//  Potion at 13 HP is undone each turn while the lead stays asleep: the
//  move is right there, and the log says why. With an Awakening, it is
//  used on turn 6, while the lead outlasts the hit its turn costs.
UnitTestResult test_rival_sleep_turns(){
    const std::vector<PartyStatus> turns = {
        m56_bar(49, StatusCondition::NONE), m56_bar(42, StatusCondition::NONE),
        m56_bar(34, StatusCondition::SLEEP), m56_bar(13, StatusCondition::SLEEP),
    };
    Supplies played = m56_supplies(0);
    BattleDecisions as_played(played, BattleKind::TRAINER);
    TurnAction action;
    for (const PartyStatus& status : turns){
        action = as_played.turn(status);
        as_played.turn_done(action.choice, TurnResult::DONE);
    }
    if (action.choice != TurnChoice::MOVE || action.heal_reason.find("undoes it") == std::string::npos){
        return std::format(
            "No Awakening, turn 7 at 13/65 asleep: expected a move and a reason naming the Potion undone, got choice {} and \"{}\".",
            static_cast<int>(action.choice), action.heal_reason
        );
    }
    Supplies supplied = m56_supplies(2);
    BattleDecisions with_awakenings(supplied, BattleKind::TRAINER);
    for (size_t turn = 0; turn < 3; turn++){
        action = with_awakenings.turn(turns[turn]);
        with_awakenings.turn_done(action.choice, TurnResult::DONE);
    }
    if (action.choice != TurnChoice::AWAKENING || supplied.awakenings != 1 || action.heal_reason.empty()){
        return std::format(
            "2 Awakenings, turn 6 at 34/65 asleep: expected an Awakening, with a reason, got choice {}, {} left, \"{}\".",
            static_cast<int>(action.choice), supplied.awakenings, action.heal_reason
        );
    }
    return true;
}

//  The sim's lead asleep, as the battle loop decides: an Awakening while it
//  outlasts the foe's next hit; below that, not (its turn would only let
//  the foe faint it); none against a foe that can't hurt it.
UnitTestResult test_sim_awakening_rule(){
    const SimTrainer rival = *sim_trainer(RIVAL);
    const SimBattler& bulbasaur = rival.party[3];
    SimBattler own = *own_battler(m56_lead());
    own.status = SimStatus::SLEEP;
    own.sleep = 4;
    own.hp = 34;
    if (!own_turn(own, SimItems{.potions = 15, .awakenings = 2}, bulbasaur, rival.items, 1).awakening){
        return "Asleep at 34/65 against the Bulbasaur with 2 Awakenings: expected one.";
    }
    if (own_turn(own, SimItems{.potions = 15, .awakenings = 0}, bulbasaur, rival.items, 1).awakening){
        return "Asleep with no Awakenings: expected none used.";
    }
    own.hp = 8;
    if (own_turn(own, SimItems{.potions = 0, .awakenings = 2}, bulbasaur, rival.items, 1).awakening){
        return "Asleep at 8/65, below the Bulbasaur's hit: expected no Awakening.";
    }
    own.hp = 34;
    const SimBattler& abra = rival.party[1];
    if (own_turn(own, SimItems{.potions = 15, .awakenings = 2}, abra, rival.items, 4).awakening){
        return "Asleep against Abra (Teleport only): expected no Awakening.";
    }
    return true;
}

//  m56's Wartortle Lv22 with 15 Potions against the rival: Awakenings
//  against the Bulbasaur's Sleep Powder win more battles (about 44% with
//  none, 52% with 2, 53% with 3), and the inputs name them.
UnitTestResult test_rival_awakenings(){
    const std::optional<WinEstimate> none = story_win_chance(m56_lead(), SimItems{.potions = 15}, 1.0, RIVAL);
    const std::optional<WinEstimate> three = story_win_chance(m56_lead(), SimItems{.potions = 15, .awakenings = 3}, 1.0, RIVAL);
    if (!none || !three || three->chance < none->chance + 0.05){
        return std::format(
            "Lv22, 15 Potions: expected 3 Awakenings to win at least 5 points more than none, got {:.1f}% and {:.1f}%.",
            none ? 100 * none->chance : 0.0, three ? 100 * three->chance : 0.0
        );
    }
    if (three->inputs.find("; 15 Potions, 3 Awakenings") == std::string::npos){
        return "Expected the inputs to name the Awakenings, got \"" + three->inputs + "\".";
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
    database.add<Test_AutoStoryBattleSim>("m51: no Potion the foe's hit undoes", test_m51_potion_loop);
    database.add<Test_AutoStoryBattleSim>("the sim's Potion: only when it outheals the hit", test_sim_potion_rule);
    database.add<Test_AutoStoryBattleSim>("Misty: m51's lead as the summary showed it", test_misty_real_lead);
    database.add<Test_AutoStoryBattleSim>("Cerulean rival with m51's moves at Lv20 to 30", test_rival_real_moves);
    database.add<Test_AutoStoryBattleSim>("m56: asleep at red HP, a Potion undone; an Awakening first", test_rival_sleep_turns);
    database.add<Test_AutoStoryBattleSim>("the sim's Awakening: asleep, outlasting the next hit", test_sim_awakening_rule);
    database.add<Test_AutoStoryBattleSim>("Cerulean rival: Awakenings raise the win chance", test_rival_awakenings);
}



}
}
}
