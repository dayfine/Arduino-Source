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
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"
#include "PokemonFRLG_AutoStorySlope.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryStepKind;


std::string_view story_action_name(StoryActionId id){
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
        absent_arg("cover_hp", StoryArgType::INTEGER, "buy only the first line's shortfall for the bag's Potions and Super Potions to heal this much HP"),
        absent_arg("fill_to", StoryArgType::INTEGER, "buy only what the bag lacks of the first line's item to hold this many"),
    }};
}

Pokemon::StoryActionSpec teach_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = TEACH_ACTION, .args = {
        required_arg("item", StoryArgType::WORD, "the TM or HM, e.g. \"hm03\""),
        required_arg("move", StoryArgType::WORD, "the move it holds, e.g. \"surf\""),
        absent_arg("slot", StoryArgType::INTEGER, "the party slot, from 1, when no species is named"),
        absent_arg("species", StoryArgType::WORD, "the member by its species, e.g. \"pidgey\": the first slot whose summary reads it"),
        absent_arg("forget", StoryArgType::WORD, "the move to forget when it knows four"),
        absent_arg("or_forget", StoryArgType::WORDS, "the moves to forget, in order, when it doesn't know `forget`"),
        absent_arg("keep", StoryArgType::WORDS, "the moves to keep when it knows four: the one known move outside them is forgotten"),
        absent_arg("forget_slot", StoryArgType::INTEGER, "the move slot to forget, from 1, when no move is named"),
    }};
}

