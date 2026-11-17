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

LeagueFightData fight_of(const std::string& name, const JsonObject& object){
    LeagueFightData fight{.name = name};
    for (const JsonValue& foe : object.get_array_throw("foes")){
        fight.foes.push_back(foe_of(foe.to_object_throw()));
    }
    if (const JsonObject* between = object.get_object("between")){
        if (const JsonValue* hp = between->get_value("hp")){
            fight.between_hp = hp->to_double_throw();
        }
        fight.between_pp = counts_of(between->get_object("pp"), false);
    }
    if (const JsonObject* reserve = object.get_object("reserve")){
        fight.reserve_pp = counts_of(reserve->get_object("pp"), false);
        fight.reserve_items = counts_of(reserve->get_object("items"), true);
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
