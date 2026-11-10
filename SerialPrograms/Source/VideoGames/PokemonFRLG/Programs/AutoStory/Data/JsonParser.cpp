/*  FRLG Checkpoint JSON Parser
 *  From: https://github.com/PokemonAutomation/
 */

#include <algorithm>
#include <format>
#include <initializer_list>
#include <limits>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Pokemon_AuthoredCheckpointJson.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryChoice.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGrind.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryHideout.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryItemUse.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMart.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMember.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryParty.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPcStorage.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRooms.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTeach.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTrashCans.h"
#include "LegacyJsonFormat.h"
#include "JsonParser.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Reading. Every field is required and no other is allowed, as in the
//  shared codec: the record says what the table said, not what it left out.
const JsonObject& fields(const JsonValue& value, std::initializer_list<std::string_view> keys, std::string_view source){
    const JsonObject& object = value.to_object_throw(std::string(source));
    for (const auto& [key, unused] : object){
        bool known = false;
        for (std::string_view allowed : keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("{}: unsupported field {}", source, key));
        }
    }
    return object;
}

void expect_string(const JsonObject& root, const std::string& key, std::string_view expected, std::string_view source){
    const std::string& actual = root.get_string_throw(key, std::string(source));
    if (actual != expected){
        throw ParseException(std::format("{}: {} is {}, not {}", source, key, actual, expected));
    }
}

//  The value of `values` that `name` names `label`.
template <typename Enum>
Enum read_named(std::initializer_list<Enum> values, const char* (*name)(Enum), const std::string& label, std::string_view source){
    for (Enum value : values){
        if (label == name(value)){
            return value;
        }
    }
    throw ParseException(std::format("{}: unsupported value {}", source, label));
}

//  An integer within [`low`, `high`], as the native field's type holds it.
int64_t bounded(const JsonValue& value, int64_t low, int64_t high, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < low || number > high){
        throw ParseException(std::format("{}: {} is out of range", source, number));
    }
    return number;
}

int read_int(const JsonValue& value, std::string_view source){
    return (int)bounded(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), source);
}

uint8_t read_byte(const JsonValue& value, std::string_view source){
    return (uint8_t)bounded(value, 0, std::numeric_limits<uint8_t>::max(), source);
}

uint16_t read_word(const JsonValue& value, std::string_view source){
    return (uint16_t)bounded(value, 0, std::numeric_limits<uint16_t>::max(), source);
}

size_t read_count(const JsonValue& value, std::string_view source){
    return (size_t)bounded(value, 0, std::numeric_limits<int64_t>::max(), source);
}

std::vector<std::string> read_strings(const JsonValue& value, std::string_view source){
    std::vector<std::string> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(item.to_string_throw(std::string(source)));
    }
    return result;
}

std::optional<Stats> read_stats(const JsonValue& value, std::string_view source){
    if (value.is_null()){
        return std::nullopt;
    }
    const JsonObject& root = fields(value, {"hp", "attack", "defense", "sp_attack", "sp_defense", "speed"}, source);
    return Stats{
        .hp = read_word(root.get_value_throw("hp"), source),
        .attack = read_word(root.get_value_throw("attack"), source),
        .defense = read_word(root.get_value_throw("defense"), source),
        .sp_attack = read_word(root.get_value_throw("sp_attack"), source),
        .sp_defense = read_word(root.get_value_throw("sp_defense"), source),
        .speed = read_word(root.get_value_throw("speed"), source),
    };
}

std::vector<KnownMove> read_moves(const JsonValue& value, std::string_view source){
    std::vector<KnownMove> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = fields(item, {"move", "pp"}, source);
        result.push_back({.move = row.get_string_throw("move"), .pp = read_byte(row.get_value_throw("pp"), source)});
    }
    return result;
}

