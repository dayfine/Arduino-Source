/*  Mail Glitch Battle Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_PartyDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_DigitReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCenterHeal.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryGameArgs.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryHealingSteps.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryItemsPocket.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMoveData.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPacedRoute.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPacing.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPocketList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryScreenEvents.h"
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
//  FRLG PlayerTurnInPlace runs InitMoveInPlace(..., 8) = 8 frames (133ms).
//  A 30ms hold (~1.8 GBA frames) cleanly triggers TURN_DIRECTION without ever
//  staying held past frame 8 into MOVING; 180ms release lets the turn finish.
constexpr Milliseconds SPIN_TAP_HOLD = 30ms;
constexpr Milliseconds SPIN_TAP_RELEASE = 180ms;
constexpr size_t SPIN_BATCH_TAPS = 24;
constexpr int SPIN_MAX_BATCHES = 10;

constexpr Milliseconds BATTLE_MENU_TIMEOUT = 60000ms;
constexpr Milliseconds PARTY_MENU_TIMEOUT = 10000ms;
constexpr Milliseconds SCENE_TIMEOUT = 45000ms;
constexpr Milliseconds STUCK_DIALOG_WAIT = 6000ms;
constexpr int SWITCH_GRIND_MAX_TURNS = 35;

enum class BattleWaitState{
    MENU,
    PARTY,
    OVERWORLD,
    OUT_OF_PP,
    TIMEOUT,
};

enum class GrindBattleEnd{
    WON,
    SWEEPER_FAINTED,
    SWITCH_FAILED,
    TIMEOUT,
    TURN_LIMIT,
};

struct GrindConfig{
    std::string trainee;
    std::string sweeper;
    unsigned target_level = 33;
    std::string learn_move;
    size_t learn_slot = 0;
    int64_t pad_steps_east = 0;
    int64_t heal_every = 10;
    int64_t batch_battles = 0;
    int64_t max_encounters = 150;
    std::optional<Direction> desired_end_facing;
};

void dpad(ProControllerContext& context, DpadPosition pos){
    pbf_press_dpad(context, pos, TAP_HOLD, TAP_WAIT);
}

DpadPosition direction_to_dpad(Direction dir){
    switch (dir){
    case Direction::NORTH: return DPAD_UP;
    case Direction::EAST:  return DPAD_RIGHT;
    case Direction::SOUTH: return DPAD_DOWN;
    case Direction::WEST:  return DPAD_LEFT;
    }
    return DPAD_DOWN;
}

int move_attack_priority(std::string_view slug, const MoveData* data){
    if (takes_two_turns(slug)){
        return 0;
    }
    if (slug == "surf"){
        return 100;
    }
    if (slug == "ice-beam"){
        return 90;
    }
    if (slug == "hydro-pump"){
        return 85;
    }
    if (slug == "blizzard"){
        return 80;
    }
    if (slug == "water-pulse"){
        return 75;
    }
    if (slug == "bite"){
        return 72;
    }
    if (slug == "brick-break"){
        return 70;
    }
    if (slug == "strength"){
        return 65;
    }
    if (slug == "earthquake"){
        return 55;
    }
    if (data != nullptr && data->damaging && data->power > 0){
        return std::max<int>(10, data->power / 2);
    }
    return 0;
}

//  Resets battle menu cursor to top-left (FIGHT), then opens FIGHT and selects 0-based `move_slot`.
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

//  Matches both Single-Battle Party Menu (via PartyMenuDetector) and Double-Battle
//  Party Menu (where Active Slot 2 occupies the left teal strip checked by PartyMenuDetector,
//  leaving the teal band below Active Slot 2 at y=0.760..0.805).
bool any_party_menu_shows(const ImageViewRGB32& screen){
    if (PartyMenuDetector(COLOR_RED).detect(screen)){
        return true;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageViewRGB32 dialog_top = extract_box_reference(game, ImageFloatBox(0.028, 0.840, 0.705, 0.010));
    const ImageViewRGB32 lower_teal = extract_box_reference(game, ImageFloatBox(0.028, 0.740, 0.010, 0.070));
    return is_white(dialog_top) && is_solid(lower_teal, {0.138, 0.448, 0.414}, 0.25, 20);
}

bool wait_any_party_menu_open(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (screen && any_party_menu_shows(screen)){
            return true;
        }
        pbf_wait(context, 200ms);
    }
    return false;
}

bool wait_party_menu_closed(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (screen && !any_party_menu_shows(screen)){
            return true;
        }
        pbf_wait(context, 250ms);
    }
    return false;
}

//  With the battle party menu already open (cursor always starts on Slot 0),
//  selects 0-based `party_slot` and verifies the party menu closes.
bool select_party_member_in_battle(ConsoleHandle& console, ProControllerContext& context, size_t party_slot){
    for (int attempt = 0; attempt < 3; attempt++){
        pbf_wait(context, 650ms);
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (screen && !any_party_menu_shows(screen)){
            return true;
        }
        if (attempt == 0){
            for (size_t c = 0; c < party_slot; c++){
                pbf_press_dpad(context, DPAD_DOWN, 140ms, 360ms);
            }
        }
        PartySelectionWatcher mon_menu(COLOR_RED);
        context.wait_for_all_requests();
        pbf_press_button(context, BUTTON_A, 160ms, 0ms);
        wait_until(console, context, 2000ms, {mon_menu});
        pbf_wait(context, 300ms);
        pbf_press_button(context, BUTTON_A, 160ms, 600ms);
        if (wait_party_menu_closed(console, context, 3500ms)){
            return true;
        }
        console.log(
            std::format("MailGlitch: party menu stayed open after selecting slot {} (attempt {}); retrying.", party_slot + 1, attempt + 1),
            COLOR_ORANGE
        );
        pbf_press_button(context, BUTTON_B, 160ms, 500ms);
    }
    return false;
}

//  From the battle command menu: opens POKEMON (bottom-left), waits for
//  Single- or Double-Battle Party Menu, and sends 0-based `party_slot`.
bool battle_switch_to_slot(ConsoleHandle& console, ProControllerContext& context, size_t party_slot){
    dpad(context, DPAD_LEFT);
    dpad(context, DPAD_DOWN);
    pbf_press_button(context, BUTTON_A, 160ms, 400ms);
    if (!wait_any_party_menu_open(console, context, PARTY_MENU_TIMEOUT)){
        console.log("MailGlitch: PartyMenu timed out after selecting POKEMON.", COLOR_ORANGE);
        return false;
    }
    return select_party_member_in_battle(console, context, party_slot);
}

std::optional<size_t> find_move_slot(const SummaryRead& summary, std::string_view move_slug){
    for (size_t i = 0; i < summary.moves.size(); i++){
        if (summary.moves[i].move == move_slug){
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> wait_summary_move_cursor(
    ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout
){
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const std::optional<size_t> cursor = screen ? read_summary_move_cursor(screen) : std::nullopt;
        if (cursor){
            return cursor;
        }
        pbf_wait(context, 300ms);
    }
    return std::nullopt;
}

//  Handles a "Delete a move to make room for X?" YES/NO prompt by replacing `learn_slot` (0-3).
//  Waits for the Move Summary fade-in to settle and confirms the screen closes before returning
//  so the caller's dialog loop can never press B onto an open Move Summary screen.
void accept_move_into_slot(ConsoleHandle& console, ProControllerContext& context, size_t learn_slot){
    console.log(std::format("MailGlitch: move learn prompt detected; overwriting move slot {}.", learn_slot + 1));
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    std::optional<size_t> cursor = wait_summary_move_cursor(console, context, 6000ms);
    //  Let gPaletteFade finish before sending D-pad / A inputs on the Move Summary screen.
    pbf_wait(context, 500ms);
    if (cursor){
        for (int tries = 0; tries < 8 && *cursor != learn_slot; tries++){
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
    for (int confirm = 0; confirm < 4; confirm++){
        pbf_press_button(context, BUTTON_A, 160ms, 1400ms);
        context.wait_for_all_requests();
        const VideoSnapshot after = console.video().snapshot();
        if (!after || !read_summary_move_cursor(after)){
            break;
        }
    }
    pbf_wait(context, 1000ms);
}

std::optional<BattleWaitState> detect_interactive_battle_state(const ImageViewRGB32& screen){
    if (BattleMenuDetector(COLOR_RED).detect(screen)){
        return BattleWaitState::MENU;
    }
    if (any_party_menu_shows(screen)){
        return BattleWaitState::PARTY;
    }
    if (BattleOutOfPpDetector(COLOR_RED).detect(screen)){
        return BattleWaitState::OUT_OF_PP;
    }
    return std::nullopt;
}

//  Advances non-menu battle frames only on the red AdvanceBattleDialog arrow
//  (avoiding stray B presses into a "Delete a move?" YES/NO prompt).
//  When `confirm_yes_no_with_a` is true (used by faint_holder_and_flee), YES/NO
//  and stuck battle dialogs press A ("Use next POKEMON? YES") instead of
//  running accept_move_into_slot or pressing B (which answers NO and flees).
bool step_non_menu_battle_frame(
    ConsoleHandle& console, ProControllerContext& context, const ImageViewRGB32& screen,
    size_t learn_slot, bool& saw_move_prompt, WallClock& last_press, size_t& no_battle_polls,
    bool confirm_yes_no_with_a, bool probe_overworld
){
    const WallClock now = current_time();
    if (battle_yes_no_shows(screen)){
        if (confirm_yes_no_with_a){
            pbf_press_button(context, BUTTON_A, 140ms, 400ms);
        }else{
            saw_move_prompt = true;
            accept_move_into_slot(console, context, learn_slot);
        }
        no_battle_polls = 0;
        last_press = current_time();
        return false;
    }
    if (AdvanceBattleDialogDetector(COLOR_RED).detect(screen)){
        no_battle_polls = 0;
        pbf_press_button(context, BUTTON_B, 140ms, 250ms);
        last_press = now;
        return false;
    }
    if (BattleScreenDetector(COLOR_RED).detect(screen) || BattleDialogDetector(COLOR_RED).detect(screen)){
        no_battle_polls = 0;
        if (now - last_press >= STUCK_DIALOG_WAIT){
            if (probe_overworld && overworld_shows(console, context)){
                return true;
            }
            pbf_press_button(context, confirm_yes_no_with_a ? BUTTON_A : BUTTON_B, 140ms, 250ms);
            last_press = now;
        }else{
            pbf_wait(context, 250ms);
        }
        return false;
    }
    no_battle_polls++;
    if (no_battle_polls >= 4){
        if (overworld_shows(console, context)){
            return true;
        }
        no_battle_polls = 0;
    }
    if (now - last_press >= 1200ms){
        pbf_press_button(context, BUTTON_B, 140ms, 250ms);
        last_press = now;
    }else{
        pbf_wait(context, 250ms);
    }
    return false;
}

BattleWaitState wait_battle_menu_or_end(
    ConsoleHandle& console, ProControllerContext& context, size_t learn_slot, bool& saw_move_prompt,
    bool confirm_yes_no_with_a = false, bool probe_overworld = false
){
    const WallClock deadline = current_time() + BATTLE_MENU_TIMEOUT;
    WallClock last_press = current_time();
    size_t no_battle_polls = 0;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (!screen){
            pbf_wait(context, 250ms);
            continue;
        }
        if (const std::optional<BattleWaitState> interactive = detect_interactive_battle_state(screen)){
            return *interactive;
        }
        if (step_non_menu_battle_frame(
            console, context, screen, learn_slot, saw_move_prompt, last_press, no_battle_polls,
            confirm_yes_no_with_a, probe_overworld
        )){
            return BattleWaitState::OVERWORLD;
        }
    }
    return BattleWaitState::TIMEOUT;
}

size_t next_clockwise_index(std::optional<Direction> facing){
    if (!facing){
        return 0;
    }
    switch (*facing){
    case Direction::NORTH: return 1;
    case Direction::EAST:  return 2;
    case Direction::SOUTH: return 3;
    case Direction::WEST:  return 0;
    }
    return 0;
}

std::optional<Direction> read_synced_facing(StoryEnv& env){
    for (int tries = 0; tries < 3; tries++){
        const std::optional<Direction> facing = OverworldWalkSteps::read_facing(env.console, env.context, env.player_gender);
        if (facing){
            return facing;
        }
        pbf_wait(env.context, 200ms);
    }
    return std::nullopt;
}

void turn_to_facing_no_step(StoryEnv& env, Direction desired){
    const std::optional<Direction> current = read_synced_facing(env);
    if (current && *current == desired){
        return;
    }
    pbf_press_dpad(env.context, direction_to_dpad(desired), SPIN_TAP_HOLD, SPIN_TAP_RELEASE);
    env.context.wait_for_all_requests();
}

//  Spins in place with ultra-light 30ms clockwise taps, re-syncing facing via
//  read_facing before every 24-tap batch so Tap 0 never matches current facing.
bool spin_for_encounter(StoryEnv& env){
    static constexpr DpadPosition CLOCKWISE[4] = {DPAD_UP, DPAD_RIGHT, DPAD_DOWN, DPAD_LEFT};
    for (int batch = 0; batch < SPIN_MAX_BATCHES; batch++){
        env.context.wait_for_all_requests();
        const VideoSnapshot pre = env.console.video().snapshot();
        if (pre && (BattleScreenDetector(COLOR_RED).detect(pre) || BattleMenuDetector(COLOR_RED).detect(pre))){
            return true;
        }
        const size_t start_idx = next_clockwise_index(read_synced_facing(env));
        BattleFlashWatcher battle_flash(COLOR_RED);
        BattleScreenWatcher battle_screen(COLOR_RED);
        BattleMenuWatcher battle_menu(COLOR_RED);
        WhiteDialogWatcher dialog(COLOR_RED);
        const int ret = run_until<ProControllerContext>(
            env.console, env.context,
            [start_idx](ProControllerContext& ctx){
                for (size_t i = 0; i < SPIN_BATCH_TAPS; i++){
                    pbf_press_dpad(ctx, CLOCKWISE[(start_idx + i) % 4], SPIN_TAP_HOLD, SPIN_TAP_RELEASE);
                }
            },
            {{battle_flash}, {battle_screen}, {battle_menu}, {dialog}}
        );
        if (ret == 0 || ret == 1 || ret == 2){
            return true;
        }
        if (ret == 3){
            mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
        }
    }
    return false;
}

void note_sweeper_hp(ConsoleHandle& console, bool& sweeper_hurt){
    const VideoSnapshot snap = console.video().snapshot();
    if (!snap || !BattlePokemonDetector(COLOR_RED).detect(snap)){
        return;
    }
    const PartyStatus status = read_party_status(snap);
    if (status.hp_seen && status.hp_fill > 0.0 && status.hp_fill < 0.50){
        console.log(std::format("MailGlitch: sweeper HP low ({:.0f}%); will heal after this battle.", status.hp_fill * 100.0), COLOR_ORANGE);
        sweeper_hurt = true;
    }
}

//  Turn 1 of a switch-grind battle: switch trainee (Slot 1) -> sweeper (Slot 2).
std::optional<GrindBattleEnd> execute_grind_turn1_switch(
    StoryEnv& env, size_t learn_slot, bool& saw_move_prompt, bool& sweeper_sent
){
    const BattleWaitState state = wait_battle_menu_or_end(env.console, env.context, learn_slot, saw_move_prompt);
    if (state == BattleWaitState::OVERWORLD){
        return GrindBattleEnd::WON;
    }
    if (state == BattleWaitState::MENU){
        sweeper_sent = battle_switch_to_slot(env.console, env.context, 1);
        return sweeper_sent ? std::nullopt : std::optional<GrindBattleEnd>(GrindBattleEnd::SWITCH_FAILED);
    }
    if (state == BattleWaitState::PARTY){
        sweeper_sent = select_party_member_in_battle(env.console, env.context, 1);
        return sweeper_sent ? std::nullopt : std::optional<GrindBattleEnd>(GrindBattleEnd::SWITCH_FAILED);
    }
    return GrindBattleEnd::TIMEOUT;
}

//  Fights a single switch-grind wild battle until overworld returns.
GrindBattleEnd fight_switch_grind_battle(
    StoryEnv& env, SweeperMovePool& moves, size_t learn_slot,
    bool& saw_move_prompt, bool& sweeper_hurt
){
    bool sweeper_sent = false;
    if (const std::optional<GrindBattleEnd> early = execute_grind_turn1_switch(env, learn_slot, saw_move_prompt, sweeper_sent)){
        return *early;
    }
    size_t last_attack_slot = moves.pick_slot();
    for (int turn = 0; turn < SWITCH_GRIND_MAX_TURNS; turn++){
        const BattleWaitState state = wait_battle_menu_or_end(env.console, env.context, learn_slot, saw_move_prompt);
        if (state == BattleWaitState::OVERWORLD){
            wait_out_battle_exit(env.console, env.context);
            return GrindBattleEnd::WON;
        }
        if (state == BattleWaitState::OUT_OF_PP){
            env.console.log(std::format("MailGlitch: move slot {} out of PP; rotating attack.", last_attack_slot + 1), COLOR_ORANGE);
            pbf_press_button(env.context, BUTTON_B, 160ms, 500ms);
            pbf_press_button(env.context, BUTTON_B, 160ms, 500ms);
            moves.mark_empty(last_attack_slot);
            continue;
        }
        if (state == BattleWaitState::PARTY){
            if (sweeper_sent){
                return GrindBattleEnd::SWEEPER_FAINTED;
            }
            if (!select_party_member_in_battle(env.console, env.context, 1)){
                return GrindBattleEnd::SWITCH_FAILED;
            }
            sweeper_sent = true;
            continue;
        }
        if (state == BattleWaitState::MENU){
            note_sweeper_hp(env.console, sweeper_hurt);
            last_attack_slot = moves.pick_slot();
            open_fight_and_pick_move(env.context, last_attack_slot);
            moves.note_used(last_attack_slot);
            continue;
        }
        return GrindBattleEnd::TIMEOUT;
    }
    return GrindBattleEnd::TURN_LIMIT;
}

std::string grind_battle_error_code(GrindBattleEnd end){
    switch (end){
    case GrindBattleEnd::SWEEPER_FAINTED: return "grind_sweeper_fainted";
    case GrindBattleEnd::SWITCH_FAILED:   return "grind_switch_failed";
    case GrindBattleEnd::TURN_LIMIT:      return "grind_turn_limit";
    case GrindBattleEnd::TIMEOUT:
    case GrindBattleEnd::WON:             return "grind_battle_timeout";
    }
    return "grind_battle_timeout";
}

std::optional<SummaryRead> setup_grind_party(
    StoryEnv& env, const std::string& trainee, const std::string& sweeper, size_t trainee_slot, std::string& err
){
    if (!swap_party_slots(env, trainee_slot, 0)){
        err = "grind_swap_trainee_failed";
        return std::nullopt;
    }
    const std::optional<size_t> sweeper_slot = find_party_slot_by_species(env, sweeper);
    if (!sweeper_slot){
        err = "grind_missing_sweeper";
        return std::nullopt;
    }
    if (!swap_party_slots(env, *sweeper_slot, 1)){
        err = "grind_swap_sweeper_failed";
        return std::nullopt;
    }
    return read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 1).value_or(SummaryRead{});
}

GrindConfig parse_grind_config(const StoryStep& step){
    GrindConfig cfg;
    cfg.trainee = Pokemon::story_arg<std::string>(step.args, "species");
    cfg.sweeper = Pokemon::story_arg<std::string>(step.args, "sweeper");
    cfg.target_level = static_cast<unsigned>(Pokemon::story_arg<int64_t>(step.args, "target_level"));
    cfg.learn_move = Pokemon::story_arg<std::string>(step.args, "learn_move");
    cfg.learn_slot = counted_arg(step.args, "learn_slot", 4).value_or(0);
    cfg.batch_battles = std::max<int64_t>(0, Pokemon::story_arg<int64_t>(step.args, "batch_battles"));
    cfg.max_encounters = Pokemon::story_arg<int64_t>(step.args, "encounters");
    if (step.pose_after){
        if (const std::optional<StoryPose> after = story_pose(Pokemon::pose_of(*step.pose_after))){
            cfg.desired_end_facing = after->facing;
        }
    }
    return cfg;
}

Outcome finish_switch_grind(StoryEnv& env, const GrindConfig& cfg, const SummaryRead& trainee_summary){
    swap_party_slots(env, 1, 0);
    if (!find_move_slot(trainee_summary, cfg.learn_move)){
        env.console.log(std::format("MailGlitch: warning — {} did not read on {} summary.", cfg.learn_move, cfg.trainee), COLOR_ORANGE);
    }
    return Pokemon::outcome_ok();
}

Outcome finish_grind_batch(StoryEnv& env, RunState& state, const GrindConfig& cfg, int64_t fought, unsigned current_level){
    state.flags.insert(std::string(GRIND_BATCH_USED_FLAG));
    swap_party_slots(env, 1, 0);
    env.console.log(std::format(
        "MailGlitch: finished grind batch ({} battles) for {} (Lv {} / target {}); returning to PurifiedZone to heal and save.",
        fought, cfg.trainee, current_level, cfg.target_level
    ));
    return Pokemon::outcome_ok();
}

std::optional<Outcome> check_grind_progress(
    StoryEnv& env, RunState& state, const GrindConfig& cfg,
    int64_t battle, unsigned& current_level
){
    const std::optional<SummaryRead> summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
    if (!summary || !summary->level){
        return std::nullopt;
    }
    current_level = *summary->level;
    env.console.log(std::format("MailGlitch: switch_grind battle {}: {} is Lv {}.", battle, cfg.trainee, current_level));
    if (current_level < cfg.target_level){
        return std::nullopt;
    }
    if (!cfg.learn_move.empty() && !find_move_slot(*summary, cfg.learn_move)){
        env.console.log(
            std::format("MailGlitch: {} reached Lv {} without learning {}; failing batch to reload pre-level-up save!", cfg.trainee, current_level, cfg.learn_move),
            COLOR_RED
        );
        return Pokemon::outcome_failed("grind_missed_learn_move", cfg.learn_move);
    }
    state.flags.insert(grind_done_flag(cfg.trainee));
    if (cfg.batch_battles > 0){
        state.flags.insert(std::string(GRIND_BATCH_USED_FLAG));
    }
    return finish_switch_grind(env, cfg, *summary);
}

Outcome run_switch_grind_loop(StoryEnv& env, RunState& state, const GrindConfig& cfg, SweeperMovePool& moves, unsigned current_level){
    const int64_t limit = (cfg.batch_battles > 0) ? cfg.batch_battles : cfg.max_encounters;
    int64_t since_check = 0;
    int64_t fought = 0;
    bool sweeper_hurt = false;
    for (int64_t battle = 1; battle <= limit; battle++){
        if (cfg.batch_battles > 0 && fought > 0 && (sweeper_hurt || moves.low_pp())){
            break;
        }
        if (!spin_for_encounter(env)){
            return Pokemon::outcome_failed("grind_no_encounter", cfg.trainee);
        }
        if (handle_encounter(env.console, env.context, true)){
            throw ProgramFinishedException("MailGlitch: wild shiny encountered during switch_grind! Stopped without running.", env.console);
        }
        bool saw_move_prompt = false;
        const GrindBattleEnd end = fight_switch_grind_battle(env, moves, cfg.learn_slot, saw_move_prompt, sweeper_hurt);
        if (end != GrindBattleEnd::WON){
            return Pokemon::outcome_failed(grind_battle_error_code(end), cfg.trainee);
        }
        fought++;
        since_check++;
        const bool check_now = should_check_grind_summary(current_level, cfg.target_level, since_check, saw_move_prompt)
            || (cfg.batch_battles > 0 && battle == limit);
        if (check_now){
            since_check = 0;
            if (const std::optional<Outcome> done = check_grind_progress(env, state, cfg, battle, current_level)){
                return *done;
            }
        }
    }
    if (cfg.batch_battles > 0){
        return finish_grind_batch(env, state, cfg, fought, current_level);
    }
    return Pokemon::outcome_failed("grind_out_of_encounters", cfg.trainee);
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
    //  Ensure cursor is at the very top of Items pocket after read_open_pocket.
    for (int c = 0; c < 15; c++){
        pbf_press_dpad(env.context, DPAD_UP, 80ms, 120ms);
    }
    for (size_t c = 0; c < *revive_row; c++){
        pbf_press_dpad(env.context, DPAD_DOWN, 120ms, 280ms);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 900ms);
    pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    if (!wait_any_party_menu_open(env.console, env.context, 15000ms)){
        env.console.log("MailGlitch: PartyMenu did not open after selecting REVIVE -> USE.", COLOR_RED);
        mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
        return false;
    }
    pbf_wait(env.context, 600ms);
    //  The party cursor may be remembered on the active (right-column) mon
    //  after a battle. DPAD_LEFT from any right-column slot jumps to slot 0.
    pbf_press_dpad(env.context, DPAD_LEFT, 120ms, 280ms);
    for (size_t c = 0; c < party_slot; c++){
        pbf_press_dpad(env.context, DPAD_DOWN, 120ms, 280ms);
    }
    pbf_press_button(env.context, BUTTON_A, 160ms, 2500ms);
    pbf_press_button(env.context, BUTTON_A, 160ms, 1500ms);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    return true;
}

size_t pick_non_damaging_move_slot(const SummaryRead& summary){
    for (size_t i = 0; i < summary.moves.size(); i++){
        const MoveData* data = move_data(summary.moves[i].move);
        if (data != nullptr && (!data->damaging || data->power == 0)){
            return i;
        }
    }
    return 0;
}

Outcome faint_holder_and_flee(StoryEnv& env, const std::string& holder, size_t stall_move_slot){
    bool dummy_prompt = false;
    bool holder_fainted = false;
    for (int turn = 0; turn < 25; turn++){
        const BattleWaitState wait = wait_battle_menu_or_end(
            env.console, env.context, 0, dummy_prompt, /*confirm_yes_no_with_a=*/!holder_fainted
        );
        if (wait == BattleWaitState::OVERWORLD){
            break;
        }
        if (wait == BattleWaitState::PARTY){
            holder_fainted = true;
            if (!select_party_member_in_battle(env.console, env.context, 1)){
                return Pokemon::outcome_failed("berry_hp_switch_failed", holder);
            }
            continue;
        }
        if (wait == BattleWaitState::MENU){
            if (holder_fainted){
                flee_battle(env.console, env.context);
                break;
            }
            open_fight_and_pick_move(env.context, stall_move_slot);
            continue;
        }
        return Pokemon::outcome_failed("berry_hp_battle_timeout", holder);
    }
    wait_out_battle_exit(env.console, env.context);
    if (!holder_fainted){
        return Pokemon::outcome_failed("berry_hp_did_not_faint", holder);
    }
    return Pokemon::outcome_ok();
}

