/*  AutoStory Engine Adapter
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  #648 StoryEngine checkpoint and save policy builders, run planning, and
 *  pre-run save/bag/summary readers for FRLG AutoStory.
 *
 */

#include <format>
#include <utility>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryHallOfFame.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryFailure;


namespace{

template <typename Body>
std::optional<StoryFailure> guarded_save(StoryEnv& env, Body&& body){
    try{
        return body();
    }catch (OperationFailedException& e){
        InterruptLogGuard(env.console.logger()).save_frame("save", e);
        return frlg_failure("operation_failed", e.message());
    }
}

StoryCheckpoint joined_rows(const StoryCheckpoint& first, const StoryCheckpoint& second){
    StoryCheckpoint row = second;
    row.id = first.id;
    row.start = first.start;
    row.supplies = first.supplies;
    row.live_start = first.live_start;
    row.steps = first.steps;
    row.steps.insert(row.steps.end(), second.steps.begin(), second.steps.end());
    return row;
}

StartRead read_console_start(StoryEnv& env, const std::string& checkpoint){
    if (env.keep_start_frame){
        env.keep_start_frame = false;
        return keep_console_start(env.console, env.context, env.start_frames, checkpoint);
    }
    return check_console_start(env.console, env.context, env.start_frames, checkpoint);
}

void keep_next_start_frame(StoryEnv& env, const std::string& ended){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    for (size_t c = 0; c + 1 < table.size(); c++){
        if (table[c].id == ended && checked_start_pose(table[c + 1])){
            keep_console_frame(env.console, env.context, env.start_frames, table[c + 1].id, "saved at the end of " + ended);
            return;
        }
    }
}

std::string next_checkpoint_id(const std::vector<StoryCheckpoint>& rows, std::string_view ended){
    for (size_t c = 0; c + 1 < rows.size(); c++){
        if (rows[c].id == ended){
            return rows[c + 1].id;
        }
    }
    return {};
}

const StoryCheckpoint* find_checkpoint_row(const std::vector<StoryCheckpoint>& rows, std::string_view id){
    for (const StoryCheckpoint& candidate : rows){
        if (candidate.id == id){
            return &candidate;
        }
    }
    return nullptr;
}

void init_checkpoint_state(const StoryCheckpoint& data, StoryExecutor& executor, FrlgRunState& run_state){
    StoryEnv& env = executor.env();
    executor.set_menu(data.menu);
    env.start_map = data.start ? data.start->map : std::string();
    env.checkpoint = data.id;
    env.start_menu = data.menu;
    run_state.pose = data.start;
    run_state.gates = data.start_gates;
    run_state.flags.insert(data.start_flags.begin(), data.start_flags.end());
}

Pokemon::StoryStep make_start_check_step(
    const StoryCheckpoint& data, StoryExecutor& executor, FrlgRunState& run_state
){
    Pokemon::StoryStep check_step;
    check_step.name = "save_at_start_pose";
    check_step.action = "start_check";
    check_step.run = [&data, &executor, &run_state](Pokemon::StoryBackend&, Pokemon::RunState&) -> std::optional<StoryFailure>{
        init_checkpoint_state(data, executor, run_state);
        StoryEnv& env = executor.env();
        return run_start_check(
            checked_start_pose(data),
            [&env, id = data.id]{ return read_console_start(env, id); },
            [&env](const std::string& line){ env.console.log(line, COLOR_ORANGE); }
        );
    };
    return check_step;
}

Pokemon::StoryStep make_poison_postcondition_step(
    const PoisonSurvivalPostcondition& cond, StoryExecutor& executor
){
    Pokemon::StoryStep post_step;
    post_step.name = "postcondition_poison_survival";
    post_step.action = "postcondition_poison_survival";
    post_step.run = [&executor, cond](Pokemon::StoryBackend&, Pokemon::RunState&) -> std::optional<StoryFailure>{
        StoryEnv& env = executor.env();
        std::optional<StoryFailure> failed = check_poison_survival(cond.steps, cond.max_hp, env.supplies);
        if (!failed){
            env.console.log("[Postcondition] poison_survival: passed.");
        }else{
            env.console.log(std::format("[Postcondition] {}", *failed), COLOR_RED);
        }
        return failed;
    };
    return post_step;
}

} // namespace


StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan;
    if (first > 0 && first < checkpoints.size() && checkpoints[first].live_start
        && checkpoints[first - 1].handoff == StoryHandoff::GIFT_RNG && handoffs.gift_rng
    ){
        plan.stop = StoryHandoff::GIFT_RNG;
        return plan;
    }
    for (size_t c = first; c <= last && c < checkpoints.size(); c++){
        const StoryCheckpoint& row = checkpoints[c];
        if (row.live_start && c != first){
            continue;
        }
        if (row.handoff == StoryHandoff::SID_HELPER && handoffs.sid_helper){
            plan.rows.clear();
            plan.stop = StoryHandoff::SID_HELPER;
            return plan;
        }
        if (row.handoff == StoryHandoff::STARTER_RNG && handoffs.starter_rng){
            plan.rows.emplace_back(row);
            plan.stop = StoryHandoff::STARTER_RNG;
            return plan;
        }
        if (row.handoff == StoryHandoff::GIFT_RNG && handoffs.gift_rng){
            plan.rows.emplace_back(row);
            plan.stop = StoryHandoff::GIFT_RNG;
            return plan;
        }
        if (row.handoff == StoryHandoff::STARTER_RNG && c + 1 < checkpoints.size()){
            plan.rows.emplace_back(joined_rows(row, checkpoints[c + 1]));
            c++;
            continue;
        }
        plan.rows.emplace_back(row);
    }
    return plan;
}