PartyMember read_member(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"species", "moves", "moves_through_level", "moves_read", "level", "stats"}, source);
    return PartyMember{
        .species = root.get_string_throw("species"),
        .moves = read_moves(root.get_value_throw("moves"), source),
        .moves_through_level = read_byte(root.get_value_throw("moves_through_level"), source),
        .moves_read = root.get_boolean_throw("moves_read"),
        .level = read_byte(root.get_value_throw("level"), source),
        .stats = read_stats(root.get_value_throw("stats"), source),
    };
}

std::vector<size_t> read_slots(const JsonValue& value, std::string_view source){
    std::vector<size_t> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        result.push_back(read_count(item, source));
    }
    return result;
}

std::optional<size_t> read_foe_out(const JsonValue& value, std::string_view source){
    if (value.is_null()){
        return std::nullopt;
    }
    return read_count(value, source);
}

Supplies read_supplies(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"potions", "antidotes", "parlyz_heals", "pocket", "status", "hurt", "hp_red",
        "hp_fill", "lead", "protected_slots", "foes", "trainer", "risk", "foe_out", "wild_foes"}, source);
    Supplies result;
    result.potions = read_int(root.get_value_throw("potions"), source);
    result.antidotes = read_int(root.get_value_throw("antidotes"), source);
    result.parlyz_heals = read_int(root.get_value_throw("parlyz_heals"), source);
    result.pocket = read_strings(root.get_value_throw("pocket"), source);
    result.status = read_named({StatusCondition::NONE, StatusCondition::POISON, StatusCondition::PARALYSIS, StatusCondition::OTHER},
        status_name, root.get_string_throw("status"), source);
    result.hurt = root.get_boolean_throw("hurt");
    result.hp_red = root.get_boolean_throw("hp_red");
    result.hp_fill = root.get_double_throw("hp_fill");
    result.lead = read_member(root.get_value_throw("lead"), source);
    result.protected_slots = read_slots(root.get_value_throw("protected_slots"), source);
    result.foes = read_strings(root.get_value_throw("foes"), source);
    result.trainer = root.get_string_throw("trainer");
    result.risk = read_named({BattleRisk::LOW, BattleRisk::HIGH}, battle_risk_name, root.get_string_throw("risk"), source);
    result.foe_out = read_foe_out(root.get_value_throw("foe_out"), source);
    result.wild_foes = read_strings(root.get_value_throw("wild_foes"), source);
    return result;
}

void check_envelope(const JsonObject& root, std::string_view source){
    expect_string(root, "format", StoryJsonInternal::FORMAT, source);
    expect_string(root, "game", StoryJsonInternal::GAME, source);
    expect_string(root, "status", StoryJsonInternal::STATUS, source);
    expect_string(root, "state_basis", StoryJsonInternal::STATE_BASIS, source);
    if (read_strings(root.get_value_throw("unbound"), source) != StoryJsonInternal::unbound_fields()){
        throw ParseException(std::format("{}: unbound is not the export's list", source));
    }
    if (root.get_string_throw("source_revision").empty()){
        throw ParseException(std::format("{}: source_revision names no revision", source));
    }
}

constexpr std::pair<StoryHandoff, std::string_view> HANDOFFS[]{
    {StoryHandoff::NONE, "none"},
    {StoryHandoff::SID_HELPER, "sid_helper"},
    {StoryHandoff::STARTER_RNG, "starter_rng"},
    {StoryHandoff::GIFT_RNG, "gift_rng"},
};

//  The healing items a checkpoint starts with, and its lead: a new game's
//  unless "lead" names the one after Brock (story_lead_after_brock()).
Supplies read_authored_supplies(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value, {"potions", "antidotes", "parlyz_heals", "lead"}, source);
    Supplies result;
    if (const JsonValue* field = root.get_value("potions")){
        result.potions = read_int(*field, source);
    }
    if (const JsonValue* field = root.get_value("antidotes")){
        result.antidotes = read_int(*field, source);
    }
    if (const JsonValue* field = root.get_value("parlyz_heals")){
        result.parlyz_heals = read_int(*field, source);
    }
    if (const JsonValue* field = root.get_value("lead")){
        if (field->to_string_throw(std::string(source)) != "after_brock"){
            throw ParseException(std::format("{}: unsupported lead {}", source, field->to_string_throw()));
        }
        result.lead = story_lead_after_brock();
    }
    return result;
}