bool drain_pp_in_wild_battle(StoryEnv& env, size_t move, size_t backup, int& remaining){
    bool move_prompt = false;
    for (int turn = 0; turn < SWITCH_GRIND_MAX_TURNS; turn++){
        const BattleWaitState state = wait_battle_menu_or_end(env.console, env.context, 0, move_prompt);
        if (state == BattleWaitState::OVERWORLD){
            wait_out_battle_exit(env.console, env.context);
            return true;
        }
        if (state == BattleWaitState::OUT_OF_PP && remaining >= 0){
            pbf_press_button(env.context, BUTTON_B, 160ms, 500ms);
            pbf_press_button(env.context, BUTTON_B, 160ms, 500ms);
            remaining = 0;
            continue;
        }
        if (state != BattleWaitState::MENU){
            return false;
        }
        if (const VideoSnapshot screen = env.console.video().snapshot()){
            const PartyStatus status = read_party_status(screen);
            env.supplies.status = status.status;
            env.supplies.hurt = status.hp_seen && status.hp_fill < 0.80;
        }
        open_fight_and_pick_move(env.context, remaining > 0 ? move : backup);
        if (remaining > 0){
            remaining--;
        }
    }
    return false;
}

bool restored_single_digit_pp(StoryEnv& env, size_t move, int expected_pp){
    dpad(env.context, DPAD_LEFT);
    dpad(env.context, DPAD_UP);
    pbf_press_button(env.context, BUTTON_A, 160ms, 900ms);
    dpad(env.context, DPAD_LEFT);
    dpad(env.context, DPAD_UP);
    if (move % 2 == 1){
        dpad(env.context, DPAD_RIGHT);
    }
    if (move >= 2){
        dpad(env.context, DPAD_DOWN);
    }
    BattleSelectionArrowWatcher selection(COLOR_RED, &env.console.overlay(), static_cast<BattleMoveOption>(move));
    env.context.wait_for_all_requests();
    if (wait_until(env.console, env.context, 4000ms, {selection}) != 0){
        return false;
    }
    const VideoSnapshot screen = env.console.video().snapshot();
    if (!screen){
        return false;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageViewRGB32 digits = extract_box_reference(game, ImageFloatBox(207.0 / 240, 122.0 / 160, 7.0 / 240, 13.0 / 160));
    const int pp = read_digits_waterfill_template(env.console.logger(), digits, DigitTemplateType::StatBox);
    pbf_press_button(env.context, BUTTON_B, 160ms, 500ms);
    env.console.log(std::format("MailGlitch: after the opening turn, the depleted move now reads {} PP (expected {}).", pp, expected_pp));
    return pp == expected_pp;
}

Outcome let_berry_holder_eat(StoryEnv& env, size_t harmless_move, size_t holder_attack){
    bool move_prompt = false;
    if (wait_battle_menu_or_end(env.console, env.context, 0, move_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_berry_turn", "opening command menu");
    }
    open_fight_and_pick_move(env.context, harmless_move);
    pbf_press_button(env.context, BUTTON_A, 160ms, 500ms);
    if (wait_battle_menu_or_end(env.console, env.context, 0, move_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_berry_turn", "berry holder's command menu");
    }
    open_fight_and_pick_move(env.context, holder_attack);
    pbf_press_button(env.context, BUTTON_A, 160ms, 500ms);
    return Pokemon::outcome_ok();
}

Outcome execute_double_battle_setup_turns(
    StoryEnv& env, size_t knock_off_slot, size_t recycle_slot, size_t depleted_slot, int restored_pp
){
    bool dummy_prompt = false;
    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_t1_slot1", "Turn 1 Slot 1 menu");
    }
    env.console.log("MailGlitch: Turn 1 Slot 1 (CH'DING) -> Knock Off targeting Ally Slot 2 (DPAD_DOWN, A).");
    open_fight_and_pick_move(env.context, knock_off_slot);
    pbf_wait(env.context, 400ms);
    dpad(env.context, DPAD_DOWN);
    pbf_press_button(env.context, BUTTON_A, 160ms, 800ms);

    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_t1_slot2", "Turn 1 Slot 2 menu");
    }
    if (!restored_single_digit_pp(env, depleted_slot, restored_pp)){
        return Pokemon::outcome_failed("double_battle_berry_not_consumed", "Leppa did not visibly restore PP");
    }
    env.console.log("MailGlitch: Switch Blastoise out for MIMIEN while Knock Off targets its position.");
    if (!battle_switch_to_slot(env.console, env.context, 2)){
        return Pokemon::outcome_failed("double_battle_switch_mimien", "Turn 1 Slot 2 switch");
    }

    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_t2_slot1", "Turn 2 Slot 1 menu");
    }
    env.console.log("MailGlitch: Switch CH'DING out for Blastoise.");
    if (!battle_switch_to_slot(env.console, env.context, 2)){
        return Pokemon::outcome_failed("double_battle_switch_sweeper", "Turn 2 Slot 1 switch");
    }

    if (wait_battle_menu_or_end(env.console, env.context, 0, dummy_prompt) != BattleWaitState::MENU){
        return Pokemon::outcome_failed("double_battle_no_t2_slot2", "Turn 2 Slot 2 menu");
    }
    env.console.log("MailGlitch: Turn 2 Slot 2 (MIMIEN) -> Recycle (restores Berry over Retro Mail!).");
    open_fight_and_pick_move(env.context, recycle_slot);
    return Pokemon::outcome_ok();
}