std::string check_save_place(StoryEnv& env, const std::string& start_map){
    if (!save_card_place(start_map) || !Pokemon::runs_check(env.checks, Pokemon::StoryCheckKind::SAVE_PLACE)){
        return "";
    }
    const std::string place = read_save_place(env.console, env.context, env.language);
    if (place.empty()){
        env.console.log("Couldn't read the save card's place; going on.", COLOR_ORANGE);
        return "";
    }
    const std::string error = save_place_error(start_map, place);
    if (error.empty()){
        env.console.log("The save is in " + place + ", where the checkpoint starts.");
    }
    return error;
}

void read_lead_moves(StoryEnv& env, StartMenu menu){
    const std::optional<SummaryRead> read = read_party_summary(env.console, env.context, menu, env.language, 0);
    if (!read || read->moves.empty()){
        env.console.log("Couldn't read the lead's moves; going on with the checkpoint's.", COLOR_ORANGE);
        return;
    }
    const PartyMember& before = env.supplies.lead;
    const std::string species = read->species.empty() ? before.species : read->species;
    const uint8_t level = read->level && *read->level <= 100 ? static_cast<uint8_t>(*read->level) : before.moves_through_level;
    const bool stats_fit = species == read->species && read->level.has_value();
    env.supplies.lead = member_from_summary(species, level, read->moves, stats_fit ? read->stats : std::nullopt);
    env.saved_supplies.lead = env.supplies.lead;
}

void read_bag_supplies(StoryEnv& env, StartMenu menu){
    if (menu != StartMenu::FULL){
        return;
    }
    if (!Pokemon::runs_check(env.checks, Pokemon::StoryCheckKind::BAG_CONTENTS)){
        env.console.log(std::format("Story: {}; keeping the save's counts.", Pokemon::skipped_check_line("the Bag after the load", env.checks)));
    }else if (const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language); rows.empty()){
        env.console.log("Couldn't read the Items pocket; going on with the checkpoint's items.", COLOR_ORANGE);
    }else{
        const bool same = bag_matches(env.supplies, rows);
        env.console.log(std::format("Story: the Bag after the load {} the save's counts; a battle reads it again.", same ? "matches" : "differs from"));
        env.bag_read = true;
    }
    recover_after_battle(env.console, env.context, env.supplies, env.checks, env.language);
}

bool is_hall_of_fame_checkpoint(const StoryCheckpoint& data){
    return !data.steps.empty() && data.steps.back().action == HALL_OF_FAME_ACTION;
}

Pokemon::StoryCheckpoint make_frlg_engine_checkpoint(
    const StoryCheckpoint& data, StoryExecutor& executor, FrlgRunState& run_state
){
    Pokemon::StoryCheckpoint cp;
    cp.id = data.id;
    cp.title = data.title;
    const bool has_start_check = Pokemon::runs_check(executor.env().checks, Pokemon::StoryCheckKind::START_SCREEN)
        && checked_start_pose(data).has_value();
    if (has_start_check){
        cp.steps.push_back(make_start_check_step(data, executor, run_state));
    }
    const size_t step_count = is_hall_of_fame_checkpoint(data) ? data.steps.size() - 1 : data.steps.size();
    for (size_t i = 0; i < step_count; i++){
        const StoryStep& step = data.steps[i];
        const bool needs_init = !has_start_check && i == 0;
        Pokemon::StoryStep engine_step;
        engine_step.name = step.name;
        engine_step.why = step.why;
        engine_step.action = step.action;
        engine_step.run = [&data, step, &executor, &run_state, needs_init](
            Pokemon::StoryBackend&, Pokemon::RunState&
        ) -> std::optional<StoryFailure>{
            if (needs_init){
                init_checkpoint_state(data, executor, run_state);
            }
            return executor.run_step(data.id, step, run_state);
        };
        cp.steps.push_back(std::move(engine_step));
    }
    if (data.poison_survival){
        cp.steps.push_back(make_poison_postcondition_step(*data.poison_survival, executor));
    }
    return cp;
}

Pokemon::StorySavePolicy frlg_save_policy(
    StoryEnv& env,
    const std::vector<StoryCheckpoint>& rows,
    StoryExecutor& executor,
    FrlgRunState& run_state
){
    return [&env, &rows, &executor, &run_state](
        Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint& passed
    ) -> std::optional<StoryFailure>{
        const StoryCheckpoint* row = find_checkpoint_row(rows, passed.id);
        if (row == nullptr){
            return frlg_failure("unknown_checkpoint", passed.id, true);
        }
        //  E6: Hall of Fame self-save is an FRLG policy special case.
        if (is_hall_of_fame_checkpoint(*row)){
            std::optional<StoryFailure> failed = executor.run_step(row->id, row->steps.back(), run_state);
            if (!failed){
                env.has_save = true;
                env.saved_checkpoint_id = next_checkpoint_id(rows, row->id);
                env.saved_supplies = env.supplies;
            }
            return failed;
        }
        return guarded_save(env, [&]() -> std::optional<StoryFailure>{
            save_game(env.console, env.context, row->menu, env.language, [end_map = checkpoint_end_map(row->id)](const std::string& place){
                return save_end_error(end_map, place);
            });
            keep_next_start_frame(env, row->id);
            env.has_save = true;
            env.saved_checkpoint_id = next_checkpoint_id(rows, row->id);
            env.saved_supplies = env.supplies;
            env.saved_field_moves.save(env.party.users());
            env.prepared.clear();
            env.bag_read = false;
            return std::nullopt;
        });
    };
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