//  Whether `step` is a GAME step FRLG's executor does, with args its
//  action reads.
bool known_game_step(const StoryStep& step){
    for (const std::string& name : {CLOSE_TRAINER_CARD, LEAVE_GIFT_SUMMARY, CUT_TREE_VARIANT, TRASH_CANS_VARIANT}){
        if (step.variant == name){
            return step.args.empty();
        }
    }
    return parse_capture_step(step) || parse_pc_step(step) || parse_mart_step(step)
        || parse_teach_step(step) || parse_grind_step(step) || parse_grind_check_step(step)
        || parse_fossil_step(step) || parse_level_step(step)
        || (is_center_variant(step.variant) && step.args.empty()) || is_item_use_step(step) || is_hideout_step(step);
}

//  Whether FRLG does `step`'s kind the way its variant names.
bool known_variant(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::ROUTE:
        return step.variant.empty() || step.variant == RUN_VARIANT || step.variant == BIKE_VARIANT;
    case StoryStepKind::LEGS:
        return step.variant.empty() || step.variant == RUN_VARIANT || step.variant == BIKE_VARIANT || step.variant == TAPS_VARIANT;
    case StoryStepKind::STEP:
        return step.variant.empty() || step.variant == TRAINER_LINE_VARIANT || step.variant == TRAINER_WAIT_VARIANT;
    case StoryStepKind::GAME:
        return known_game_step(step);
    default:
        return step.variant.empty();
    }
}

//  The checkpoint's steps against what FRLG can do: each variant known,
//  and nothing lint_story_checkpoint() finds.
void check_steps(const StoryCheckpoint& checkpoint, std::string_view source){
    for (const StoryStep& step : checkpoint.steps){
        if (!known_variant(step)){
            throw ParseException(std::format("{}: {} has an unsupported variant \"{}\"", source, step.name, step.variant));
        }
    }
    const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(checkpoint);
    if (!problems.empty()){
        throw ParseException(std::format("{}: {}", source, problems.front()));
    }
}

StoryCheckpoint apply_execution(Pokemon::AuthoredCheckpoint parsed, std::string_view source){
    const JsonObject& execution = parsed.execution;
    for (const auto& [key, unused] : execution){
        if (key != "menu" && key != "supplies" && key != "live_start" && key != "handoff"){
            throw ParseException(std::format("{}: unsupported execution field {}", source, key));
        }
    }
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = std::move(parsed.checkpoint);
    result.menu = read_named({StartMenu::BEFORE_POKEMON, StartMenu::BEFORE_POKEDEX, StartMenu::FULL},
        StoryJsonInternal::menu_name, execution.get_string_throw("menu", std::string(source)), source);
    if (const JsonValue* field = execution.get_value("supplies")){
        result.supplies = read_authored_supplies(*field, source);
    }
    if (const JsonValue* field = execution.get_value("live_start")){
        result.live_start = field->to_boolean_throw(std::string(source));
    }
    if (const JsonValue* field = execution.get_value("handoff")){
        const std::string& name = field->to_string_throw(std::string(source));
        bool found = false;
        for (const auto& [handoff, label] : HANDOFFS){
            if (name == label){
                result.handoff = handoff;
                found = true;
            }
        }
        if (!found){
            throw ParseException(std::format("{}: unsupported handoff {}", source, name));
        }
    }
    check_steps(result, source);
    return result;
}

