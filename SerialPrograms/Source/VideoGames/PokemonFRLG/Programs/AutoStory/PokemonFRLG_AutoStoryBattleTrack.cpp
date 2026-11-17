/*  AutoStory Battle Track
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The texts are pokefirered's (src/battle_message.c), read as letters:
 *  "Foe VENUSAUR's SP. ATK rose!" is "foevenusaursspatkrose".
 *
 */

#include <algorithm>
#include <array>
#include "PokemonFRLG_AutoStoryBattleTrack.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Guard Spec's Mist lasts 5 turns (pret).
constexpr int MIST_TURNS = 5;
//  Sunny Day and Rain Dance last 5 turns, the one they start in included:
//  the choices of the 4 after it.
constexpr int WEATHER_TURNS = 4;
//  The fewest turns a sleep lasts (pret: 2 to 5), and Rest's.
constexpr int MIN_SLEEP_TURNS = 2;
constexpr int REST_SLEEP_TURNS = 3;
constexpr int MAX_STAGE = 6;

bool has(std::string_view letters, std::string_view word){
    return letters.find(word) != std::string_view::npos;
}

//  A stat stage change: whose, which, by how much.
struct StatChange{
    bool foe = false;
    int StatStages::* stat = nullptr;
    int stages = 0;

    bool operator==(const StatChange&) const = default;
};

struct StatWord{
    std::string_view word;
    int StatStages::* stat;
};
constexpr std::array<StatWord, 7> STAT_WORDS{{
    {"spatk", &StatStages::sp_attack}, {"spdef", &StatStages::sp_defense}, {"attack", &StatStages::attack},
    {"defense", &StatStages::defense}, {"speed", &StatStages::speed}, {"accuracy", &StatStages::accuracy},
    {"evasiveness", &StatStages::evasion},
}};
struct StageWord{
    std::string_view word;
    int stages;
};
//  "harshly fell" before "fell", "sharply rose" before "rose".
constexpr std::array<StageWord, 4> STAGE_WORDS{{
    {"sharplyrose", 2}, {"rose", 1}, {"harshlyfell", -2}, {"fell", -1},
}};

//  The stages "[sharply ]rose!" or "[harshly ]fell!" at the start of `after`.
std::optional<int> stages_at(std::string_view after){
    for (const StageWord& stage : STAGE_WORDS){
        if (after.starts_with(stage.word)){
            return stage.stages;
        }
    }
    return std::nullopt;
}

//  "<Pokemon>'s <STAT> [sharply ]rose!" or "[harshly ]fell!".
std::optional<StatChange> stat_change(std::string_view letters){
    for (const StatWord& stat : STAT_WORDS){
        for (size_t at = letters.find(stat.word); at != std::string_view::npos; at = letters.find(stat.word, at + 1)){
            if (const std::optional<int> stages = stages_at(letters.substr(at + stat.word.size()))){
                return StatChange{.foe = letters.starts_with("foe"), .stat = stat.stat, .stages = *stages};
            }
        }
    }
    return std::nullopt;
}

enum class FoeStatusText{
    ASLEEP,
    RESTED,
    SLEPT_THROUGH,
    WOKE,
    FROZEN,
    PARALYZED,
    BURNED,
    POISONED,
    CURED,
};

//  The foe's status texts; none for ours or another text.
std::optional<FoeStatusText> foe_status_text(std::string_view letters){
    if (!letters.starts_with("foe")){
        return std::nullopt;
    }
    struct Word{
        std::string_view word;
        FoeStatusText status;
    };
    static constexpr std::array<Word, 11> WORDS{{
        {"cured", FoeStatusText::CURED}, {"wokeup", FoeStatusText::WOKE}, {"defrosted", FoeStatusText::CURED},
        {"fellasleep", FoeStatusText::ASLEEP}, {"wenttosleep", FoeStatusText::RESTED},
        {"sleptandbecamehealthy", FoeStatusText::RESTED}, {"fastasleep", FoeStatusText::SLEPT_THROUGH},
        {"frozensolid", FoeStatusText::FROZEN}, {"paralyzed", FoeStatusText::PARALYZED},
        {"wasburned", FoeStatusText::BURNED}, {"poisoned", FoeStatusText::POISONED},
    }};
    for (const Word& word : WORDS){
        if (has(letters, word.word)){
            return word.status;
        }
    }
    return std::nullopt;
}

void apply_status(FoeStatusText text, BattleTrackState& state){
    switch (text){
    case FoeStatusText::ASLEEP:
        state.foe_status = PlannedStatus::SLEEP;
        state.foe_sleep_turns = MIN_SLEEP_TURNS;
        return;
    case FoeStatusText::RESTED:
        state.foe_status = PlannedStatus::SLEEP;
        state.foe_sleep_turns = REST_SLEEP_TURNS;
        return;
    case FoeStatusText::SLEPT_THROUGH:
        state.foe_status = PlannedStatus::SLEEP;
        state.foe_sleep_turns = std::max(1, state.foe_sleep_turns - 1);
        return;
    case FoeStatusText::FROZEN:
        state.foe_status = PlannedStatus::FREEZE;
        return;
    case FoeStatusText::PARALYZED:
        state.foe_status = PlannedStatus::PARALYSIS;
        return;
    case FoeStatusText::BURNED:
        state.foe_status = PlannedStatus::BURN;
        return;
    case FoeStatusText::POISONED:
        state.foe_status = PlannedStatus::POISON;
        return;
    case FoeStatusText::WOKE:
    case FoeStatusText::CURED:
        state.foe_status = PlannedStatus::NONE;
        state.foe_sleep_turns = 0;
        return;
    }
}