Outcome sweep_double_battle(StoryEnv& env, size_t surf_slot, size_t recycle_slot){
    bool dummy_prompt = false;
    for (int action = 0; action < 20; action++){
        const BattleWaitState wait = wait_battle_menu_or_end(
            env.console, env.context, 0, dummy_prompt, /*confirm_yes_no_with_a=*/false, /*probe_overworld=*/true
        );
        if (wait == BattleWaitState::OVERWORLD){
            return Pokemon::outcome_ok();
        }
        if (wait != BattleWaitState::MENU){
            return Pokemon::outcome_failed("double_battle_sweep_timeout", "Lea & Jed");
        }
        open_fight_and_pick_move(env.context, action % 2 == 0 ? surf_slot : recycle_slot);
    }
    return Pokemon::outcome_failed("double_battle_sweep_turn_limit", "Lea & Jed");
}

} // namespace


SweeperMovePool::SweeperMovePool(const SummaryRead& summary){
    for (size_t i = 0; i < summary.moves.size(); i++){
        const std::string& slug = summary.moves[i].move;
        const MoveData* data = move_data(slug);
        const int prio = move_attack_priority(slug, data);
        if (prio <= 0){
            continue;
        }
        const int pp = (data != nullptr && data->pp > 0) ? static_cast<int>(data->pp) : 10;
        m_moves.push_back(SweeperMoveEntry{
            .slot = i, .slug = slug, .priority = prio, .max_pp = pp, .remaining_pp = pp,
        });
    }
    std::sort(m_moves.begin(), m_moves.end(), [](const SweeperMoveEntry& a, const SweeperMoveEntry& b){
        if (a.priority != b.priority){
            return a.priority > b.priority;
        }
        return a.slot < b.slot;
    });
    if (m_moves.empty()){
        m_moves.push_back(SweeperMoveEntry{
            .slot = 0, .slug = "slot1", .priority = 10, .max_pp = 15, .remaining_pp = 15,
        });
    }
}

