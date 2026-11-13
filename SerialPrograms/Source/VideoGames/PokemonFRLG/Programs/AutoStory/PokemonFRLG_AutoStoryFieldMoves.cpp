/*  AutoStory Field Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_PartyDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Map/PokemonFRLG_MapDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_FlyMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldFacing.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecoveryWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::StoryStep;


namespace{

//  A Pokemon's menu has at most SUMMARY, four field moves, SWITCH, ITEM
//  and CANCEL (the party's six slots are PARTY_SLOTS).
constexpr size_t MON_MENU_ROWS = 8;

} // namespace

std::optional<FlyPlan> parse_fly_step(const StoryStep& step){
    if (step.action != FLY_VARIANT){
        return std::nullopt;
    }
    const std::string* town = game_arg<std::string>(step.args, "town");
    const std::optional<size_t> index = town == nullptr ? std::nullopt : fly_town_index(*town);
    const std::string* species = game_arg<std::string>(step.args, "species");
    //  A slot or a species, not both.
    const std::optional<size_t> slot = species != nullptr
        ? (step.args.contains("slot") || species->empty() ? std::nullopt : std::optional<size_t>(0))
        : counted_arg(step.args, "slot", PARTY_SLOTS);
    const std::optional<size_t> menu_row = step.args.contains("menu_row")
        ? counted_arg(step.args, "menu_row", MON_MENU_ROWS)
        : std::optional<size_t>(FlyPlan{}.menu_row);
    if (!index || !slot || !menu_row){
        return std::nullopt;
    }
    return FlyPlan{
        .slot = *slot, .species = species == nullptr ? "" : *species, .menu_row = *menu_row, .town = *index,
        .start_menu = step_start_menu(step.args),
    };
}

std::optional<size_t> FlyPlan::party_slot(const MemberReader& reader) const{
    if (species.empty()){
        return slot;
    }
    return find_party_species(reader, species);
}

std::optional<Direction> parse_surf_step(const StoryStep& step){
    const Direction* direction = step.action == SURF_VARIANT ? game_arg<Direction>(step.args, "direction") : nullptr;
    if (direction == nullptr || step.args.size() != 1){
        return std::nullopt;
    }
    return *direction;
}

bool is_field_move_step(const StoryStep& step){
    return parse_fly_step(step).has_value() || parse_surf_step(step).has_value();
}

std::vector<Pokemon::StoryActionSpec> field_move_specs(){
    using Pokemon::StoryArgType;
    using Pokemon::StoryStepKind;
    return {
        {.kind = StoryStepKind::GAME, .name = FLY_VARIANT, .args = {
            required_arg("town", StoryArgType::WORD, "the town's map, as kanto_fly_towns() names it (CeladonCity, Route4)"),
            absent_arg("slot", StoryArgType::INTEGER, "the Fly user's party slot, from 1, when no species is named"),
            absent_arg("species", StoryArgType::WORD, "the Fly user by its species, e.g. \"pidgey\": the first slot whose summary reads it"),
            optional_arg("menu_row", StoryArgType::INTEGER, int64_t(2), "FLY's row on its menu, from 1: SUMMARY, then field moves in move order"),
            optional_arg("safari", StoryArgType::BOOLEAN, false, "in the Safari Zone, whose START menu has RETIRE on top"),
        }},
        {.kind = StoryStepKind::GAME, .name = SURF_VARIANT, .args = {
            required_arg("direction", StoryArgType::DIRECTION, "the way to the water, from ground beside it"),
        }},
    };
}


namespace{

//  Not measured on the console: the party menu's and the fly map's cursor
//  taps, as the lift panel's (PokemonFRLG_AutoStoryHideout.cpp). A fly map
//  cursor step takes 4 frames (HandleRegionMapInput(): 2 pixels a frame,
//  8 a cell); the release leaves it time to stop.
constexpr Milliseconds MENU_TAP = 80ms;
constexpr Milliseconds MENU_TAP_RELEASE = 240ms;
constexpr Milliseconds CURSOR_TAP = 80ms;
constexpr Milliseconds CURSOR_TAP_RELEASE = 170ms;
//  Up-left to the map's corner: 21 cells at 4 frames each is 1.4 s.
constexpr Milliseconds CURSOR_TO_CORNER = 3000ms;
//  The Pokemon's menu, after its A.
constexpr Milliseconds MON_MENU_TIMEOUT = 5000ms;
//  The fly map, after FLY's A, and before its cursor takes the D-pad
//  (open_fly_map_from_overworld()'s wait).
constexpr Milliseconds FLY_MAP_TIMEOUT = 20000ms;
constexpr Milliseconds FLY_MAP_SETTLE = 1000ms;
//  The fade after the town's A: none, the town wasn't one Fly goes to. The
//  flight's black band is no fade (FlyLanding).
constexpr Milliseconds TAKEOFF_TIMEOUT = 10000ms;
//  The flight and the landing, to the player on screen at rest.
constexpr Milliseconds LANDING_TIMEOUT = 30000ms;

void tap_dpad(ProControllerContext& context, DpadPosition dpad, size_t count, Milliseconds tap, Milliseconds release){
    for (size_t c = 0; c < count; c++){
        pbf_press_dpad(context, dpad, tap, release);
    }
}

//  From the overworld to the fly map: the party, the Fly user, FLY.
//  Returns the failure, or empty.
//  Overlaps open_fly_map_from_overworld() in PokemonFRLG_Navigation.h.
//  Ours picks the Fly user by slot or species and FLY's menu row, and returns a failure code instead of
//  throwing; upstream assumes the last party slot and the second menu row.
//  Could be unified with upstream if needed.
std::string open_fly_map(ConsoleHandle& console, ProControllerContext& context, const FlyPlan& plan){
    open_party_menu_from_overworld(console, context, start_menu_context(plan.start_menu));
    tap_dpad(context, DPAD_DOWN, plan.slot, MENU_TAP, MENU_TAP_RELEASE);
    PartySelectionWatcher mon_menu(COLOR_RED);
    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    if (wait_until(console, context, MON_MENU_TIMEOUT, {mon_menu}) < 0){
        return "fly_no_menu";
    }
    tap_dpad(context, DPAD_DOWN, plan.menu_row, MENU_TAP, MENU_TAP_RELEASE);
    KantoMapWatcher fly_map(COLOR_RED);
    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    if (wait_until(console, context, FLY_MAP_TIMEOUT, {fly_map}) < 0){
        return "fly_no_map";
    }
    pbf_wait(context, FLY_MAP_SETTLE);
    return "";
}

//  On the fly map: the cursor to the corner, then to the town, and A.
//  Returns the failure, or empty once the player has landed: a fade has
//  passed, the overworld is at rest, it is no longer `departure` (the
//  overworld shot before the Fly menu) and the player's facing reads.
//  Overlaps fly_from_kanto_map() in PokemonFRLG_Navigation.h.
//  Ours moves the cursor by map cell from a town table and waits for a quiet overworld after landing;
//  upstream uses timed stick holds per KantoFlyLocation and a blind wait.
//  Could be unified with upstream if needed.
std::string fly_to_town(
    ConsoleHandle& console, ProControllerContext& context, PlayerGender player,
    const FlyTown& town, const VideoSnapshot& departure
){
    pbf_press_dpad(context, DPAD_UP_LEFT, CURSOR_TO_CORNER, CURSOR_TAP_RELEASE);
    tap_dpad(context, DPAD_RIGHT, (size_t)town.cell.x, CURSOR_TAP, CURSOR_TAP_RELEASE);
    tap_dpad(context, DPAD_DOWN, (size_t)town.cell.y, CURSOR_TAP, CURSOR_TAP_RELEASE);
    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    context.wait_for_all_requests();
    //  The look goes on from the A: a fade, then a quiet overworld with the
    //  player on screen. The band of the flight and the departure town's
    //  overworld are quiet too (console B, 2026-11-12 23:59, 12.03), so a
    //  quiet screen alone is no landing; nor is a quiet departure town after
    //  the fly map's fade (2026-11-13 02:46).
    const auto has_left = [&]{
        const VideoSnapshot now = console.video().snapshot();
        if (!departure || !now){
            return false;
        }
        const double change = departure_change(*departure.frame, *now.frame, GameSettings::instance().GAME_BOX);
        console.log(std::format("Story: the screen differs from the departure town by {:.1f}.", change));
        return left_departure(*departure.frame, *now.frame, GameSettings::instance().GAME_BOX);
    };
    ConsoleScreenLook eyes(console, context);
    const WallClock start = current_time();
    FlyLanding landing;
    bool fresh = true;
    do{
        const Pokemon::ScreenLook look = eyes.look(fresh);
        fresh = false;
        landing = judge_fly_landing(look, MovementMechanics::AFTER_BATTLE_QUIET, has_left, [&]{
            return OverworldWalkSteps::read_facing(console, context, player);
        });
    }while (!landing.landed() && current_time() - start < TAKEOFF_TIMEOUT + LANDING_TIMEOUT);
    console.log(std::format(
        "Story: {} {}, {}", landing.landed() ? "landed in" : "no landing in", town.name, describe(landing)
    ), landing.landed() ? COLOR_WHITE : COLOR_RED);
    const std::string shot = save_route_screenshot(console, "fly", std::format("landed in {}", town.name));
    if (!shot.empty()){
        console.log("Story: landing screenshot " + shot);
    }
    if (landing.landed()){
        return "";
    }
    return landing.faded ? "fly_no_landing" : "fly_no_takeoff";
}

} // namespace

std::optional<Pokemon::Outcome> run_field_move_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, PlayerGender player, const StoryStep& step
){
    const auto done = [&step]{
        return step.pose_after ? Pokemon::outcome_ok(Pokemon::pose_of(*step.pose_after)) : Pokemon::outcome_ok();
    };
    if (const std::optional<Direction> water = parse_surf_step(step)){
        console.log(std::format("Story: Surf {}.", Pokemon::direction_name(*water)));
        OverworldWalkSteps::start_surf(console, context, *water, false);
        return done();
    }
    std::optional<FlyPlan> plan = parse_fly_step(step);
    if (!plan){
        return std::nullopt;
    }
    const FlyTown& town = kanto_fly_towns()[plan->town];
    context.wait_for_all_requests();
    const VideoSnapshot departure = console.video().snapshot();
    const std::optional<size_t> slot = plan->party_slot(console_member_reader(console, context, language, plan->start_menu));
    if (!slot){
        console.log(std::format("Story: no {} read in the party to Fly with.", plan->species), COLOR_RED);
        return Pokemon::outcome_failed("fly_no_member", plan->species);
    }
    plan->slot = *slot;
    console.log(std::format(
        "Story: Fly with slot {}, menu row {}, to {} (cell {}, {}).",
        plan->slot + 1, plan->menu_row + 1, town.name, town.cell.x, town.cell.y
    ));
    std::string failure = open_fly_map(console, context, *plan);
    if (failure.empty()){
        failure = fly_to_town(console, context, player, town, departure);
    }
    return failure.empty() ? done() : Pokemon::outcome_failed(failure, town.name);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