//  Whether two leads are the same as far as a checkpoint's row sets them.
bool same_lead(const PartyMember& a, const PartyMember& b){
    if (a.species != b.species || a.level != b.level || a.moves_through_level != b.moves_through_level
        || a.moves_read != b.moves_read || a.moves.size() != b.moves.size()
    ){
        return false;
    }
    for (size_t c = 0; c < a.moves.size(); c++){
        if (a.moves[c].move != b.moves[c].move || a.moves[c].pp != b.moves[c].pp){
            return false;
        }
    }
    return true;
}

//  The supplies as authored: what differs from a new game's.
JsonObject authored_supplies_json(const Supplies& supplies){
    JsonObject result;
    if (supplies.potions){
        result["potions"] = supplies.potions;
    }
    if (supplies.antidotes){
        result["antidotes"] = supplies.antidotes;
    }
    if (supplies.parlyz_heals){
        result["parlyz_heals"] = supplies.parlyz_heals;
    }
    if (same_lead(supplies.lead, story_lead_after_brock())){
        result["lead"] = "after_brock";
    }else if (!same_lead(supplies.lead, Supplies{}.lead)){
        throw ParseException("Only a new game's lead or the lead after Brock can be authored.");
    }
    return result;
}


} // namespace

StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    return apply_execution(Pokemon::read_authored_checkpoint(value, "frlg", source), source);
}

StoryCheckpoint load_authored_checkpoint(std::string_view file){
    return apply_execution(Pokemon::load_authored_checkpoint(autostory_checkpoint_json(), file, "frlg"), file);
}

std::vector<StoryCheckpoint> load_authored_story(){
    std::vector<StoryCheckpoint> result;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name.ends_with(".plan")){
            result.push_back(read_authored_checkpoint(parse_json(text), name));
        }
    }
    std::sort(result.begin(), result.end(), [](const StoryCheckpoint& a, const StoryCheckpoint& b){
        return a.id < b.id;
    });
    for (size_t c = 1; c < result.size(); c++){
        if (result[c].id == result[c - 1].id){
            throw ParseException("Two authored checkpoints are " + result[c].id + ".");
        }
    }
    return result;
}

JsonValue authored_checkpoint_json(const StoryCheckpoint& checkpoint){
    JsonObject execution;
    execution["menu"] = StoryJsonInternal::menu_name(checkpoint.menu);
    JsonObject supplies = authored_supplies_json(checkpoint.supplies);
    if (supplies.size() != 0){
        execution["supplies"] = std::move(supplies);
    }
    if (checkpoint.live_start){
        execution["live_start"] = true;
    }
    if (checkpoint.handoff != StoryHandoff::NONE){
        for (const auto& [handoff, label] : HANDOFFS){
            if (handoff == checkpoint.handoff){
                execution["handoff"] = std::string(label);
            }
        }
    }
    return Pokemon::authored_checkpoint_json(checkpoint, "frlg", std::move(execution));
}

StoryCheckpoint read_story_checkpoint_export(const JsonValue& value, std::string_view source){
    const JsonObject& root = fields(value,
        {"format", "game", "status", "checkpoint", "native_fields", "state_basis", "unbound", "source_revision"}, source);
    check_envelope(root, source);
    StoryCheckpoint result;
    Pokemon::StoryCheckpoint& common = result;
    common = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), source);
    const JsonObject& native = fields(root.get_value_throw("native_fields"), {"menu", "supplies"}, source);
    result.menu = read_named({StartMenu::BEFORE_POKEMON, StartMenu::BEFORE_POKEDEX, StartMenu::FULL},
        StoryJsonInternal::menu_name, native.get_string_throw("menu"), source);
    result.supplies = read_supplies(native.get_value_throw("supplies"), source);
    return result;
}

StoryCheckpoint load_story_checkpoint(std::string_view file){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == file){
            return read_story_checkpoint_export(parse_json(text), std::format("StoryData/{}.json", file));
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("StoryData/{}.json is not built into the program.", file));
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