size_t SweeperMovePool::pick_slot() const{
    for (const SweeperMoveEntry& entry : m_moves){
        if (entry.remaining_pp > 0){
            return entry.slot;
        }
    }
    return m_moves.front().slot;
}

void SweeperMovePool::note_used(size_t slot){
    for (SweeperMoveEntry& entry : m_moves){
        if (entry.slot == slot && entry.remaining_pp > 0){
            entry.remaining_pp--;
            return;
        }
    }
}

void SweeperMovePool::mark_empty(size_t slot){
    for (SweeperMoveEntry& entry : m_moves){
        if (entry.slot == slot){
            entry.remaining_pp = 0;
            return;
        }
    }
}

void SweeperMovePool::restore_all(){
    for (SweeperMoveEntry& entry : m_moves){
        entry.remaining_pp = entry.max_pp;
    }
}

bool SweeperMovePool::low_pp() const{
    int total = 0;
    for (const SweeperMoveEntry& entry : m_moves){
        total += entry.remaining_pp;
    }
    return total <= 6;
}

bool should_check_grind_summary(
    unsigned current_level, unsigned target_level, int64_t battles_since_check, bool saw_move_prompt
){
    if (saw_move_prompt || current_level + 2 >= target_level){
        return true;
    }
    const int64_t interval = (current_level + 6 >= target_level) ? 5 : 12;
    return battles_since_check >= interval;
}


