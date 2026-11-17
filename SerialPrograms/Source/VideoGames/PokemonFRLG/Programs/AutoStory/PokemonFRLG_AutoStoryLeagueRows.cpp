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
#include "PokemonFRLG_AutoStoryLeagueRows.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The League fight of `row`'s battle; none if it has none.
const LeagueFightData* row_fight(const StoryCheckpoint& row){
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

//  The supplies' counts by bag slug.
SlugCounts counts_of(const Supplies& supplies){
    SlugCounts counts(supplies.other_counts.begin(), supplies.other_counts.end());
    counts["potion"] = supplies.potions;
    counts["super-potion"] = supplies.super_potions;
    counts["antidote"] = supplies.antidotes;
    counts["parlyz-heal"] = supplies.parlyz_heals;
    counts["awakening"] = supplies.awakenings;
    counts["full-heal"] = supplies.full_heals;
    return counts;
}

} // namespace


Pokemon::Outcome league_row_pass(const StoryCheckpoint& row, const Supplies& start, const Supplies& end){
    const LeagueFightData* fight = row_fight(row);
    if (fight == nullptr){
        return Pokemon::outcome_ok();
    }
    const SlugCounts before = counts_of(start);
    const SlugCounts after = counts_of(end);
    SlugCounts used;
    std::string text;
    for (const auto& [item, count] : before){
        const int less = count - count_of(after, item);
        if (less > 0){
            used[item] = less;
            text += std::format("{}{} {}", text.empty() ? "" : ", ", item, less);
        }
    }
    if (league_try_passes(*fight, LeagueTry{.won = true, .used = used})){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("league_over_caps", std::format("{} spent {}", fight->name, text.empty() ? "nothing" : text));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
