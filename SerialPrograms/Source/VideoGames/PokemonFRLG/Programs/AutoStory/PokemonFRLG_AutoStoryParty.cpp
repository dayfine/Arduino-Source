/*  AutoStory Party
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
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

//  pret/pokefirered include/constants/pokemon.h TYPE_*, MYSTERY left out.
enum class Type{
    NORMAL, FIGHTING, FLYING, POISON, GROUND, ROCK, BUG, GHOST, STEEL,
    FIRE, WATER, GRASS, ELECTRIC, PSYCHIC, ICE, DRAGON, DARK,
};

struct MoveData{
    std::string_view move;
    uint8_t pp = 0;
    //  Flail's and Dragon Rage's damage doesn't come from their power, 1.
    uint8_t power = 0;
    Type type = Type::NORMAL;
};

//  pret/pokefirered src/data/battle_moves.h.
constexpr MoveData MOVES[] = {
    {.move = "tackle", .pp = 35, .power = 35, .type = Type::NORMAL},
    {.move = "tail-whip", .pp = 30, .power = 0, .type = Type::NORMAL},
    {.move = "bubble", .pp = 30, .power = 20, .type = Type::WATER},
    {.move = "withdraw", .pp = 40, .power = 0, .type = Type::WATER},
    {.move = "water-gun", .pp = 25, .power = 40, .type = Type::WATER},
    {.move = "bite", .pp = 25, .power = 60, .type = Type::DARK},
    {.move = "rapid-spin", .pp = 40, .power = 20, .type = Type::NORMAL},
    {.move = "protect", .pp = 10, .power = 0, .type = Type::NORMAL},
    {.move = "rain-dance", .pp = 5, .power = 0, .type = Type::WATER},
    {.move = "skull-bash", .pp = 15, .power = 100, .type = Type::NORMAL},
    {.move = "hydro-pump", .pp = 5, .power = 120, .type = Type::WATER},
    {.move = "splash", .pp = 40, .power = 0, .type = Type::NORMAL},
    {.move = "flail", .pp = 15, .power = 1, .type = Type::NORMAL},
    {.move = "thrash", .pp = 20, .power = 90, .type = Type::NORMAL},
    {.move = "dragon-rage", .pp = 10, .power = 1, .type = Type::DRAGON},
    {.move = "leer", .pp = 30, .power = 0, .type = Type::NORMAL},
    {.move = "twister", .pp = 20, .power = 40, .type = Type::DRAGON},
    {.move = "dragon-dance", .pp = 20, .power = 0, .type = Type::DRAGON},
    {.move = "hyper-beam", .pp = 5, .power = 150, .type = Type::NORMAL},
};

//  The types of the party's species and of the trainers' in
//  TRAINER_PARTIES (pret/pokefirered src/data/pokemon/species_info.h).
struct SpeciesTypes{
    std::string_view species;
    Type first;
    Type second;
};
constexpr SpeciesTypes SPECIES_TYPES[] = {
    {.species = "squirtle", .first = Type::WATER, .second = Type::WATER},
    {.species = "wartortle", .first = Type::WATER, .second = Type::WATER},
    {.species = "blastoise", .first = Type::WATER, .second = Type::WATER},
    {.species = "magikarp", .first = Type::WATER, .second = Type::WATER},
    {.species = "gyarados", .first = Type::WATER, .second = Type::FLYING},
    {.species = "pidgeotto", .first = Type::NORMAL, .second = Type::FLYING},
    {.species = "abra", .first = Type::PSYCHIC, .second = Type::PSYCHIC},
    {.species = "rattata", .first = Type::NORMAL, .second = Type::NORMAL},
    {.species = "bulbasaur", .first = Type::GRASS, .second = Type::POISON},
    {.species = "staryu", .first = Type::WATER, .second = Type::WATER},
    {.species = "starmie", .first = Type::WATER, .second = Type::PSYCHIC},
};

//  pret/pokefirered src/battle_main.c gTypeEffectiveness, the rows of the
//  types the party's moves have (Normal, Water, Dark, Dragon): every
//  pairing not here is 1x. In tenths: 20 is 2x, 5 is 0.5x.
struct Effectiveness{
    Type attack;
    Type defend;
    uint8_t tenths;
};
constexpr Effectiveness TYPE_CHART[] = {
    {.attack = Type::NORMAL, .defend = Type::ROCK, .tenths = 5},
    {.attack = Type::NORMAL, .defend = Type::STEEL, .tenths = 5},
    {.attack = Type::NORMAL, .defend = Type::GHOST, .tenths = 0},
    {.attack = Type::WATER, .defend = Type::FIRE, .tenths = 20},
    {.attack = Type::WATER, .defend = Type::WATER, .tenths = 5},
    {.attack = Type::WATER, .defend = Type::GRASS, .tenths = 5},
    {.attack = Type::WATER, .defend = Type::GROUND, .tenths = 20},
    {.attack = Type::WATER, .defend = Type::ROCK, .tenths = 20},
    {.attack = Type::WATER, .defend = Type::DRAGON, .tenths = 5},
    {.attack = Type::DARK, .defend = Type::FIGHTING, .tenths = 5},
    {.attack = Type::DARK, .defend = Type::PSYCHIC, .tenths = 20},
    {.attack = Type::DARK, .defend = Type::GHOST, .tenths = 20},
    {.attack = Type::DARK, .defend = Type::DARK, .tenths = 5},
    {.attack = Type::DARK, .defend = Type::STEEL, .tenths = 5},
    {.attack = Type::DRAGON, .defend = Type::DRAGON, .tenths = 20},
    {.attack = Type::DRAGON, .defend = Type::STEEL, .tenths = 5},
};

//  The species of a trainer's party, by the table's battle step, from
//  pret/pokefirered src/data/trainer_parties.h.
struct TrainerParty{
    std::string_view battle;
    std::array<std::string_view, 6> species;
};
constexpr TrainerParty TRAINER_PARTIES[] = {
    //  sParty_RivalCeruleanBulbasaur: Lv17, Lv16, Lv15, Lv18.
    {.battle = "rival_cerulean", .species = {"pidgeotto", "abra", "rattata", "bulbasaur"}},
    //  sParty_Misty: Lv18, Lv21.
    {.battle = "misty", .species = {"staryu", "starmie"}},
};

const MoveData* move_data(std::string_view move){
    for (const MoveData& entry : MOVES){
        if (entry.move == move){
            return &entry;
        }
    }
    return nullptr;
}

//  The story's plan per line (named by its first species): a level-up move
//  it learns over a full move list, and the move whose slot it takes. Any
//  other move offered over four is declined. Squirtle's is the speedrun's:
//  Water Gun where Bubble was (so the attack stays bottom left), Bite over
//  Tail Whip. TM moves are not level-up prompts and are not here.
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

const SpeciesTypes* species_types(std::string_view species){
    for (const SpeciesTypes& entry : SPECIES_TYPES){
        if (entry.species == species){
            return &entry;
        }
    }
    return nullptr;
}

//  In tenths; 10 for a species the table doesn't have.
size_t effectiveness(Type attack, std::string_view defender){
    const SpeciesTypes* types = species_types(defender);
    if (types == nullptr){
        return 10;
    }
    size_t tenths = 10;
    for (Type defend : {types->first, types->second}){
        for (const Effectiveness& entry : TYPE_CHART){
            if (entry.attack == attack && entry.defend == defend){
                tenths = tenths * entry.tenths / 10;
            }
        }
        if (types->second == types->first){
            break;
        }
    }
    return tenths;
}

//  `move`'s power times STAB times its effectiveness, summed over `foes`,
//  in hundredths of a point.
size_t expected_damage(const PartyMember& member, std::string_view move, const std::vector<std::string>& foes){
    const MoveData* data = move_data(move);
    if (data == nullptr){
        return 0;
    }
    const SpeciesTypes* own = species_types(member.species);
    const bool stab = own != nullptr && (own->first == data->type || own->second == data->type);
    size_t damage = 0;
    for (const std::string& foe : foes){
        damage += (size_t)data->power * (stab ? 15 : 10) * effectiveness(data->type, foe);
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
    PartyMember member{.species = std::string(species), .moves = {}, .moves_through_level = level, .moves_read = false};
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

PartyMember member_from_summary(std::string_view species, uint8_t level, const std::vector<SummaryMove>& moves){
    PartyMember member{.species = std::string(species), .moves = {}, .moves_through_level = level, .moves_read = true};
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

std::vector<std::string> trainer_party(std::string_view battle){
    std::vector<std::string> party;
    for (const TrainerParty& trainer : TRAINER_PARTIES){
        if (trainer.battle != battle){
            continue;
        }
        for (std::string_view species : trainer.species){
            if (!species.empty()){
                party.emplace_back(species);
            }
        }
    }
    return party;
}

std::optional<size_t> attack_slot_against(const PartyMember& member, const std::vector<std::string>& foes){
    const std::optional<size_t> plan = attack_slot(member);
    if (foes.empty() || !plan){
        return plan;
    }
    std::optional<size_t> best;
    size_t best_damage = 0;
    for (size_t slot : attack_order(member)){
        const size_t damage = expected_damage(member, member.moves[slot].move, foes);
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