std::string grind_done_flag(std::string_view species){
    return std::format("mail_glitch_grind_done:{}", species);
}

bool mail_glitch_grind_done(const std::set<std::string>& flags){
    return flags.contains(grind_done_flag("farfetchd")) && flags.contains(grind_done_flag("mr-mime"));
}

Outcome run_switch_grind_step(StoryEnv& env, RunState& state, const StoryStep& step){
    const GrindConfig cfg = parse_grind_config(step);
    const std::string done_flag = grind_done_flag(cfg.trainee);
    if (state.flags.contains(done_flag)){
        return Pokemon::outcome_ok();
    }
    if (cfg.batch_battles > 0 && state.flags.contains(std::string(GRIND_BATCH_USED_FLAG))){
        return Pokemon::outcome_ok();
    }
    const std::optional<size_t> trainee_slot = find_party_slot_by_species(env, cfg.trainee);
    if (!trainee_slot){
        return Pokemon::outcome_failed("grind_missing_trainee", cfg.trainee);
    }
    const std::optional<SummaryRead> summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, *trainee_slot);
    if (!summary || !summary->level){
        return Pokemon::outcome_failed("grind_unread_level", cfg.trainee);
    }
    const unsigned current_level = *summary->level;
    if (current_level >= cfg.target_level && find_move_slot(*summary, cfg.learn_move).has_value()){
        state.flags.insert(done_flag);
        env.console.log(std::format("MailGlitch: {} is already Lv {} with {}; skipping switch_grind.", cfg.trainee, current_level, cfg.learn_move));
        return Pokemon::outcome_ok();
    }

    std::string setup_err;
    const std::optional<SummaryRead> sweeper_info = setup_grind_party(env, cfg.trainee, cfg.sweeper, *trainee_slot, setup_err);
    if (!sweeper_info){
        return Pokemon::outcome_failed(setup_err, cfg.trainee);
    }
    SweeperMovePool moves(*sweeper_info);
    env.console.log(std::format(
        "MailGlitch: switch-grinding {} (Lv {} -> {}, batch={}) with {} ({} attacking moves, primary slot {}) for {}.",
        cfg.trainee, current_level, cfg.target_level, cfg.batch_battles, cfg.sweeper, moves.entries().size(), moves.pick_slot() + 1, cfg.learn_move
    ));
    return run_switch_grind_loop(env, state, cfg, moves, current_level);
}

