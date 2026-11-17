/*  AutoStory League Plan Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view PLAN_FILE = "frlg-league-plan";
constexpr std::string_view PLAN_FORMAT = "frlg-league-plan/1";

struct ItemName{
    std::string_view sim;
    std::string_view slug;
};
//  The plan's item names (PokemonSimulation's) and the program's slugs.
constexpr ItemName ITEM_NAMES[] = {
    {"xspecial", "x-special"}, {"xspeed", "x-speed"}, {"xaccuracy", "x-accuracy"},
    {"xattack", "x-attack"}, {"xdefend", "x-defend"}, {"guardspec", "guard-spec"},
    {"fullrestore", "full-restore"}, {"maxpotion", "max-potion"}, {"hyperpotion", "hyper-potion"},
    {"superpotion", "super-potion"}, {"potion", "potion"}, {"fullheal", "full-heal"},
    {"parlyzheal", "parlyz-heal"}, {"antidote", "antidote"}, {"awakening", "awakening"},
    {"burnheal", "burn-heal"}, {"iceheal", "ice-heal"}, {"elixir", "elixir"}, {"maxelixir", "max-elixir"},
};

std::string item_slug(std::string_view sim){
    for (const ItemName& name : ITEM_NAMES){
        if (name.sim == sim){
            return std::string(name.slug);
        }
    }
    throw ParseException(std::format("{}.json: unknown item \"{}\".", PLAN_FILE, sim));
}

struct TrainerFight{
    std::string_view prefix;
    std::string_view fight;
};
constexpr TrainerFight TRAINER_FIGHTS[] = {
    {"TRAINER_ELITE_FOUR_LORELEI", "lorelei"},
    {"TRAINER_ELITE_FOUR_BRUNO", "bruno"},
    {"TRAINER_ELITE_FOUR_AGATHA", "agatha"},
    {"TRAINER_ELITE_FOUR_LANCE", "lance"},
    {"TRAINER_CHAMPION_FIRST", "champion"},
};

SlugCounts counts_of(const JsonObject* object, bool items){
    SlugCounts counts;
    if (object == nullptr){
        return counts;
    }
    for (const auto& [name, value] : *object){
        counts[items ? item_slug(name) : name] = static_cast<int>(value.to_integer_throw());
    }
    return counts;
}

LeagueFoePlan foe_of(const JsonObject& object){
    LeagueFoePlan foe{.species = object.get_string_throw("species")};
    for (const JsonValue& move : object.get_array_throw("moves")){
        foe.moves.push_back(move.to_string_throw());
    }
    if (const JsonArray* sequence = object.get_array("sequence")){
        for (const JsonValue& move : *sequence){
            foe.sequence.push_back(move.to_string_throw());
        }
    }
    if (const JsonObject* heal = object.get_object("heal")){
        foe.heal = LeagueFoeHeal{
            .below = static_cast<int>(heal->get_integer_throw("below")),
            .heal_at_least = static_cast<int>(heal->get_integer_default("heal_at_least", 0)),
            .heal_when_can_ko = heal->get_boolean_default("heal_when_can_ko", false),
            .ko_rule = heal->get_boolean_default("ko_rule", true),
        };
    }
    foe.kill_order = object.get_boolean_default("kill_order", true);
    const JsonArray* setup = object.get_array("setup");
    if (setup == nullptr){
        return foe;
    }
    for (const JsonValue& value : *setup){
        const JsonObject& goal = value.to_object_throw();
        LeagueSetupGoal parsed{
            .item = item_slug(goal.get_string_throw("item")),
            .stage = static_cast<int>(goal.get_integer_default("stage", 1)),
        };
        if (const std::string* serves = goal.get_string("serves")){
            parsed.serves = *serves;
        }
        foe.setup.push_back(std::move(parsed));
    }
    return foe;
}

void read_reset(const JsonObject& reset, LeagueFightData& fight){
    fight.reset_on_roar = reset.get_boolean_default("roar", false);
    const JsonObject* after = reset.get_object("after_foe");
    if (after == nullptr){
        return;
    }
    fight.early_reset = LeagueEarlyReset{
        .index = static_cast<size_t>(after->get_integer_throw("index")),
        .speed_below = static_cast<int>(after->get_integer_throw("speed_below")),
        .items_used = counts_of(after->get_object("items_used"), true),
    };
}

LeaguePassFigures pass_of(const JsonObject& pass){
    const JsonObject& min_pass = pass.get_object_throw("min_pass");
    return LeaguePassFigures{
        .expected_pass = pass.get_double_throw("expected_pass"),
        .min_pass = static_cast<int>(min_pass.get_integer_throw("k")),
        .min_pass_of = static_cast<int>(min_pass.get_integer_throw("of")),
        .reset_budget = static_cast<int>(pass.get_integer_throw("reset_budget")),
    };
}

LeagueFightData fight_of(const std::string& name, const JsonObject& object){
    LeagueFightData fight{.name = name, .goal = object.get_string_default("goal")};
    for (const JsonValue& foe : object.get_array_throw("foes")){
        fight.foes.push_back(foe_of(foe.to_object_throw()));
    }
    if (const JsonObject* between = object.get_object("between")){
        fight.between_one_heal = between->get_string_default("heal") == "one";
        if (const JsonValue* hp = between->get_value("hp")){
            fight.between_hp = hp->to_double_throw();
        }
        fight.between_pp = counts_of(between->get_object("pp"), false);
    }
    if (const JsonObject* reserve = object.get_object("reserve")){
        fight.reserve_pp = counts_of(reserve->get_object("pp"), false);
        fight.reserve_items = counts_of(reserve->get_object("items"), true);
    }
    fight.caps = counts_of(object.get_object("caps"), true);
    if (const JsonObject* reset = object.get_object("reset")){
        read_reset(*reset, fight);
    }
    if (const JsonObject* contract = object.get_object("contract")){
        if (const JsonObject* pass = contract->get_object("pass")){
            fight.pass = pass_of(*pass);
        }
    }
    return fight;
}

} // namespace


LeaguePlanData parse_league_plan(std::string_view json){
    const JsonValue value = parse_json(std::string(json));
    const JsonObject& root = value.to_object_throw();
    if (root.get_string_default("format") != PLAN_FORMAT){
        throw ParseException(std::format("{}.json: not a {} plan.", PLAN_FILE, PLAN_FORMAT));
    }
    LeaguePlanData plan;
    const JsonObject& fights = root.get_object_throw("fights");
    //  In the League's order, whatever the file's.
    for (const TrainerFight& trainer : TRAINER_FIGHTS){
        const std::string name(trainer.fight);
        if (const JsonObject* fight = fights.get_object(name)){
            plan.fights.push_back(fight_of(name, *fight));
        }
    }
    return plan;
}

const LeaguePlanData& league_plan_data(){
    static const LeaguePlanData PLAN = []{
        for (const auto& [file, text] : autostory_checkpoint_json()){
            if (file == PLAN_FILE){
                return parse_league_plan(text);
            }
        }
        throw ParseException(std::format("StoryData/{}.json is not built into the program.", PLAN_FILE));
    }();
    return PLAN;
}

std::optional<std::string> league_fight_name(std::string_view trainer){
    for (const TrainerFight& fight : TRAINER_FIGHTS){
        if (trainer.starts_with(fight.prefix)){
            return std::string(fight.fight);
        }
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
