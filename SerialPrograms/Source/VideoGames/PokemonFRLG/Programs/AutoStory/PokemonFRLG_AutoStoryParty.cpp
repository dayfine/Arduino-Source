/*  AutoStory Party
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <iterator>
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct LevelUpMove{
    std::string_view species;
    uint8_t level = 0;
    std::string_view move;
};

//  pret/pokefirered src/data/pokemon/level_up_learnsets.h, the lines the
//  story's party comes from.
constexpr LevelUpMove LEVEL_UP_MOVES[] = {
    {.species = "squirtle", .level = 1, .move = "tackle"},
    {.species = "squirtle", .level = 4, .move = "tail-whip"},
    {.species = "squirtle", .level = 7, .move = "bubble"},
    {.species = "squirtle", .level = 10, .move = "withdraw"},
    {.species = "squirtle", .level = 13, .move = "water-gun"},
    {.species = "squirtle", .level = 18, .move = "bite"},
    {.species = "squirtle", .level = 23, .move = "rapid-spin"},
    {.species = "squirtle", .level = 28, .move = "protect"},
    {.species = "squirtle", .level = 33, .move = "rain-dance"},
    {.species = "squirtle", .level = 40, .move = "skull-bash"},
    {.species = "squirtle", .level = 47, .move = "hydro-pump"},
    {.species = "wartortle", .level = 1, .move = "tackle"},
    {.species = "wartortle", .level = 1, .move = "tail-whip"},
    {.species = "wartortle", .level = 1, .move = "bubble"},
    {.species = "wartortle", .level = 4, .move = "tail-whip"},
    {.species = "wartortle", .level = 7, .move = "bubble"},
    {.species = "wartortle", .level = 10, .move = "withdraw"},
    {.species = "wartortle", .level = 13, .move = "water-gun"},
    {.species = "wartortle", .level = 19, .move = "bite"},
    {.species = "wartortle", .level = 25, .move = "rapid-spin"},
    {.species = "wartortle", .level = 31, .move = "protect"},
    {.species = "wartortle", .level = 37, .move = "rain-dance"},
    {.species = "wartortle", .level = 45, .move = "skull-bash"},
    {.species = "wartortle", .level = 53, .move = "hydro-pump"},
    {.species = "blastoise", .level = 1, .move = "tackle"},
    {.species = "blastoise", .level = 1, .move = "tail-whip"},
    {.species = "blastoise", .level = 1, .move = "bubble"},
    {.species = "blastoise", .level = 1, .move = "withdraw"},
    {.species = "blastoise", .level = 4, .move = "tail-whip"},
    {.species = "blastoise", .level = 7, .move = "bubble"},
    {.species = "blastoise", .level = 10, .move = "withdraw"},
    {.species = "blastoise", .level = 13, .move = "water-gun"},
    {.species = "blastoise", .level = 19, .move = "bite"},
    {.species = "blastoise", .level = 25, .move = "rapid-spin"},
    {.species = "blastoise", .level = 31, .move = "protect"},
    {.species = "blastoise", .level = 42, .move = "rain-dance"},
    {.species = "blastoise", .level = 55, .move = "skull-bash"},
    {.species = "blastoise", .level = 68, .move = "hydro-pump"},
    {.species = "magikarp", .level = 1, .move = "splash"},
    {.species = "magikarp", .level = 15, .move = "tackle"},
    {.species = "magikarp", .level = 30, .move = "flail"},
    {.species = "gyarados", .level = 1, .move = "thrash"},
    {.species = "gyarados", .level = 20, .move = "bite"},
    {.species = "gyarados", .level = 25, .move = "dragon-rage"},
    {.species = "gyarados", .level = 30, .move = "leer"},
    {.species = "gyarados", .level = 35, .move = "twister"},
    {.species = "gyarados", .level = 40, .move = "hydro-pump"},
    {.species = "gyarados", .level = 45, .move = "rain-dance"},
    {.species = "gyarados", .level = 50, .move = "dragon-dance"},
    {.species = "gyarados", .level = 55, .move = "hyper-beam"},
};

struct Evolution{
    std::string_view species;
    LevelEvolution evolution;
};

//  pret/pokefirered src/data/pokemon/evolution.h.
constexpr Evolution EVOLUTIONS[] = {
    {.species = "squirtle", .evolution = {.into = "wartortle", .level = 16}},
    {.species = "wartortle", .evolution = {.into = "blastoise", .level = 36}},
    {.species = "magikarp", .evolution = {.into = "gyarados", .level = 20}},
};

//  The story's plan per line (named by its first species): a level-up move
//  it learns over a full move list, and the move whose slot it takes. Any
//  other move offered over four is declined. Squirtle's is the speedrun's:
//  Water Gun where Bubble was (so the attack stays bottom left), Bite over
//  Tail Whip. TM and tutor moves are not level-up prompts and are not
//  here; no rule replaces them, so Water Pulse (TM03, 00.14c) and Mega Kick
//  (Route 4's tutor, 00.14) are kept over every later level-up move.
struct LearnRule{
    std::string_view line;
    std::string_view move;
    std::string_view replaces;
};
constexpr LearnRule LEARN_RULES[] = {
    {.line = "squirtle", .move = "water-gun", .replaces = "bubble"},
    {.line = "squirtle", .move = "bite", .replaces = "tail-whip"},
    //  A save whose Water Gun took another slot may have no Tail Whip.
    {.line = "squirtle", .move = "bite", .replaces = "withdraw"},
    {.line = "magikarp", .move = "twister", .replaces = "splash"},
};

//  The attacks per line, the one used first at the top. Past these, the
//  strongest damaging move with PP left.
struct Attack{
    std::string_view line;
    std::string_view move;
};
constexpr Attack ATTACKS[] = {
    {.line = "squirtle", .move = "water-gun"},
    {.line = "squirtle", .move = "bite"},
    {.line = "squirtle", .move = "bubble"},
    {.line = "squirtle", .move = "tackle"},
    {.line = "magikarp", .move = "bite"},
    {.line = "magikarp", .move = "twister"},
    {.line = "magikarp", .move = "tackle"},
};


//  The first species of `species`'s line.
std::string_view line_of(std::string_view species){
    for (const Evolution& evolution : EVOLUTIONS){
        if (evolution.evolution.into == species){
            return line_of(evolution.species);
        }
    }
    return species;
}

bool knows(const PartyMember& member, std::string_view move){
    return std::ranges::any_of(member.moves, [move](const KnownMove& known){
        return known.move == move;
    });
}

std::optional<size_t> slot_of(const PartyMember& member, std::string_view move){
    for (size_t slot = 0; slot < member.moves.size(); slot++){
        if (member.moves[slot].move == move){
            return slot;
        }
    }
    return std::nullopt;
}

//  The next level-up move past `moves_through_level` that it doesn't know.
const LevelUpMove* next_move(const PartyMember& member){
    for (const LevelUpMove& entry : LEVEL_UP_MOVES){
        if (entry.species == member.species && entry.level > member.moves_through_level && !knows(member, entry.move)){
            return &entry;
        }
    }
    return nullptr;
}

KnownMove at_full_pp(std::string_view move){
    return KnownMove{.move = std::string(move), .pp = max_pp(move)};
}

//  The slot the plan gives `move`, over a full move list: the first of
//  its rules' moves the member knows.
std::optional<size_t> forget_slot_for(const PartyMember& member, std::string_view move){
    const std::string_view line = line_of(member.species);
    for (const LearnRule& rule : LEARN_RULES){
        if (rule.line != line || rule.move != move){
            continue;
        }
        if (const std::optional<size_t> slot = slot_of(member, rule.replaces)){
            return slot;
        }
    }
    return std::nullopt;
}

//  Learns, or answers the prompt for, each of its moves at exactly `level`.
void take_level(PartyMember& member, uint8_t level){
    for (const LevelUpMove& entry : LEVEL_UP_MOVES){
        if (entry.species != member.species || entry.level != level || knows(member, entry.move)){
            continue;
        }
        if (member.moves.size() < MAX_MOVES){
            member.moves.emplace_back(at_full_pp(entry.move));
        }else if (std::optional<size_t> slot = forget_slot_for(member, entry.move)){
            member.moves[*slot] = at_full_pp(entry.move);
        }
    }
    member.moves_through_level = level;
}

//  The base stat `attacker` uses `move` with: Attack or Sp. Atk, by the
//  move's type.
uint8_t attacking_stat(const SpeciesData& attacker, const MoveData& move){
    return special_type(move.type) ? attacker.base.sp_attack : attacker.base.attack;
}

//  In tenths.
size_t effectiveness(const MoveData& move, const SpeciesData& defender){
    size_t tenths = type_effectiveness(move.type, defender.first);
    if (defender.second != defender.first){
        tenths = tenths * type_effectiveness(move.type, defender.second) / 10;
    }
    return tenths;
}

//  `move`'s expected damage, summed over `foes`: its power times STAB
//  times its effectiveness times the lead's base attacking stat for it.
//  The foe's Defense and Sp. Def, levels, IVs, EVs, natures and accuracy
//  are left out. Each foe must be in species_data().
size_t expected_damage(const PartyMember& member, std::string_view move, const std::vector<std::string>& foes){
    const MoveData* data = move_data(move);
    const SpeciesData* own = species_data(member.species);
    if (data == nullptr || own == nullptr){
        return 0;
    }
    const bool stab = own->first == data->type || own->second == data->type;
    size_t damage = 0;
    for (const std::string& foe : foes){
        damage += (size_t)data->power * (stab ? 15 : 10) * effectiveness(*data, *species_data(foe)) * attacking_stat(*own, *data);
    }
    return damage;
}

//  The member's slots in the plan's order: its line's attacks first, as
//  ATTACKS lists them, then the rest in slot order.
std::vector<size_t> attack_order(const PartyMember& member){
    std::vector<size_t> order;
    const std::string_view line = line_of(member.species);
    for (const Attack& attack : ATTACKS){
        const std::optional<size_t> slot = attack.line == line ? slot_of(member, attack.move) : std::nullopt;
        if (slot && std::ranges::find(order, *slot) == order.end()){
            order.push_back(*slot);
        }
    }
    for (size_t slot = 0; slot < member.moves.size(); slot++){
        if (std::ranges::find(order, slot) == order.end()){
            order.push_back(slot);
        }
    }
    return order;
}

}


std::optional<LevelEvolution> level_evolution(std::string_view species){
    for (const Evolution& evolution : EVOLUTIONS){
        if (evolution.species == species){
            return evolution.evolution;
        }
    }
    return std::nullopt;
}

uint8_t max_pp(std::string_view move){
    const MoveData* data = move_data(move);
    return data == nullptr ? 0 : data->pp;
}

uint8_t move_power(std::string_view move){
    const MoveData* data = move_data(move);
    return data == nullptr ? 0 : data->power;
}

PartyMember new_member(std::string_view species, uint8_t level){
    PartyMember member{
        .species = std::string(species), .moves = {}, .moves_through_level = level, .moves_read = false,
        .level = 0, .stats = std::nullopt,
    };
    for (const LevelUpMove& entry : LEVEL_UP_MOVES){
        if (entry.species != species || entry.level > level || knows(member, entry.move)){
            continue;
        }
        if (member.moves.size() == MAX_MOVES){
            member.moves.erase(member.moves.begin());
        }
        member.moves.emplace_back(at_full_pp(entry.move));
    }
    return member;
}

PartyMember grown_to(PartyMember member, uint8_t level){
    for (uint8_t at = static_cast<uint8_t>(member.moves_through_level + 1); at <= level; at++){
        take_level(member, at);
        const std::optional<LevelEvolution> evolution = level_evolution(member.species);
        if (evolution && evolution->level <= at){
            evolve(member);
            //  The new species learns its moves of the level it evolved at.
            take_level(member, at);
        }
    }
    return member;
}

PartyMember story_lead_after_brock(){
    return grown_to(new_member("squirtle", 5), 13);
}

uint8_t battle_level(const PartyMember& member){
    return std::max(member.level, member.moves_through_level);
}

PartyMember member_from_summary(
    std::string_view species, uint8_t level, const std::vector<SummaryMove>& moves,
    const std::optional<Stats>& stats
){
    PartyMember member{
        .species = std::string(species), .moves = {}, .moves_through_level = level, .moves_read = true,
        .level = level, .stats = stats,
    };
    for (const SummaryMove& read : moves){
        const uint8_t full = max_pp(read.move);
        const uint8_t pp = read.pp >= 0 ? static_cast<uint8_t>(std::min<int>(read.pp, 255)) : static_cast<uint8_t>(full / 2);
        member.moves.emplace_back(KnownMove{.move = read.move, .pp = pp});
    }
    return member;
}

LearnDecision learn_move(PartyMember& member){
    while (member.moves.size() < MAX_MOVES){
        const LevelUpMove* learned = next_move(member);
        if (learned == nullptr){
            break;
        }
        member.moves.emplace_back(at_full_pp(learned->move));
        member.moves_through_level = learned->level;
    }
    const LevelUpMove* offered = next_move(member);
    if (offered == nullptr){
        return LearnDecision{};
    }
    member.moves_through_level = offered->level;
    LearnDecision decision{.move = std::string(offered->move), .forget_slot = forget_slot_for(member, offered->move)};
    if (decision.forget_slot){
        member.moves[*decision.forget_slot] = at_full_pp(offered->move);
    }
    return decision;
}

void evolve(PartyMember& member){
    if (const std::optional<LevelEvolution> evolution = level_evolution(member.species)){
        member.species = std::string(evolution->into);
        member.stats.reset();
    }
}

std::optional<size_t> attack_slot(const PartyMember& member){
    if (member.moves.size() < MAX_MOVES && !member.moves_read){
        return std::nullopt;
    }
    const std::string_view line = line_of(member.species);
    for (const Attack& attack : ATTACKS){
        if (attack.line != line){
            continue;
        }
        const std::optional<size_t> slot = slot_of(member, attack.move);
        if (slot && member.moves[*slot].pp > 0){
            return slot;
        }
    }
    std::optional<size_t> strongest;
    for (size_t slot = 0; slot < member.moves.size(); slot++){
        const uint8_t power = move_power(member.moves[slot].move);
        if (power > 0 && member.moves[slot].pp > 0 && (!strongest || power > move_power(member.moves[*strongest].move))){
            strongest = slot;
        }
    }
    if (strongest){
        return strongest;
    }
    for (size_t slot = 0; slot < member.moves.size(); slot++){
        if (member.moves[slot].pp > 0){
            return slot;
        }
    }
    return TOP_LEFT_MOVE;
}

std::vector<std::string> trainer_party(std::string_view trainer){
    std::vector<std::string> party;
    for (const TrainerMon& mon : trainer_mons(trainer)){
        party.push_back(mon.species);
    }
    return party;
}

std::optional<size_t> attack_slot_against(const PartyMember& member, const std::vector<std::string>& foes){
    const std::optional<size_t> plan = attack_slot(member);
    std::vector<std::string> known;
    std::ranges::copy_if(foes, std::back_inserter(known), [](const std::string& foe){
        return species_data(foe) != nullptr;
    });
    if (known.empty() || !plan){
        return plan;
    }
    std::optional<size_t> best;
    size_t best_damage = 0;
    for (size_t slot : attack_order(member)){
        const size_t damage = expected_damage(member, member.moves[slot].move, known);
        if (member.moves[slot].pp > 0 && damage > best_damage){
            best = slot;
            best_damage = damage;
        }
    }
    return best ? best : plan;
}

size_t attack_pp(const PartyMember& member){
    size_t pp = 0;
    for (const KnownMove& known : member.moves){
        if (move_power(known.move) > 0){
            pp += known.pp;
        }
    }
    return pp;
}

void spend_pp(PartyMember& member, size_t slot){
    if (slot < member.moves.size() && member.moves[slot].pp > 0){
        member.moves[slot].pp--;
    }
}

void no_pp_left(PartyMember& member, size_t slot){
    if (slot < member.moves.size()){
        member.moves[slot].pp = 0;
    }
}

void restore_pp(PartyMember& member){
    for (KnownMove& known : member.moves){
        known.pp = max_pp(known.move);
    }
}



}
}
}