Outcome run_clear_purified_zone_step(StoryEnv& env, RunState&, const StoryStep& step){
    const Direction facing = Pokemon::story_arg<Direction>(step.args, "facing");
    env.console.log("MailGlitch: waiting for Pokemon Tower 5F PurifiedZone heal chime and clearing dialog.");
    pbf_wait(env.context, 2200ms);
    mash_until_overworld(env.console, env.context, SCENE_TIMEOUT);
    turn_to_facing_no_step(env, facing);
    return Pokemon::outcome_ok();
}

Outcome run_prepare_berry_hp_step(StoryEnv& env, RunState&, const StoryStep& step){
    const std::string& holder = Pokemon::story_arg<std::string>(step.args, "species");
    const std::string& sweeper = Pokemon::story_arg<std::string>(step.args, "sweeper");

    const std::optional<size_t> holder_slot = find_party_slot_by_species(env, holder);
    if (!holder_slot){
        return Pokemon::outcome_failed("berry_hp_missing_holder", holder);
    }
    std::string setup_err;
    if (!setup_grind_party(env, holder, sweeper, *holder_slot, setup_err)){
        return Pokemon::outcome_failed("berry_hp_swap_failed", holder);
    }
    const std::optional<SummaryRead> holder_summary = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
    const size_t stall_move_slot = holder_summary ? pick_non_damaging_move_slot(*holder_summary) : 0;
    env.console.log(std::format(
        "MailGlitch: fainting {} (using stall move slot {}) in wild battle to Revive to exact maxHP / 2.",
        holder, stall_move_slot + 1
    ));
    bool fainted = false;
    for (int attempt = 1; attempt <= 4; attempt++){
        if (!spin_for_encounter(env)){
            return Pokemon::outcome_failed("berry_hp_no_encounter", holder);
        }
        if (handle_encounter(env.console, env.context, true)){
            throw ProgramFinishedException("MailGlitch: wild shiny encountered during prepare_berry_hp!", env.console);
        }
        const Outcome battle_res = faint_holder_and_flee(env, holder, stall_move_slot);
        if (battle_res.succeeded()){
            fainted = true;
            break;
        }
        if (battle_res.failure != "berry_hp_did_not_faint"){
            return battle_res;
        }
        env.console.log(
            std::format("MailGlitch: {} did not faint in wild battle {} (e.g. wild Roar); retrying.", holder, attempt),
            COLOR_ORANGE
        );
    }
    if (!fainted){
        return Pokemon::outcome_failed("berry_hp_did_not_faint", holder);
    }
    if (!use_revive_on_slot(env, 0)){
        return Pokemon::outcome_failed("berry_hp_no_revive", holder);
    }
    env.console.log(std::format("MailGlitch: {} revived to exact maxHP / 2.", holder), COLOR_BLUE);
    return Pokemon::outcome_ok();
}

