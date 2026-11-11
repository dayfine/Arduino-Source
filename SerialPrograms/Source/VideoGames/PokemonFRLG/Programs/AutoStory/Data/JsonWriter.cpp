/*  Legacy FRLG Checkpoint JSON Writer
 *  From: https://github.com/PokemonAutomation/
 */

#include <limits>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "LegacyJsonFormat.h"
#include "JsonWriter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

JsonArray strings(const std::vector<std::string>& values){
    JsonArray result;
    for (const std::string& value : values){
        result.push_back(value);
    }
    return result;
}

int64_t count(size_t value){
    if (value > (size_t)std::numeric_limits<int64_t>::max()){
        throw ParseException("FRLG checkpoint count exceeds JSON integer range.");
    }
    return (int64_t)value;
}

JsonObject stats_json(const Stats& stats){
    JsonObject result;
    result["hp"] = stats.hp;
    result["attack"] = stats.attack;
    result["defense"] = stats.defense;
    result["sp_attack"] = stats.sp_attack;
    result["sp_defense"] = stats.sp_defense;
    result["speed"] = stats.speed;
    return result;
}

JsonObject member_json(const PartyMember& member){
    JsonObject result;
    result["species"] = member.species;
    JsonArray moves;
    for (const KnownMove& move : member.moves){
        JsonObject entry;
        entry["move"] = move.move;
        entry["pp"] = move.pp;
        moves.push_back(std::move(entry));
    }
    result["moves"] = std::move(moves);
    result["moves_through_level"] = member.moves_through_level;
    result["moves_read"] = member.moves_read;
    result["level"] = member.level;
    result["stats"] = member.stats ? JsonValue(stats_json(*member.stats)) : JsonValue();
    return result;
}

JsonObject supplies_json(const Supplies& supplies){
    JsonObject result;
    result["potions"] = supplies.potions;
    result["antidotes"] = supplies.antidotes;
    result["parlyz_heals"] = supplies.parlyz_heals;
    result["pocket"] = strings(supplies.pocket);
    result["status"] = std::string(status_name(supplies.status));
    result["hurt"] = supplies.hurt;
    result["hp_red"] = supplies.hp_red;
    result["hp_fill"] = supplies.hp_fill;
    result["lead"] = member_json(supplies.lead);
    JsonArray protected_slots;
    for (size_t slot : supplies.protected_slots){
        protected_slots.push_back(count(slot));
    }
    result["protected_slots"] = std::move(protected_slots);
    result["foes"] = strings(supplies.foes);
    result["trainer"] = supplies.trainer;
    result["risk"] = std::string(battle_risk_name(supplies.risk));
    result["foe_out"] = supplies.foe_out.empty() ? JsonValue() : JsonValue(supplies.foe_out);
    result["wild_foes"] = strings(supplies.wild_foes);
    return result;
}


} // namespace

JsonValue story_checkpoint_export(const StoryCheckpoint& checkpoint){
    JsonObject result;
    result["format"] = std::string(StoryJsonInternal::FORMAT);
    result["game"] = std::string(StoryJsonInternal::GAME);
    result["status"] = std::string(StoryJsonInternal::STATUS);
    JsonObject native;
    native["menu"] = std::string(StoryJsonInternal::menu_name(checkpoint.menu));
    native["supplies"] = supplies_json(checkpoint.supplies);
    result["native_fields"] = std::move(native);
    result["state_basis"] = std::string(StoryJsonInternal::STATE_BASIS);
    result["unbound"] = strings(StoryJsonInternal::unbound_fields());
    return result;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
