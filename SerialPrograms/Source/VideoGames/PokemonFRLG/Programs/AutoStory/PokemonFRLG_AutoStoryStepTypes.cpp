/*  AutoStory Step Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <initializer_list>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view CARD_KEY_HELD_FLAG = "card_key_held";

std::string read_string_field(const JsonObject& obj, const std::string& key, std::string_view source){
    return obj.get_string_throw(key, std::string(source));
}

std::string read_string_default(const JsonObject& obj, const std::string& key, std::string fallback = {}){
    const JsonValue* value = obj.get_value(key);
    if (value == nullptr){
        return fallback;
    }
    return value->to_string_throw(key);
}

int64_t read_int_field(const JsonObject& obj, const std::string& key, std::string_view source){
    return obj.get_integer_throw(key, std::string(source));
}

int64_t read_int_default(const JsonObject& obj, const std::string& key, int64_t fallback){
    const JsonValue* value = obj.get_value(key);
    if (value == nullptr){
        return fallback;
    }
    return value->to_integer_throw(key);
}

bool read_bool_field(const JsonObject& obj, const std::string& key, std::string_view source){
    return obj.get_boolean_throw(key, std::string(source));
}

bool read_bool_default(const JsonObject& obj, const std::string& key, bool fallback){
    const JsonValue* value = obj.get_value(key);
    if (value == nullptr){
        return fallback;
    }
    return value->to_boolean_throw(key);
}

std::vector<std::string> read_words_value(const JsonValue& value, std::string_view source){
    std::vector<std::string> out;
    for (const JsonValue& entry : value.to_array_throw(std::string(source))){
        out.push_back(entry.to_string_throw(std::string(source)));
    }
    return out;
}

std::vector<std::vector<int64_t>> read_int_rows_value(const JsonValue& value, std::string_view source){
    std::vector<std::vector<int64_t>> out;
    for (const JsonValue& row : value.to_array_throw(std::string(source))){
        std::vector<int64_t> nums;
        for (const JsonValue& cell : row.to_array_throw(std::string(source))){
            nums.push_back(cell.to_integer_throw(std::string(source)));
        }
        out.push_back(std::move(nums));
    }
    return out;
}

Pokemon::Direction read_direction_field(const JsonObject& obj, const std::string& key, std::string_view source){
    return Pokemon::read_direction(obj.get_string_throw(key, std::string(source)), source);
}

Pokemon::Direction read_direction_default(
    const JsonObject& obj, const std::string& key, Pokemon::Direction fallback, std::string_view source
){
    const JsonValue* value = obj.get_value(key);
    if (value == nullptr){
        return fallback;
    }
    return Pokemon::read_direction(value->to_string_throw(std::string(source)), source);
}

//  Rejects any key in `args` not in `action_keys` or the common FRLG step
//  metadata keys (`pose_after`, `skip_if`, `gates_after`, `flags_after`).
void check_allowed_args(const JsonObject& args, std::initializer_list<std::string_view> action_keys){
    for (const auto& [key, unused] : args){
        bool known = key == "pose_after" || key == "skip_if" || key == "gates_after" || key == "flags_after";
        for (std::string_view allowed : action_keys){
            known |= key == allowed;
        }
        if (!known){
            throw ParseException(std::format("unknown field \"{}\"", key));
        }
    }
}

void parse_common_step_args(const JsonObject& args, std::string_view source, StoryStep& step){
    if (const JsonValue* pose = args.get_value("pose_after")){
        step.pose_after = Pokemon::read_grid_pose(*pose, source);
    }
    if (const JsonValue* skip = args.get_value("skip_if")){
        if (const std::string* s = skip->to_string()){
            step.skip_if = *s;
        }else{
            const JsonArray& arr = skip->to_array_throw(std::string(source));
            if (arr.size() != 1){
                throw ParseException(std::format("{}: skip_if must have 1 flag", source));
            }
            step.skip_if = arr[0].to_string_throw(std::string(source));
        }
    }
    if (const JsonValue* gates = args.get_value("gates_after")){
        step.gates_after = read_gate_change(*gates, source);
    }
    if (const JsonValue* flags = args.get_value("flags_after")){
        step.flags_after = read_words_value(*flags, source);
    }
}

void check_variant(std::string_view action, std::string_view variant, std::initializer_list<std::string_view> allowed){
    if (variant.empty()){
        return;
    }
    for (std::string_view candidate : allowed){
        if (variant == candidate){
            return;
        }
    }
    throw ParseException(std::format("action \"{}\": unknown variant \"{}\"", action, variant));
}

void parse_battle_args(const JsonObject& args, std::string_view source, FrlgArgMap& out){
    check_allowed_args(args, {
        "may_lose", "trainer", "foes", "foe_stats", "use_items", "run",
        "policy", "setup_spa", "setup_accuracy", "setup_outspeed",
        "setup_guard_accuracy", "setup_torrent_power", "setup_target_torrent", "foe_threats",
    });
    out["may_lose"] = read_bool_default(args, "may_lose", false);
    out["trainer"] = read_string_default(args, "trainer");
    out["foes"] = args.get_value("foes") ? read_words_value(*args.get_value("foes"), source) : std::vector<std::string>{};
    out["foe_stats"] = args.get_value("foe_stats")
        ? read_int_rows_value(*args.get_value("foe_stats"), source)
        : std::vector<std::vector<int64_t>>{};
    out["use_items"] = args.get_value("use_items")
        ? Pokemon::read_story_items(*args.get_value("use_items"), source)
        : std::vector<StoryItem>{};
    out["run"] = read_bool_default(args, "run", false);
    out["policy"] = read_string_default(args, "policy");
    out["setup_spa"] = read_int_default(args, "setup_spa", 0);
    out["setup_accuracy"] = read_int_default(args, "setup_accuracy", 0);
    out["setup_outspeed"] = read_bool_default(args, "setup_outspeed", false);
    out["setup_guard_accuracy"] = read_bool_default(args, "setup_guard_accuracy", false);
    out["setup_torrent_power"] = read_bool_default(args, "setup_torrent_power", false);
    out["setup_target_torrent"] = read_bool_default(args, "setup_target_torrent", false);
    out["foe_threats"] = args.get_value("foe_threats")
        ? read_int_rows_value(*args.get_value("foe_threats"), source)
        : std::vector<std::vector<int64_t>>{};
}

void parse_pc_args(const JsonObject& args, std::string_view source, FrlgArgMap& out){
    check_allowed_args(args, {"do", "slot", "species", "box", "if_present", "protect"});
    out["do"] = read_string_field(args, "do", source);
    if (const JsonValue* slot = args.get_value("slot")){
        out["slot"] = slot->to_integer_throw(std::string(source));
    }
    if (const JsonValue* species = args.get_value("species")){
        out["species"] = species->to_string_throw(std::string(source));
    }
    out["box"] = read_int_field(args, "box", source);
    out["if_present"] = read_bool_default(args, "if_present", false);
    out["protect"] = read_bool_default(args, "protect", false);
}

void parse_capture_args(const JsonObject& args, std::string_view source, FrlgArgMap& out){
    check_allowed_args(args, {"species", "catches", "encounters", "balls", "pace", "move", "turns"});
    out["species"] = read_string_field(args, "species", source);
    out["catches"] = read_int_default(args, "catches", 1);
    out["encounters"] = read_int_default(args, "encounters", 20);
    out["balls"] = read_int_default(args, "balls", 5);
    out["pace"] = read_direction_default(args, "pace", Pokemon::Direction::EAST, source);
    if (const JsonValue* move = args.get_value("move")){
        out["move"] = move->to_integer_throw(std::string(source));
    }
    out["turns"] = read_int_default(args, "turns", 2);
}

void parse_mart_args(const JsonObject& args, std::string_view source, FrlgArgMap& out){
    check_allowed_args(args, {"map", "items", "fill_to", "if_affordable", "sell"});
    out["map"] = read_string_field(args, "map", source);
    out["items"] = Pokemon::read_story_items(args.get_value_throw("items", std::string(source)), source);
    if (const JsonValue* fill = args.get_value("fill_to")){
        out["fill_to"] = fill->to_integer_throw(std::string(source));
    }
    if (const JsonValue* affordable = args.get_value("if_affordable")){
        out["if_affordable"] = affordable->to_boolean_throw(std::string(source));
    }
    if (const JsonValue* sell = args.get_value("sell")){
        out["sell"] = Pokemon::read_story_items(*sell, source);
    }
}

void parse_teach_args(const JsonObject& args, std::string_view source, FrlgArgMap& out){
    check_allowed_args(args, {"item", "move", "slot", "species", "forget", "or_forget", "keep", "forget_slot"});
    out["item"] = read_string_field(args, "item", source);
    out["move"] = read_string_field(args, "move", source);
    if (const JsonValue* slot = args.get_value("slot")){
        out["slot"] = slot->to_integer_throw(std::string(source));
    }
    if (const JsonValue* species = args.get_value("species")){
        out["species"] = species->to_string_throw(std::string(source));
    }
    if (const JsonValue* forget = args.get_value("forget")){
        out["forget"] = forget->to_string_throw(std::string(source));
    }
    if (const JsonValue* or_forget = args.get_value("or_forget")){
        out["or_forget"] = read_words_value(*or_forget, source);
    }
    if (const JsonValue* keep = args.get_value("keep")){
        out["keep"] = read_words_value(*keep, source);
    }
    if (const JsonValue* forget_slot = args.get_value("forget_slot")){
        out["forget_slot"] = forget_slot->to_integer_throw(std::string(source));
    }
}

void parse_grind_args(const JsonObject& args, std::string_view source, bool with_nurse, FrlgArgMap& out){
    if (with_nurse){
        check_allowed_args(args, {"slot", "level", "encounters", "pace", "heal_every", "nurse"});
        out["nurse"] = Pokemon::read_grid_pose(args.get_value_throw("nurse", std::string(source)), source);
    }else{
        check_allowed_args(args, {"slot", "level", "encounters", "pace", "heal_every"});
    }
    out["slot"] = read_int_field(args, "slot", source);
    out["level"] = read_int_field(args, "level", source);
    out["encounters"] = read_int_default(args, "encounters", 60);
    out["pace"] = read_direction_default(args, "pace", Pokemon::Direction::EAST, source);
    out["heal_every"] = read_int_default(args, "heal_every", 0);
}

bool parse_field_move_args(std::string_view action, const JsonObject& args, std::string_view source, FrlgArgMap& out){
    if (action == "cut_tree" || action == "surf"){
        check_allowed_args(args, {"direction"});
        out["direction"] = read_direction_field(args, "direction", source);
        return true;
    }
    if (action == "push_boulder"){
        check_allowed_args(args, {"direction", "tiles", "activate"});
        out["direction"] = read_direction_field(args, "direction", source);
        out["tiles"] = read_int_default(args, "tiles", 1);
        out["activate"] = read_bool_default(args, "activate", false);
        return true;
    }
    if (action == "lift_floor"){
        check_allowed_args(args, {"from", "to"});
        out["from"] = read_string_field(args, "from", source);
        out["to"] = read_string_field(args, "to", source);
        return true;
    }
    if (action == "use_item"){
        check_allowed_args(args, {"item", "safari", "count", "if_any"});
        out["item"] = read_string_field(args, "item", source);
        out["safari"] = read_bool_default(args, "safari", false);
        out["count"] = read_int_default(args, "count", 1);
        out["if_any"] = read_bool_default(args, "if_any", false);
        return true;
    }
    if (action == "use_repel"){
        check_allowed_args(args, {"safari", "if_any"});
        out["safari"] = read_bool_default(args, "safari", false);
        out["if_any"] = read_bool_default(args, "if_any", false);
        return true;
    }
    if (action == "fly"){
        check_allowed_args(args, {"town", "slot", "species", "menu_row", "safari"});
        out["town"] = read_string_field(args, "town", source);
        if (const JsonValue* slot = args.get_value("slot")){
            out["slot"] = slot->to_integer_throw(std::string(source));
        }
        if (const JsonValue* species = args.get_value("species")){
            out["species"] = species->to_string_throw(std::string(source));
        }
        out["menu_row"] = read_int_default(args, "menu_row", 2);
        out["safari"] = read_bool_default(args, "safari", false);
        return true;
    }
    return false;
}

bool parse_misc_action_args(std::string_view action, const JsonObject& args, std::string_view source, FrlgArgMap& out){
    if (action == "fossil"){
        check_allowed_args(args, {"fossil"});
        out["fossil"] = read_string_field(args, "fossil", source);
        return true;
    }
    if (action == "level"){
        check_allowed_args(args, {"slot", "min"});
        out["slot"] = read_int_field(args, "slot", source);
        out["min"] = read_int_field(args, "min", source);
        return true;
    }
    if (action == "nugget_farm"){
        check_allowed_args(args, {"count", "nurse"});
        out["count"] = read_int_field(args, "count", source);
        out["nurse"] = Pokemon::read_grid_pose(args.get_value_throw("nurse", std::string(source)), source);
        return true;
    }
    if (action == "slope"){
        check_allowed_args(args, {"moves"});
        out["moves"] = read_words_value(args.get_value_throw("moves", std::string(source)), source);
        return true;
    }
    if (action == "count_items"){
        check_allowed_args(args, {"items", "found"});
        out["items"] = read_words_value(args.get_value_throw("items", std::string(source)), source);
        out["found"] = read_bool_default(args, "found", false);
        return true;
    }
    return false;
}

void parse_league_action_args(std::string_view action, const JsonObject& args, std::string_view source, FrlgArgMap& out){
    if (action == "heal_to_full"){
        check_allowed_args(args, {"max_hp", "full_restore", "below_percent"});
        out["max_hp"] = read_int_field(args, "max_hp", source);
        out.emplace("full_restore", read_bool_default(args, "full_restore", false));
        out["below_percent"] = read_int_default(args, "below_percent", 100);
        return;
    }
    if (action == "restore_pp"){
        check_allowed_args(args, {"item"});
        out["item"] = read_string_field(args, "item", source);
        return;
    }
    if (action == "check_full_hp"){
        check_allowed_args(args, {"max_hp"});
        out["max_hp"] = read_int_field(args, "max_hp", source);
        return;
    }
    if (action == "bag_check"){
        check_allowed_args(args, {"items", "any"});
        out["items"] = Pokemon::read_story_items(args.get_value_throw("items", std::string(source)), source);
        out["any"] = read_bool_default(args, "any", false);
        return;
    }
    if (action == "league_top_up"){
        check_allowed_args(args, {"fight", "max_hp", "full_if_unknown"});
        out["fight"] = read_string_field(args, "fight", source);
        out["max_hp"] = read_int_field(args, "max_hp", source);
        out["full_if_unknown"] = read_bool_default(args, "full_if_unknown", false);
        return;
    }
    throw ParseException(std::format("unknown FRLG action \"{}\"", action));
}

bool parse_simple_action_step(std::string_view action, const JsonObject& args, StoryStep& step){
    static const std::set<std::string_view> NO_ARGS{
        "new_game", "options", "scene", "heal", "close_trainer_card", "leave_gift_summary",
        "trash_cans", "poster_switch", "ride_bike", "register_bike", "center_enter",
        "center_exit", "hall_of_fame",
    };
    if (NO_ARGS.contains(action)){
        check_allowed_args(args, {});
        return true;
    }
    if (action == "talk"){
        check_allowed_args(args, {"variant"});
        step.variant = read_string_default(args, "variant");
        check_variant(action, step.variant, {"wanderer"});
        return true;
    }
    if (action == "route"){
        check_allowed_args(args, {"target", "fight_wild", "variant"});
        step.variant = read_string_default(args, "variant");
        check_variant(action, step.variant, {"run", "bike"});
        step.args["target"] = Pokemon::read_grid_pose(args.get_value_throw("target", "route"), "route");
        step.args["fight_wild"] = read_bool_default(args, "fight_wild", false);
        return true;
    }
    if (action == "legs"){
        check_allowed_args(args, {"legs", "fight_wild", "variant"});
        step.variant = read_string_default(args, "variant");
        check_variant(action, step.variant, {"run", "bike", "taps"});
        step.args["legs"] = Pokemon::read_story_legs(args.get_value_throw("legs", "legs"), "legs");
        step.args["fight_wild"] = read_bool_default(args, "fight_wild", false);
        return true;
    }
    if (action == "step"){
        check_allowed_args(args, {"direction", "variant"});
        step.variant = read_string_default(args, "variant");
        check_variant(action, step.variant, {"trainer_line", "trainer_wait"});
        step.args["direction"] = read_direction_field(args, "direction", "step");
        return true;
    }
    return false;
}

std::optional<Pokemon::StoryCheckKind> frlg_action_check_kind(std::string_view action){
    if (action == "level"){
        return Pokemon::StoryCheckKind::PARTY_STATS;
    }
    return std::nullopt;
}

bool parse_specialized_action_args(std::string_view action, const JsonObject& args, FrlgArgMap& out){
    if (action == "battle"){
        parse_battle_args(args, action, out);
        return true;
    }
    if (action == "pc"){
        parse_pc_args(args, action, out);
        return true;
    }
    if (action == "capture"){
        parse_capture_args(args, action, out);
        return true;
    }
    if (action == "mart"){
        parse_mart_args(args, action, out);
        return true;
    }
    if (action == "teach"){
        parse_teach_args(args, action, out);
        return true;
    }
    if (action == "grind" || action == "grind_check"){
        parse_grind_args(args, action, action == "grind", out);
        return true;
    }
    return false;
}

StoryStep parse_frlg_step(std::string_view action, const JsonObject& args){
    StoryStep step;
    step.action = std::string(action);
    step.check = frlg_action_check_kind(action);
    parse_common_step_args(args, action, step);

    if (parse_simple_action_step(action, args, step)){
        return step;
    }
    if (action == "answer"){
        check_allowed_args(args, {"yes"});
        step.args["yes"] = read_bool_default(args, "yes", true);
        return step;
    }
    if (action == "shop"){
        check_allowed_args(args, {"items"});
        step.args["items"] = Pokemon::read_story_items(args.get_value_throw("items", "shop"), "shop");
        return step;
    }
    if (action == "ready"){
        check_allowed_args(args, {"target", "fight_wild"});
        step.args["target"] = Pokemon::read_grid_pose(args.get_value_throw("target", "ready"), "ready");
        step.args["fight_wild"] = read_bool_default(args, "fight_wild", false);
        return step;
    }
    if (parse_specialized_action_args(action, args, step.args)){
        return step;
    }
    if (!parse_field_move_args(action, args, action, step.args)
        && !parse_misc_action_args(action, args, action, step.args)
    ){
        parse_league_action_args(action, args, action, step.args);
    }
    return step;
}

} // namespace


Pokemon::GateChange read_gate_change(const JsonValue& value, std::string_view source){
    const JsonObject& obj = value.to_object_throw(std::string(source));
    Pokemon::reject_unknown_keys(obj, {"opened", "switches"});
    Pokemon::GateChange change;
    if (const JsonValue* opened = obj.get_value("opened")){
        for (const JsonValue& gate : opened->to_array_throw(std::string(source))){
            change.opened.insert(gate.to_string_throw(std::string(source)));
        }
    }
    if (const JsonValue* switches = obj.get_value("switches")){
        for (const auto& [name, state] : switches->to_object_throw(std::string(source))){
            change.switches.emplace(name, state.to_boolean_throw(std::string(source)));
        }
    }
    return change;
}

Pokemon::StoryFailure frlg_unsupported_step(const StoryStep& step){
    const std::string what = step.variant.empty()
        ? step.action
        : std::format("{}({})", step.action, step.variant);
    return frlg_failure("unsupported_step", std::format("{}: {}", step.name, what), true);
}

Pokemon::GridPose to_grid_pose(const StoryPose& pose){
    return Pokemon::GridPose{
        .map = pose.map,
        .tiles = {pose.tile},
        .facing = pose.facing,
    };
}

std::optional<StoryPose> story_pose(const Pokemon::GridPose& pose){
    if (pose.map.empty() || pose.tiles.size() != 1){
        return std::nullopt;
    }
    return StoryPose{pose.map, pose.tiles[0], pose.facing};
}

std::optional<StoryPose> story_pose(const std::optional<Pokemon::GridPose>& pose){
    if (!pose){
        return std::nullopt;
    }
    return story_pose(*pose);
}

std::string format_grid_pose(const Pokemon::GridPose& pose){
    if (pose.map.empty()){
        return "unknown";
    }
    std::string text = pose.map;
    if (pose.tiles.size() == 1){
        text += " " + Pokemon::to_string(pose.tiles[0]);
    }else if (!pose.tiles.empty()){
        text += " {";
        for (size_t i = 0; i < pose.tiles.size(); i++){
            text += (i == 0 ? "" : ", ") + Pokemon::to_string(pose.tiles[i]);
        }
        text += "}";
    }
    if (pose.facing){
        text += std::format(" facing {}", Pokemon::direction_name(*pose.facing));
    }
    return text;
}

std::string format_grid_pose(const std::optional<Pokemon::GridPose>& pose){
    return pose ? format_grid_pose(*pose) : "unknown";
}

Pokemon::GateRules frlg_gate_rules(const FrlgRunState& state){
    return Pokemon::GateRules{.card_key_held = state.flags.contains(std::string(CARD_KEY_HELD_FLAG))};
}

bool is_core_action(std::string_view action){
    static const std::set<std::string_view> CORE{
        "new_game", "options", "route", "legs", "step", "scene",
        "talk", "answer", "battle", "shop", "heal", "ready",
    };
    return CORE.contains(action);
}

const std::vector<std::string>& frlg_action_names(){
    static const std::vector<std::string> NAMES{
        "answer",
        "bag_check",
        "battle",
        "capture",
        "center_enter",
        "center_exit",
        "check_full_hp",
        "close_trainer_card",
        "count_items",
        "cut_tree",
        "fly",
        "fossil",
        "grind",
        "grind_check",
        "hall_of_fame",
        "heal",
        "heal_to_full",
        "league_top_up",
        "leave_gift_summary",
        "legs",
        "level",
        "lift_floor",
        "mart",
        "new_game",
        "nugget_farm",
        "options",
        "pc",
        "poster_switch",
        "push_boulder",
        "ready",
        "register_bike",
        "restore_pp",
        "ride_bike",
        "route",
        "scene",
        "shop",
        "slope",
        "step",
        "surf",
        "talk",
        "teach",
        "trash_cans",
        "use_item",
        "use_repel",
    };
    return NAMES;
}

Pokemon::StoryActions make_frlg_action_parsers(
    std::function<Pokemon::StoryStepRun(StoryStep)> make_run
){
    Pokemon::StoryActions actions;
    for (const std::string& name : frlg_action_names()){
        actions.emplace(name, [name, make_run](const JsonObject& args) -> Pokemon::StoryStepRun{
            StoryStep parsed = parse_frlg_step(name, args);
            return make_run ? make_run(std::move(parsed)) : Pokemon::StoryStepRun{};
        });
    }
    return actions;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