Outcome run_prepare_berry_pp_step(StoryEnv& env, const StoryStep& step){
    const std::string& species = Pokemon::story_arg<std::string>(step.args, "species");
    const std::string& move = Pokemon::story_arg<std::string>(step.args, "move");
    const std::string& backup_move = Pokemon::story_arg<std::string>(step.args, "backup_move");
    const auto lead = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
    if (!lead || lead->species != species){
        return Pokemon::outcome_failed("berry_pp_wrong_lead", species);
    }
    const auto slot = find_move_slot(*lead, move);
    const auto backup = find_move_slot(*lead, backup_move);
    const MoveData* data = move_data(move);
    if (!slot || !backup || !data || data->pp <= 0){
        return Pokemon::outcome_failed("berry_pp_missing_move", move);
    }
    const SummaryMove& attack = lead->moves[*slot];
    int remaining = attack.pp >= 0 ? attack.pp : (attack.max_pp >= 0 ? attack.max_pp : data->pp);
    while (remaining > 0){
        if (!spin_for_encounter(env)){
            return Pokemon::outcome_failed("berry_pp_no_encounter", species);
        }
        if (handle_encounter(env.console, env.context, true)){
            throw ProgramFinishedException("MailGlitch: shiny found while preparing Berry PP.", env.console);
        }
        if (!drain_pp_in_wild_battle(env, *slot, *backup, remaining)){
            return Pokemon::outcome_failed("berry_pp_battle_failed", move);
        }
        recover_after_battle(env.console, env.context, env.supplies, env.checks, env.language);
        if (env.supplies.hurt && env.supplies.super_potions > 0){
            use_recovery_item(env.console, env.context, "super-potion", env.checks, env.language, env.supplies);
        }
        if (env.supplies.status == StatusCondition::POISON){
            return Pokemon::outcome_failed("berry_pp_poisoned_lead", species);
        }
        env.console.log(std::format("MailGlitch: {} has {} PP remaining to deplete.", move, remaining));
    }
    if (step.pose_after){
        if (const auto after = story_pose(Pokemon::pose_of(*step.pose_after)); after && after->facing){
            turn_to_facing_no_step(env, *after->facing);
        }
    }
    return Pokemon::outcome_ok();
}

