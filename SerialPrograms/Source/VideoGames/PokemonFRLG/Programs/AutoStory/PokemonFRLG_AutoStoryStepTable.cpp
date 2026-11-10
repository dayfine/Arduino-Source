/*  AutoStory Step Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryChoice.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryStepKind;


const char* story_action_name(StoryActionId id){
    switch (id){
    case StoryActionId::NEW_GAME:   return "new_game";
    case StoryActionId::OPTIONS:    return "options";
    case StoryActionId::ROUTE:      return "route";
    case StoryActionId::LEGS:       return "legs";
    case StoryActionId::STEP:       return "step";
    case StoryActionId::TALK:       return "talk";
    case StoryActionId::ANSWER:     return "answer";
    case StoryActionId::MASH:       return "mash";
    case StoryActionId::BATTLE:     return "battle";
    case StoryActionId::HEAL:       return "heal";
    case StoryActionId::SHOP:       return "shop";
    case StoryActionId::READY:      return "ready";
    case StoryActionId::CLOSE_TRAINER_CARD: return "close_trainer_card";
    case StoryActionId::LEAVE_GIFT_SUMMARY: return "leave_gift_summary";
    case StoryActionId::CUT_TREE:   return "cut_tree";
    case StoryActionId::TRASH_CANS: return "trash_cans";
    case StoryActionId::GAME:       return "game";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown story action.");
}

const std::vector<StoryActionRow>& story_action_rows(){
    static const std::vector<StoryActionRow> ROWS{
        {StoryStepKind::NEW_GAME,   "", StoryActionId::NEW_GAME},
        {StoryStepKind::OPTIONS,    "", StoryActionId::OPTIONS},
        {StoryStepKind::ROUTE,      "", StoryActionId::ROUTE},
        {StoryStepKind::LEGS,       "", StoryActionId::LEGS},
        {StoryStepKind::STEP,       "", StoryActionId::STEP},
        {StoryStepKind::TALK,       "", StoryActionId::TALK},
        {StoryStepKind::ANSWER,     "", StoryActionId::ANSWER},
        {StoryStepKind::MASH,       "", StoryActionId::MASH},
        {StoryStepKind::BATTLE,     "", StoryActionId::BATTLE},
        {StoryStepKind::HEAL,       "", StoryActionId::HEAL},
        {StoryStepKind::SHOP,       "", StoryActionId::SHOP},
        {StoryStepKind::READY,      "", StoryActionId::READY},
        {StoryStepKind::GAME,       CLOSE_TRAINER_CARD, StoryActionId::CLOSE_TRAINER_CARD},
        {StoryStepKind::GAME,       LEAVE_GIFT_SUMMARY, StoryActionId::LEAVE_GIFT_SUMMARY},
        {StoryStepKind::GAME,       CUT_TREE_VARIANT,   StoryActionId::CUT_TREE},
        {StoryStepKind::GAME,       TRASH_CANS_VARIANT, StoryActionId::TRASH_CANS},
        {StoryStepKind::GAME,       "", StoryActionId::GAME},
    };
    return ROWS;
}

namespace{

using Pokemon::StoryArgType;
using Pokemon::Direction;

//  PC_ACTION's args (pc_args()).
Pokemon::StoryActionSpec pc_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = PC_ACTION, .args = {
        required_arg("do", StoryArgType::WORD, "\"deposit\" or \"withdraw\""),
        required_arg("slot", StoryArgType::INTEGER, "the party slot to deposit, or the box slot to withdraw, from 1"),
        required_arg("box", StoryArgType::INTEGER, "the box, from 1; a withdrawal's must be the one the PC shows"),
        optional_arg("if_present", StoryArgType::BOOLEAN, false, "skip a deposit when the party hasn't got the slot"),
    }};
}

Pokemon::StoryActionSpec capture_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = CAPTURE_ACTION, .args = {
        required_arg("species", StoryArgType::WORD, "the species' slug, e.g. \"rattata\""),
        optional_arg("catches", StoryArgType::INTEGER, int64_t(1), "how many to catch"),
        optional_arg("encounters", StoryArgType::INTEGER, int64_t(20), "wild battles before the step gives up"),
        optional_arg("balls", StoryArgType::INTEGER, int64_t(5), "Poke Balls thrown before the step gives up"),
        optional_arg("pace", StoryArgType::DIRECTION, Direction::EAST, "the way the player steps to look for a battle"),
        absent_arg("move", StoryArgType::INTEGER, "the FIGHT list's move that weakens the target, from 1; none: only balls"),
        optional_arg("turns", StoryArgType::INTEGER, int64_t(2), "the most turns spent weakening each target"),
    }};
}

Pokemon::StoryActionSpec fossil_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = FOSSIL_ACTION, .args = {
        required_arg("fossil", Pokemon::StoryArgType::WORD, "\"helix\" or \"dome\""),
    }};
}

Pokemon::StoryActionSpec level_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = LEVEL_ACTION, .args = {
        required_arg("slot", StoryArgType::INTEGER, "the party slot, from 1"),
        required_arg("min", StoryArgType::INTEGER, "the level it must have"),
    }};
}

Pokemon::StoryActionSpec mart_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = MART_ACTION, .args = {
        required_arg("map", StoryArgType::WORD, "the Mart's map, for its stock"),
        required_arg("budget", StoryArgType::INTEGER, "the most the step spends"),
        absent_arg("floor", StoryArgType::INTEGER, "the least money the player has here, when the money box can't be read"),
        required_arg("items", StoryArgType::ITEMS, "what to buy, in priority order"),
    }};
}

Pokemon::StoryActionSpec teach_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = TEACH_ACTION, .args = {
        required_arg("item", StoryArgType::WORD, "the TM or HM, e.g. \"hm03\""),
        required_arg("move", StoryArgType::WORD, "the move it holds, e.g. \"surf\""),
        required_arg("slot", StoryArgType::INTEGER, "the party slot, from 1"),
        absent_arg("forget", StoryArgType::WORD, "the move to forget when it knows four"),
        absent_arg("forget_slot", StoryArgType::INTEGER, "the move slot to forget, from 1, when no move is named"),
    }};
}

Pokemon::StoryActionSpec grind_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = GRIND_ACTION, .args = {
        required_arg("slot", StoryArgType::INTEGER, "the party slot that grinds, from 1"),
        required_arg("level", StoryArgType::INTEGER, "the level to reach; with a trainer, the cap"),
        optional_arg("encounters", StoryArgType::INTEGER, int64_t(60), "wild battles before the step gives up"),
        optional_arg("pace", StoryArgType::DIRECTION, Direction::EAST, "the way the player steps to look for a battle"),
        optional_arg("heal_every", StoryArgType::INTEGER, int64_t(0), "battles between visits to the nurse; 0: only when hurt"),
        absent_arg("beat", StoryArgType::WORD, "the next boss's trainer: grind until the sim's win chance is enough"),
        absent_arg("chance", StoryArgType::INTEGER, "the win chance against it, in percent"),
        required_arg("nurse", StoryArgType::GRID_POSE, "where the step heals"),
    }};
}

//  GRIND_CHECK_ACTION's args: GRIND_ACTION's but the nurse.
Pokemon::StoryActionSpec grind_check_action_spec(){
    Pokemon::StoryActionSpec spec = grind_action_spec();
    spec.name = GRIND_CHECK_ACTION;
    std::erase_if(spec.args, [](const Pokemon::StoryArgSpec& arg){ return arg.key == "nurse"; });
    return spec;
}

} // namespace

std::span<const Pokemon::StoryActionSpec> story_action_specs(){
    static const std::vector<Pokemon::StoryActionSpec> SPECS = []{
        const std::span<const Pokemon::StoryActionSpec> builtin = Pokemon::builtin_story_actions();
        std::vector<Pokemon::StoryActionSpec> specs(builtin.begin(), builtin.end());
        for (StoryStepKind kind : {StoryStepKind::ROUTE, StoryStepKind::LEGS}){
            Pokemon::StoryActionSpec run = *Pokemon::find_story_action(builtin, kind, "");
            run.name = "run";
            specs.push_back(std::move(run));
        }
        for (const std::string& name : {TRAINER_LINE_VARIANT, TRAINER_WAIT_VARIANT}){
            Pokemon::StoryActionSpec step = *Pokemon::find_story_action(builtin, StoryStepKind::STEP, "");
            step.name = name;
            specs.push_back(std::move(step));
        }
        specs.push_back({.kind = StoryStepKind::GAME, .name = CLOSE_TRAINER_CARD, .args = {}});
        specs.push_back({.kind = StoryStepKind::GAME, .name = LEAVE_GIFT_SUMMARY, .args = {}});
        specs.push_back({.kind = StoryStepKind::GAME, .name = CUT_TREE_VARIANT, .args = {
            required_arg("direction", StoryArgType::DIRECTION, "the way to the tree, which the step then walks onto"),
        }});
        specs.push_back({.kind = StoryStepKind::GAME, .name = TRASH_CANS_VARIANT, .args = {}});
        for (Pokemon::StoryActionSpec spec : {
            pc_action_spec(), capture_action_spec(), fossil_action_spec(), level_action_spec(),
            mart_action_spec(), teach_action_spec(), grind_action_spec(), grind_check_action_spec(),
        }){
            specs.push_back(std::move(spec));
        }
        return specs;
    }();
    return SPECS;
}

Pokemon::StoryActionTable make_story_action_table(const StoryActionBinding& bind){
    Pokemon::StoryActionTable table;
    for (const StoryActionRow& row : story_action_rows()){
        Pokemon::StoryAction action = bind(row.id);
        if (const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(story_action_specs(), row.kind, row.variant)){
            action.args = spec->args;
        }
        table.add(row.kind, row.variant, std::move(action));
    }
    //  FRLG's GAME actions without a row of their own: the GAME row's
    //  action, with each one's args.
    for (const Pokemon::StoryActionSpec& spec : story_action_specs()){
        const bool has_row = std::ranges::any_of(story_action_rows(), [&](const StoryActionRow& row){
            return row.kind == spec.kind && row.variant == spec.name;
        });
        if (spec.kind != StoryStepKind::GAME || spec.name.empty() || has_row){
            continue;
        }
        Pokemon::StoryAction action = bind(StoryActionId::GAME);
        action.args = spec.args;
        table.add(StoryStepKind::GAME, spec.name, std::move(action));
    }
    return table;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