void set_weather(FieldWeather weather, int turns, BattleTrackState& state){
    state.weather = weather;
    state.weather_turns = turns;
}

//  Sunny Day's and Rain Dance's start, their end-of-turn reminder (when
//  the start was missed) and their end.
void read_weather(std::string_view letters, BattleTrackState& state){
    if (has(letters, "sunlightgotbright")){
        set_weather(FieldWeather::SUN, WEATHER_TURNS, state);
    }else if (has(letters, "sunlightisstrong")){
        set_weather(FieldWeather::SUN, std::max(1, state.weather == FieldWeather::SUN ? state.weather_turns : 0), state);
    }else if (has(letters, "startedtorain")){
        set_weather(FieldWeather::RAIN, WEATHER_TURNS, state);
    }else if (has(letters, "raincontinuestofall")){
        set_weather(FieldWeather::RAIN, std::max(1, state.weather == FieldWeather::RAIN ? state.weather_turns : 0), state);
    }else if (has(letters, "sunlightfaded") || has(letters, "rainstopped")){
        set_weather(FieldWeather::NONE, /*turns=*/0, state);
    }
}

//  The foe's moves that take a turn of charging, by their charge text.
void read_charge(std::string_view letters, BattleTrackState& state){
    if (has(letters, "tookinsunlight")){
        state.foe_charging = "solar-beam";
    }else if (has(letters, "underwater")){
        state.foe_charging = "dive";
    }
}

void apply_stat(const StatChange& change, BattleTrackState& state){
    //  Ours rise from our items only (item_used()).
    if (!change.foe && change.stages > 0){
        return;
    }
    int& stage = (change.foe ? state.foe : state.own).*change.stat;
    stage = std::clamp(stage + change.stages, -MAX_STAGE, MAX_STAGE);
}

void raise(int& stage){
    stage = std::min(MAX_STAGE, stage + 1);
}

} // namespace


void BattleTrack::read_turn(const std::vector<std::string>& texts){
    m_state.foe_recharging = false;
    m_state.foe_charging.reset();
    bool restored = false;
    //  A text read twice (the box polled again, the name misread) is one
    //  change: one side can't change one stat twice in a row in a turn.
    std::optional<StatChange> last_stat;
    std::optional<FoeStatusText> last_status;
    for (const std::string& letters : texts){
        //  pokefirered: "{trainer} used\nFULL RESTORE!"; the player's own
        //  medicine shows no such text. It cures the foe's status.
        if (!restored && has(letters, "usedfullrestore")){
            restored = true;
            m_state.trainer_full_restores++;
            apply_status(FoeStatusText::CURED, m_state);
        }
        m_state.foe_recharging = m_state.foe_recharging || has(letters, "usedhyperbeam");
        read_charge(letters, m_state);
        read_weather(letters, m_state);
        const std::optional<StatChange> stat = stat_change(letters);
        if (stat && stat != last_stat){
            apply_stat(*stat, m_state);
        }
        last_stat = stat;
        const std::optional<FoeStatusText> status = foe_status_text(letters);
        if (status && status != last_status){
            apply_status(*status, m_state);
        }
        last_status = status;
    }
}

void BattleTrack::item_used(std::string_view item){
    if (item == "x-special"){
        raise(m_state.own.sp_attack);
    }else if (item == "x-speed"){
        raise(m_state.own.speed);
    }else if (item == "x-accuracy"){
        raise(m_state.own.accuracy);
    }else if (item == "x-attack"){
        raise(m_state.own.attack);
    }else if (item == "x-defend"){
        raise(m_state.own.defense);
    }else if (item == "guard-spec"){
        m_state.mist_turns = MIST_TURNS;
    }
}

void BattleTrack::foe_out(size_t slot, std::string_view ability){
    if (m_foe_slot && *m_foe_slot != slot){
        m_state.foe = StatStages{};
        apply_status(FoeStatusText::CURED, m_state);
    }
    m_foe_slot = slot;
    if (m_intimidated_by == slot){
        return;
    }
    m_intimidated_by = slot;
    if (ability == "intimidate" && m_state.mist_turns == 0){
        m_state.own.attack = std::max(-MAX_STAGE, m_state.own.attack - 1);
    }
}

void BattleTrack::lead_switched_in(){
    m_state.own = StatStages{};
}

void BattleTrack::turn_chosen(){
    if (m_state.mist_turns > 0){
        m_state.mist_turns--;
    }
    if (m_state.weather_turns > 0 && --m_state.weather_turns == 0){
        m_state.weather = FieldWeather::NONE;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