Outcome run_mail_glitch_double_battle_step(StoryEnv& env, RunState&, const StoryStep& step){
    const Direction step_dir = Pokemon::story_arg<Direction>(step.args, "step_direction");
    const std::string& move = Pokemon::story_arg<std::string>(step.args, "depleted_move");
    const auto lead = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 0);
    const auto holder = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 1);
    const auto recycler = read_party_summary(env.console, env.context, StartMenu::FULL, env.language, 2);
    const std::string& knock_off_species = Pokemon::story_arg<std::string>(step.args, "knock_off_species");
    if (!lead || !holder || !recycler
        || (lead->species != knock_off_species && !(knock_off_species == "farfetchd" && lead->species == "farfetch-d"))
        || holder->species != Pokemon::story_arg<std::string>(step.args, "sweeper")
        || recycler->species != Pokemon::story_arg<std::string>(step.args, "recycle_species")
    ){
        return Pokemon::outcome_failed("double_battle_wrong_party", "expected CH'DING, Blastoise, MIMIEN");
    }
    const auto knock_off = find_move_slot(*lead, "knock-off");
    const auto recycle = find_move_slot(*recycler, "recycle");
    const auto surf = find_move_slot(*holder, "surf");
    const auto bite = find_move_slot(*holder, "bite");
    const auto sand_attack = find_move_slot(*lead, "sand-attack");
    const auto depleted = find_move_slot(*holder, move);
    const MoveData* data = move_data(move);
    if (!knock_off || !recycle || !surf || !bite || !sand_attack || !depleted || !data || data->pp <= 0 || data->pp > 9){
        return Pokemon::outcome_failed("double_battle_missing_move", "check the battle party's moves");
    }
    if (holder->moves[*depleted].pp > 0){
        return Pokemon::outcome_failed("double_battle_move_not_depleted", move);
    }
    env.console.log("MailGlitch: starting Lea & Jed's double battle.");
    step_once(env.console, env.context, step_dir, step_dir, env.player_gender);
    if (advance_text(env.console, env.context, 30000ms, true) != TextEnd::BATTLE){
        return Pokemon::outcome_failed("double_battle_not_started", "Lea & Jed");
    }
    const Outcome berry = let_berry_holder_eat(env, *sand_attack, *bite);
    if (!berry.succeeded()){
        return berry;
    }
    const Outcome opening = execute_double_battle_setup_turns(env, *knock_off, *recycle, *depleted, data->pp);
    if (!opening.succeeded()){
        return opening;
    }
    const Outcome sweep = sweep_double_battle(env, *surf, *recycle);
    if (sweep.succeeded()){
        wait_out_battle_exit(env.console, env.context);
    }
    return sweep;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