Pokemon::StoryActionSpec grind_action_spec(){
    return {.kind = StoryStepKind::GAME, .name = GRIND_ACTION, .args = {
        required_arg("slot", StoryArgType::INTEGER, "the party slot that grinds, from 1"),
        required_arg("level", StoryArgType::INTEGER, "the level to reach"),
        optional_arg("encounters", StoryArgType::INTEGER, int64_t(60), "wild battles before the step gives up"),
        optional_arg("pace", StoryArgType::DIRECTION, Direction::EAST, "the way the player steps to look for a battle"),
        optional_arg("heal_every", StoryArgType::INTEGER, int64_t(0), "battles between visits to the nurse; 0: only when hurt"),
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

//  The variants of the built-in ROUTE, LEGS and STEP that move the player
//  another way: running, riding, tapping, and into a trainer's line.
std::vector<Pokemon::StoryActionSpec> movement_variant_specs(std::span<const Pokemon::StoryActionSpec> builtin){
    std::vector<Pokemon::StoryActionSpec> specs;
    const auto variant = [&](StoryStepKind kind, const std::string& name){
        Pokemon::StoryActionSpec spec = *Pokemon::find_story_action(builtin, kind, "");
        spec.name = name;
        specs.push_back(std::move(spec));
    };
    for (StoryStepKind kind : {StoryStepKind::ROUTE, StoryStepKind::LEGS}){
        variant(kind, RUN_VARIANT);
        variant(kind, BIKE_VARIANT);
    }
    variant(StoryStepKind::LEGS, TAPS_VARIANT);
    variant(StoryStepKind::STEP, TRAINER_LINE_VARIANT);
    variant(StoryStepKind::STEP, TRAINER_WAIT_VARIANT);
    return specs;
}

//  How a battle is fought by PokeGuy's policy (PokemonFRLG_AutoStoryBattlePolicy.h).
std::vector<Pokemon::StoryArgSpec> policy_arg_specs(){
    std::vector<Pokemon::StoryArgSpec> args;
    args.push_back(Pokemon::StoryArgSpec{
        .key = "policy", .type = Pokemon::StoryArgType::WORD, .required = false, .fallback = std::string(),
        .why = "PokeGuy's battle policy, \"solo-recovery\" or \"goal-based-setup\" (PokemonSimulation docs/battles/pokeguy-conditions.md); with foe_threats, and for the second the setup_* goals",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "setup_spa", .type = Pokemon::StoryArgType::INTEGER, .required = false, .fallback = int64_t(0),
        .why = "the doc's setup goal spa: the Sp. Atk stage to reach with X Specials",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "setup_accuracy", .type = Pokemon::StoryArgType::INTEGER, .required = false, .fallback = int64_t(0),
        .why = "the doc's setup goal accuracy: the accuracy stage to reach with X Accuracy, plus the foe's evasion",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "setup_outspeed", .type = Pokemon::StoryArgType::BOOLEAN, .required = false, .fallback = false,
        .why = "the doc's setup goal outspeed: X Speed until faster than every foe left",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "setup_guard_accuracy", .type = Pokemon::StoryArgType::BOOLEAN, .required = false, .fallback = false,
        .why = "the doc's setup goal guard_accuracy: Guard Spec first against a foe with an accuracy-lowering move",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "setup_torrent_power", .type = Pokemon::StoryArgType::BOOLEAN, .required = false, .fallback = false,
        .why = "the doc's setup goal torrent_power: a higher Sp. Atk goal while Torrent is off",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "foe_threats", .type = Pokemon::StoryArgType::INTEGER_ROWS, .required = false, .fallback = std::vector<std::vector<int64_t>>(),
        .why = "one row per foe, [speed, likely, mean, max, normal_max, lowers_accuracy, explodes]: what it does to the lead, from the doc's stats",
    });
    return args;
}

//  A trainer battle's plan names the trainer and its party.
std::vector<Pokemon::StoryArgSpec> battle_arg_specs(){
    std::vector<Pokemon::StoryArgSpec> args;
    args.push_back(Pokemon::StoryArgSpec{
        .key = "trainer", .type = Pokemon::StoryArgType::WORD, .required = false, .fallback = std::string(),
        .why = "pret's constant for the trainer (\"TRAINER_LEADER_BROCK\"); sets the battle's risk",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "foes", .type = Pokemon::StoryArgType::WORDS, .required = false, .fallback = std::vector<std::string>(),
        .why = "the trainer's party in order; attacks are chosen against the one out",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "use_items", .type = Pokemon::StoryArgType::ITEMS, .required = false, .fallback = std::vector<Pokemon::StoryItem>(),
        .why = "items used on the lead from the first turn, in order, before any attack (X Specials)",
    });
    args.push_back(Pokemon::StoryArgSpec{
        .key = "run", .type = Pokemon::StoryArgType::BOOLEAN, .required = false, .fallback = false,
        .why = "run from a scripted wild battle (Route 16's Snorlax) instead of fighting it",
    });
    for (Pokemon::StoryArgSpec& arg : policy_arg_specs()){
        args.push_back(std::move(arg));
    }
    return args;
}

} // namespace

std::span<const Pokemon::StoryActionSpec> story_action_specs(){
    static const std::vector<Pokemon::StoryActionSpec> SPECS = []{
        const std::span<const Pokemon::StoryActionSpec> builtin = Pokemon::builtin_story_actions();
        std::vector<Pokemon::StoryActionSpec> specs(builtin.begin(), builtin.end());
        for (Pokemon::StoryActionSpec& spec : specs){
            if (spec.kind != StoryStepKind::BATTLE){
                continue;
            }
            for (Pokemon::StoryArgSpec& arg : battle_arg_specs()){
                spec.args.push_back(std::move(arg));
            }
        }
        for (Pokemon::StoryActionSpec spec : movement_variant_specs(builtin)){
            specs.push_back(std::move(spec));
        }
        specs.push_back({.kind = StoryStepKind::TALK, .name = TALK_WANDERER_VARIANT, .args = {}});
        specs.push_back({.kind = StoryStepKind::GAME, .name = CLOSE_TRAINER_CARD, .args = {}});
        specs.push_back({.kind = StoryStepKind::GAME, .name = LEAVE_GIFT_SUMMARY, .args = {}});
        specs.push_back({.kind = StoryStepKind::GAME, .name = CUT_TREE_VARIANT, .args = {
            required_arg("direction", StoryArgType::DIRECTION, "the way to the tree, which the step then walks onto"),
        }});
        specs.push_back({.kind = StoryStepKind::GAME, .name = TRASH_CANS_VARIANT, .args = {}});
        for (Pokemon::StoryActionSpec spec : {
            pc_action_spec(), capture_action_spec(), fossil_action_spec(), level_action_spec(),
            mart_action_spec(), teach_action_spec(), grind_action_spec(), grind_check_action_spec(),
            slope_action_spec(), count_items_action_spec(),
        }){
            specs.push_back(std::move(spec));
        }
        for (const std::vector<Pokemon::StoryActionSpec>& more : {field_action_specs(), healing_action_specs()}){
            specs.insert(specs.end(), more.begin(), more.end());
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
