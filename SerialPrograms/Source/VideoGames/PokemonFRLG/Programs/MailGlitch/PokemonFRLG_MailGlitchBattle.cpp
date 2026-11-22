/*  Mail Glitch Battle Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattleSwitch.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGameArgs.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPacing.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPocketList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStartMenu.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_MailGlitchActions.h"
#include "PokemonFRLG_MailGlitchBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryStep;


namespace{

constexpr Milliseconds TAP_HOLD = 140ms;
constexpr Milliseconds TAP_WAIT = 260ms;
constexpr Milliseconds BATTLE_MENU_TIMEOUT = 60000ms;
constexpr Milliseconds SCENE_TIMEOUT = 45000ms;

void dpad(ProControllerContext& context, DpadPosition pos){
    pbf_press_dpad(context, pos, TAP_HOLD, TAP_WAIT);
}

//  Resets battle menu cursor to top-left (FIGHT), then opens FIGHT and selects 0-based `move_slot`.
//  Does NOT press extra target confirmation (use `select_move_and_target` for doubles).
void open_fight_and_pick_move(ProControllerContext& context, size_t move_slot){
    dpad(context, DPAD_LEFT);
    dpad(context, DPAD_UP);
    pbf_press_button(context, BUTTON_A, 160ms, 900ms);
    dpad(context, DPAD_LEFT);
    dpad(context, DPAD_UP);
    if (move_slot % 2 == 1){
        dpad(context, DPAD_RIGHT);
    }
    if (move_slot >= 2){
        dpad(context, DPAD_DOWN);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 600ms);
}

//  From the battle command menu: opens POKEMON (bottom-left) and sends 0-based `party_slot`.
void battle_switch_to_slot(ProControllerContext& context, size_t party_slot){
    dpad(context, DPAD_RIGHT);
    dpad(context, DPAD_UP);
    dpad(context, DPAD_LEFT);
    dpad(context, DPAD_DOWN);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    send_member(context, party_slot);
}

//  Finds a move's 0-based slot in a SummaryRead, or std::nullopt.
std::optional<size_t> find_move_slot(const SummaryRead& summary, std::string_view move_slug){
    for (size_t i = 0; i < summary.moves.size(); i++){
        if (summary.moves[i].move == move_slug){
            return i;
        }
    }
    return std::nullopt;
}

//  Handles a "Delete a move to make room for X?" YES/NO prompt by replacing `learn_slot` (0-3).
void accept_move_into_slot(ConsoleHandle& console, ProControllerContext& context, size_t learn_slot){
    console.log(std::format("MailGlitch: move learn prompt detected; overwriting move slot {}.", learn_slot + 1));
    //  YES on "Delete a move?" -> move summary screen opens with cursor on slot 0.
    pbf_press_button(context, BUTTON_A, 160ms, 1200ms);
    const WallClock deadline = current_time() + 6000ms;
    std::optional<size_t> cursor;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        cursor = screen ? read_summary_move_cursor(screen) : std::nullopt;
        if (cursor){
            break;
        }
        pbf_wait(context, 300ms);
    }
    if (cursor){
        while (*cursor != learn_slot){
            dpad(context, *cursor < learn_slot ? DPAD_DOWN : DPAD_UP);
            context.wait_for_all_requests();
            const VideoSnapshot screen = console.video().snapshot();
            const std::optional<size_t> next = screen ? read_summary_move_cursor(screen) : std::nullopt;
            if (!next){
                break;
            }
            cursor = next;
        }
    }else{
        for (size_t c = 0; c < learn_slot; c++){
            dpad(context, DPAD_DOWN);
        }
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1800ms);
}

//  Waits for BattleMenuDetector while advancing battle text with B and handling move-learn prompts.
//  Returns: 0 = battle menu shown, 1 = party menu shown (fainted mon), 2 = overworld returned, -1 = timeout.
int wait_battle_menu_or_end(
    ConsoleHandle& console, ProControllerContext& context, size_t learn_slot, bool& saw_move_prompt
){
    const WallClock deadline = current_time() + BATTLE_MENU_TIMEOUT;
    size_t no_battle_polls = 0;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (!screen){
            pbf_wait(context, 300ms);
            continue;
        }
        if (BattleMenuDetector(COLOR_RED).detect(screen)){
            return 0;
        }
        if (PartyMenuDetector(COLOR_RED).detect(screen)){
            return 1;
        }
        if (battle_yes_no_shows(screen)){
            saw_move_prompt = true;
            accept_move_into_slot(console, context, learn_slot);
            no_battle_polls = 0;
            continue;
        }
        if (BattleScreenDetector(COLOR_RED).detect(screen) || BattleDialogDetector(COLOR_RED).detect(screen)){
            no_battle_polls = 0;
            pbf_press_button(context, BUTTON_B, 140ms, 300ms);
            continue;
        }
        no_battle_polls++;
        if (no_battle_polls >= 4){
            if (overworld_shows(console, context)){
                return 2;
            }
            no_battle_polls = 0;
        }
        pbf_press_button(context, BUTTON_B, 140ms, 300ms);
    }
    return -1;
}

//  Picks the best attacking move slot on the sweeper for Pokemon Tower (prefers Bite, then Surf, else slot 0).
size_t pick_sweeper_attack_slot(const SummaryRead& summary, size_t battle_index){
    const std::optional<size_t> bite = find_move_slot(summary, "bite");
    const std::optional<size_t> surf = find_move_slot(summary, "surf");
    if (bite && surf){
        //  Alternate Bite (25 PP) and Surf (15 PP) so we never run dry between PurifiedZone heals.
        return (battle_index % 3 == 0) ? *surf : *bite;
    }
    if (bite){
        return *bite;
    }
    if (surf){
        return *surf;
    }
    return 0;
}

//  Steps east onto Pokemon Tower 5F PurifiedZone, clears the heal dialog, and returns to the patch anchor.
void heal_on_purified_zone(StoryEnv& env, PaceSpot& spot, int64_t pad_steps_east){
    if (pad_steps_east <= 0){
        return;
    }
    env.console.log("MailGlitch: stepping onto Pokemon Tower 5F PurifiedZone to heal HP/PP.");
    //  If pacing left the player 1 tile east of anchor, 1 fewer step east is needed.
    const int64_t steps_to_pad = spot.displaced ? std::max<int64_t>(1, pad_steps_east - 1) : pad_steps_east;
    for (int64_t i = 0; i < steps_to_pad; i++){
        step_once(env.console, env.context, Direction::EAST, spot.facing, env.player_gender);
        spot.facing = Direction::EAST;
    }
    //  PurifiedZone triggers a white dialog + heal jingle; mash B back to overworld.
    pbf_wait(env.context, 1000ms);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    //  Step west back to the patch anchor.
    for (int64_t i = 0; i < pad_steps_east; i++){
        step_once(env.console, env.context, Direction::WEST, spot.facing, env.player_gender);
        spot.facing = Direction::WEST;
    }
    spot.displaced = false;
}

//  Uses a Revive from the Items pocket on 0-based `party_slot` to set HP == maxHP / 2.
bool use_revive_on_slot(StoryEnv& env, size_t party_slot){
    open_bag_from_overworld(env.console, env.context, start_menu_context(StartMenu::FULL));
    pbf_wait(env.context, 1500ms);
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(env.context, DPAD_LEFT, 100ms, 600ms);
    }
    for (int c = 0; c < 25; c++){
        pbf_press_dpad(env.context, DPAD_UP, 80ms, 100ms);
    }
    env.context.wait_for_all_requests();
    const PocketRead read = read_open_pocket(env.console, env.context, env.language);
    std::optional<size_t> revive_row;
    for (size_t row = 0; row < read.rows.size(); row++){
        if (read.rows[row].item == "revive" && read.rows[row].count != 0){
            revive_row = row;
            break;
        }
    }
    if (!revive_row){
        mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
        return false;
    }
    for (size_t c = 0; c < *revive_row; c++){
        pbf_press_dpad(env.context, DPAD_DOWN, 100ms, 200ms);
    }
    //  A -> USE (row 0) -> A opens party menu.
    pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    for (size_t c = 0; c < party_slot; c++){
        pbf_press_dpad(env.context, DPAD_DOWN, 100ms, 250ms);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 2000ms);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    return true;
}

} // namespace


Outcome run_switch_grind_step(StoryEnv& env, RunState&, const StoryStep& step){
    const std::string& trainee = Pokemon::story_arg<std::string>(step.args, "species");
    const std::string& sweeper = Pokemon::story_arg<std::string>(step.args, "sweeper");
    const int64_t target_level = Pokemon::story_arg<int64_t>(step.args, "target_level");
    const std::string& learn_move = Pokemon::story_arg<std::string>(step.args, "learn_move");
    const size_t learn_slot = counted_arg(step.args, "learn_slot", 4).value_or(0);
    const Direction pace = Pokemon::story_arg<Direction>(step.args, "pace");
    const int64_t pad_steps_east = Pokemon::story_arg<int64_t>(step.args, "heal_pad_steps_east");
    const int64_t heal_every = std::max<int64_t>(1, Pokemon::story_arg<int64_t>(step.args, "heal_every"));
    const int64_t max_encounters = Pokemon::story_arg<int64_t>(step.args, "encounters");

    const std::optional<size_t> trainee_slot = find_party_slot_by_species(env, trainee);
    if (!trainee_slot){
        return Pokemon::outcome_failed("grind_missing_trainee", trainee);
    }
    std::optional<SummaryRead> trainee_summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, *trainee_slot);
    if (!trainee_summary || !trainee_summary->level){
        return Pokemon::outcome_failed("grind_unread_level", trainee);
    }
    unsigned current_level = *trainee_summary->level;
    if (current_level >= static_cast<unsigned>(target_level) && find_move_slot(*trainee_summary, learn_move).has_value()){
        env.console.log(std::format("MailGlitch: {} is already Lv {} with {}; skipping switch_grind.", trainee, current_level, learn_move));
        return Pokemon::outcome_ok();
    }

    //  Put trainee in Slot 1 and sweeper in Slot 2.
    if (!swap_party_slots(env, *trainee_slot, 0)){
        return Pokemon::outcome_failed("grind_swap_trainee_failed", trainee);
    }
    const std::optional<size_t> sweeper_slot = find_party_slot_by_species(env, sweeper);
    if (!sweeper_slot){
        return Pokemon::outcome_failed("grind_missing_sweeper", sweeper);
    }
    if (!swap_party_slots(env, *sweeper_slot, 1)){
        return Pokemon::outcome_failed("grind_swap_sweeper_failed", sweeper);
    }
    const std::optional<SummaryRead> sweeper_summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 1);
    const SummaryRead sweeper_info = sweeper_summary.value_or(SummaryRead{});

    env.console.log(std::format(
        "MailGlitch: switch-grinding {} (Lv {} -> {}) with {} on move slot {} for {}.",
        trainee, current_level, target_level, sweeper, learn_slot + 1, learn_move
    ));

    PaceSpot spot;
    int64_t since_heal = 0;
    for (int64_t battle = 1; battle <= max_encounters; battle++){
        if (pad_steps_east > 0 && since_heal >= heal_every){
            heal_on_purified_zone(env, spot, pad_steps_east);
            since_heal = 0;
        }
        ConsolePatchSteps patch_steps(env.console, env.context, env.player_gender, pace, spot, []{});
        if (!find_paced_encounter(env.console.logger(), console_bike_select(env.console, env.context, env.player_gender), patch_steps)){
            return Pokemon::outcome_failed("grind_no_encounter", trainee);
        }
        if (handle_encounter(env.console, env.context, true)){
            throw ProgramFinishedException("MailGlitch: wild shiny encountered during switch_grind! Stopped without running.", env.console);
        }

        //  Turn 1: switch trainee (Slot 1) -> sweeper (Slot 2).
        bool saw_move_prompt = false;
        int state = wait_battle_menu_or_end(env.console, env.context, learn_slot, saw_move_prompt);
        if (state == 0){
            battle_switch_to_slot(env.context, 1);
        }

        //  Turn 2+: sweeper attacks until wild battle ends.
        const size_t attack_slot = pick_sweeper_attack_slot(sweeper_info, static_cast<size_t>(battle));
        for (int turn = 0; turn < 12; turn++){
            state = wait_battle_menu_or_end(env.console, env.context, learn_slot, saw_move_prompt);
            if (state == 2){
                break;
            }
            if (state == 0){
                open_fight_and_pick_move(env.context, attack_slot);
            }else if (state == 1){
                //  Trainee fainted before switching: send sweeper (Slot 2).
                send_member(env.context, 1);
            }else{
                return Pokemon::outcome_failed("grind_battle_timeout", trainee);
            }
        }
        wait_out_battle_exit(env.console, env.context);
        since_heal++;

        //  Check summary every battle near target or on move prompt, otherwise every 5 battles.
        const bool near_target = current_level + 3 >= static_cast<unsigned>(target_level);
        if (saw_move_prompt || near_target || (battle % 5 == 0)){
            trainee_summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
            if (trainee_summary && trainee_summary->level){
                current_level = *trainee_summary->level;
                env.console.log(std::format("MailGlitch: switch_grind battle {}: {} is Lv {}.", battle, trainee, current_level));
                if (current_level >= static_cast<unsigned>(target_level)){
                    if (pad_steps_east > 0){
                        heal_on_purified_zone(env, spot, pad_steps_east);
                    }
                    if (!find_move_slot(*trainee_summary, learn_move)){
                        env.console.log(std::format("MailGlitch: warning — {} did not read on {} summary.", learn_move, trainee), COLOR_ORANGE);
                    }
                    return Pokemon::outcome_ok();
                }
            }
        }
    }
    return Pokemon::outcome_failed("grind_out_of_encounters", trainee);
}

Outcome run_prepare_berry_hp_step(StoryEnv& env, RunState&, const StoryStep& step){
    const std::string& holder = Pokemon::story_arg<std::string>(step.args, "species");
    const std::string& sweeper = Pokemon::story_arg<std::string>(step.args, "sweeper");
    const Direction pace = Pokemon::story_arg<Direction>(step.args, "pace");

    const std::optional<size_t> holder_slot = find_party_slot_by_species(env, holder);
    if (!holder_slot){
        return Pokemon::outcome_failed("berry_hp_missing_holder", holder);
    }
    if (!swap_party_slots(env, *holder_slot, 0)){
        return Pokemon::outcome_failed("berry_hp_swap_holder_failed", holder);
    }
    const std::optional<size_t> sweeper_slot = find_party_slot_by_species(env, sweeper);
    if (!sweeper_slot){
        return Pokemon::outcome_failed("berry_hp_missing_sweeper", sweeper);
    }
    if (!swap_party_slots(env, *sweeper_slot, 1)){
        return Pokemon::outcome_failed("berry_hp_swap_sweeper_failed", sweeper);
    }

    env.console.log(std::format("MailGlitch: fainting {} in wild battle to Revive to exact maxHP / 2.", holder));
    PaceSpot spot;
    ConsolePatchSteps patch_steps(env.console, env.context, env.player_gender, pace, spot, []{});
    if (!find_paced_encounter(env.console.logger(), console_bike_select(env.console, env.context, env.player_gender), patch_steps)){
        return Pokemon::outcome_failed("berry_hp_no_encounter", holder);
    }
    if (handle_encounter(env.console, env.context, true)){
        throw ProgramFinishedException("MailGlitch: wild shiny encountered during prepare_berry_hp!", env.console);
    }

    bool dummy_prompt = false;
    bool holder_fainted = false;
    for (int turn = 0; turn < 25; turn++){
        const int state = wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt);
        if (state == 2){
            break;
        }
        if (state == 1){
            //  Party menu opened because Slot 1 fainted: send sweeper (Slot 2), then flee.
            holder_fainted = true;
            send_member(env.context, 1);
            continue;
        }
        if (state == 0){
            if (holder_fainted){
                flee_battle(env.console, env.context);
                break;
            }
            //  Use Move 1 (weak attack) to stall until wild Pokemon faints Slot 1.
            open_fight_and_pick_move(env.context, 0);
            continue;
        }
        return Pokemon::outcome_failed("berry_hp_battle_timeout", holder);
    }
    wait_out_battle_exit(env.console, env.context);

    if (!holder_fainted){
        return Pokemon::outcome_failed("berry_hp_did_not_faint", holder);
    }
    if (!use_revive_on_slot(env, 0)){
        return Pokemon::outcome_failed("berry_hp_no_revive", holder);
    }
    env.console.log(std::format("MailGlitch: {} revived to exact maxHP / 2.", holder), COLOR_BLUE);
    return Pokemon::outcome_ok();
}

Outcome run_mail_glitch_double_battle_step(StoryEnv& env, RunState&, const StoryStep& step){
    const std::string& knock_off_mon = Pokemon::story_arg<std::string>(step.args, "knock_off_species");
    const std::string& recycle_mon = Pokemon::story_arg<std::string>(step.args, "recycle_species");
    const std::string& sweeper = Pokemon::story_arg<std::string>(step.args, "sweeper");
    const Direction step_dir = Pokemon::story_arg<Direction>(step.args, "step_direction");

    //  Confirm move slots from summaries before entering the double battle.
    //  Party order expected: Slot 1 = CH'DING, Slot 2 = Berry holder, Slot 3 = MIMIEN, Slot 4 = Sweeper.
    const std::optional<SummaryRead> s1 = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
    const std::optional<SummaryRead> s3 = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 2);
    const std::optional<SummaryRead> s4 = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 3);
    const size_t knock_off_slot = (s1 ? find_move_slot(*s1, "knock-off") : std::nullopt).value_or(0);
    const size_t recycle_slot = (s3 ? find_move_slot(*s3, "recycle") : std::nullopt).value_or(0);
    const std::optional<size_t> surf_slot = s4 ? find_move_slot(*s4, "surf") : std::nullopt;
    const size_t sweep_slot = surf_slot.value_or(0);
    const bool sweep_is_spread = surf_slot.has_value();

    env.console.log(std::format(
        "MailGlitch: starting double battle ({} Knock Off slot {}, {} Recycle slot {}, {} sweep slot {}).",
        knock_off_mon, knock_off_slot + 1, recycle_mon, recycle_slot + 1, sweeper, sweep_slot + 1
    ));

    //  Step into Twins Eli & Anne's sight=1 line to trigger the double battle.
    step_once(env.console, env.context, step_dir, step_dir, env.player_gender);
    if (advance_text(env.console, env.context, 30000ms, true) != TextEnd::BATTLE){
        return Pokemon::outcome_failed("double_battle_not_started", "Twins Eli & Anne");
    }

    //  Turn 0 (entry): Slot 2 (<= maxHP / 2 holding Berry) auto-eats its Berry.
    //  Wait for Turn 1 Slot 1 (CH'DING) command menu.
    bool dummy_prompt = false;
    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != 0){
        return Pokemon::outcome_failed("double_battle_no_t1_slot1", "Turn 1 Slot 1 menu");
    }
    env.console.log("MailGlitch: Turn 1 Slot 1 (CH'DING) -> Knock Off targeting Ally Slot 2 (DPAD_RIGHT, A).");
    open_fight_and_pick_move(env.context, knock_off_slot);
    //  Verified in pokefirered src/battle_controller_player.c:
    //    sTargetIdentities = {PLAYER_LEFT(0), PLAYER_RIGHT(1), OPPONENT_RIGHT(2), OPPONENT_LEFT(3)}.
    //    Cursor starts at OPPONENT_LEFT(3). One DPAD_RIGHT wraps 3 -> 0 (skips self) -> 1 (PLAYER_RIGHT = Ally!).
    pbf_wait(env.context, 400ms);
    dpad(env.context, DPAD_RIGHT);
    pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);

    //  Turn 1 Slot 2 (Berry holder): switch to Slot 3 (MIMIEN, 0-based index 2).
    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != 0){
        return Pokemon::outcome_failed("double_battle_no_t1_slot2", "Turn 1 Slot 2 menu");
    }
    env.console.log("MailGlitch: Turn 1 Slot 2 -> switch to Slot 3 (MIMIEN).");
    battle_switch_to_slot(env.context, 2);

    //  Turn 2 Slot 1 (CH'DING): switch to Slot 4 (Sweeper, 0-based index 3).
    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != 0){
        return Pokemon::outcome_failed("double_battle_no_t2_slot1", "Turn 2 Slot 1 menu");
    }
    env.console.log("MailGlitch: Turn 2 Slot 1 -> switch to Slot 4 (Sweeper).");
    battle_switch_to_slot(env.context, 3);

    //  Turn 2 Slot 2 (MIMIEN): use Recycle (MOVE_TARGET_USER -> no target cursor!).
    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != 0){
        return Pokemon::outcome_failed("double_battle_no_t2_slot2", "Turn 2 Slot 2 menu");
    }
    env.console.log("MailGlitch: Turn 2 Slot 2 (MIMIEN) -> Recycle (restores Berry over Retro Mail!).");
    open_fight_and_pick_move(env.context, recycle_slot);

    //  Turn 3+: Sweeper finishes both foes (Surf hits both foes; single-target presses A once more).
    for (int action = 0; action < 20; action++){
        const int state = wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt);
        if (state == 2){
            break;
        }
        if (state == 1){
            //  If an ally fainted, send Slot 4 (or Slot 1) to continue sweep.
            send_member(env.context, 3);
            continue;
        }
        if (state == 0){
            open_fight_and_pick_move(env.context, sweep_slot);
            if (!sweep_is_spread){
                pbf_wait(env.context, 300ms);
                pbf_press_button(env.context, BUTTON_A, 160ms, 500ms);
            }
            continue;
        }
        return Pokemon::outcome_failed("double_battle_sweep_timeout", "Twins Eli & Anne");
    }

    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    wait_out_battle_exit(env.console, env.context);
    return Pokemon::outcome_ok();
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
