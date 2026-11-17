/*  AutoStory League Rows
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryLeagueRows.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("{}{}", text.empty() ? "" : ", ", line);
    }
    return text.empty() ? std::string("nothing") : text;
}

//  `wanted` of `what` against `had`, logged; short of it noted.
void compare(std::string_view what, int had, int wanted, LeagueEntryCheck& check){
    check.compared.push_back(std::format("{} {}/{}", what, had, wanted));
    if (had < wanted){
        check.short_of.push_back(std::format("{} {} of {}", what, had, wanted));
    }
}

} // namespace


const LeagueFightData* league_row_fight(const StoryCheckpoint& row){
    for (const Pokemon::StoryStep& step : row.steps){
        if (step.kind != Pokemon::StoryStepKind::BATTLE || !step.args.contains("trainer")){
            continue;
        }
        const std::optional<std::string> name = league_fight_name(Pokemon::story_arg<std::string>(step.args, "trainer"));
        if (name){
            return league_fight_data(league_plan_data(), *name);
        }
    }
    return nullptr;
}

const LeagueFightData* next_league_fight(const LeagueFightData& fight){
    const std::vector<LeagueFightData>& fights = league_plan_data().fights;
    for (size_t c = 0; c + 1 < fights.size(); c++){
        if (fights[c].name == fight.name){
            return &fights[c + 1];
        }
    }
    return nullptr;
}

LeagueEntryCheck league_entry_check(const LeagueFightData& next, LeagueSide side){
    LeagueEntryCheck check;
    PlannedCarry carry{.hp = side.hp, .max_hp = side.max_hp, .pp = side.pp, .full_pp = side.full_pp, .bag = side.bag};
    check.topped_up = planned_between(carry, next);
    for (const auto& [item, least] : next.entry_items){
        compare(item, count_of(carry.bag, item), least, check);
    }
    if (next.entry_hp_share){
        compare("HP", carry.hp, static_cast<int>(carry.max_hp * *next.entry_hp_share + 0.999), check);
    }
    for (const auto& [move, least] : next.entry_pp){
        compare(std::format("{} PP", move), count_of(carry.pp, move), least, check);
    }
    return check;
}

Pokemon::Outcome league_row_pass(const StoryCheckpoint& row, const LeagueSide& end){
    const LeagueFightData* fight = league_row_fight(row);
    if (fight == nullptr){
        return Pokemon::outcome_ok();
    }
    const LeagueFightData* next = next_league_fight(*fight);
    if (next == nullptr){
        return Pokemon::outcome_ok();
    }
    const LeagueEntryCheck check = league_entry_check(*next, end);
    if (!check.short_of.empty()){
        return Pokemon::outcome_failed("league_next_entry", std::format(
            "after {} and the top-up [{}] before {}, read vs wanted: {}; short of {}",
            fight->name, joined(check.topped_up), next->name, joined(check.compared), joined(check.short_of)
        ));
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
