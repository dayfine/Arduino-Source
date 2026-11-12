/*  AutoStory Rocket Hideout Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRoomLegs.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


const std::vector<std::string>& lift_menu(std::string_view lift){
    static const std::vector<std::string> NONE;
    static const std::vector<std::string> HIDEOUT{"B1F", "B2F", "B4F", "EXIT"};
    static const std::vector<std::string> STORE{"5F", "4F", "3F", "2F", "1F", "EXIT"};
    if (lift == LIFT_MAP){
        return HIDEOUT;
    }
    if (lift == DEPT_STORE_LIFT_MAP){
        return STORE;
    }
    return NONE;
}

const std::vector<MapPosition>& lift_exits(std::string_view lift){
    static const std::vector<MapPosition> NONE;
    if (lift == LIFT_MAP){
        return rocket_hideout_lift_exits();
    }
    if (lift == DEPT_STORE_LIFT_MAP){
        return department_store_lift_exits();
    }
    return NONE;
}

namespace{

//  The row of `floor` on `menu`, if it is a floor (not EXIT).
std::optional<size_t> floor_row(const std::vector<std::string>& menu, std::string_view floor){
    for (size_t row = 0; row + 1 < menu.size(); row++){
        if (menu[row] == floor){
            return row;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<LiftRide> parse_lift_floor_step(const Pokemon::StoryStep& step){
    if (step.action != LIFT_FLOOR_VARIANT){
        return std::nullopt;
    }
    const std::string* from_name = game_arg<std::string>(step.args, "from");
    const std::string* to_name = game_arg<std::string>(step.args, "to");
    if (from_name == nullptr || to_name == nullptr){
        return std::nullopt;
    }
    for (std::string_view lift : {LIFT_MAP, DEPT_STORE_LIFT_MAP}){
        const std::optional<size_t> from = floor_row(lift_menu(lift), *from_name);
        const std::optional<size_t> to = floor_row(lift_menu(lift), *to_name);
        if (from && to){
            return LiftRide{*from, *to, lift};
        }
    }
    return std::nullopt;
}

MenuPresses lift_menu_presses(const LiftRide& ride, size_t entries){
    const size_t down = (ride.to + entries - ride.from) % entries;
    const size_t up = (ride.from + entries - ride.to) % entries;
    if (entries <= 3){
        //  No wrap: the cursor goes straight there.
        return ride.to >= ride.from
            ? MenuPresses{Pokemon::Direction::SOUTH, ride.to - ride.from}
            : MenuPresses{Pokemon::Direction::NORTH, ride.from - ride.to};
    }
    return down <= up ? MenuPresses{Pokemon::Direction::SOUTH, down} : MenuPresses{Pokemon::Direction::NORTH, up};
}

bool is_hideout_step(const Pokemon::StoryStep& step){
    return (step.action == POSTER_SWITCH_VARIANT && step.args.empty()) || parse_lift_floor_step(step).has_value();
}

std::vector<Pokemon::StoryActionSpec> field_action_specs(){
    using Pokemon::StoryArgType;
    using Pokemon::StoryStepKind;
    std::vector<Pokemon::StoryActionSpec> specs{
        {.kind = StoryStepKind::GAME, .name = POSTER_SWITCH_VARIANT, .args = {}},
        {.kind = StoryStepKind::GAME, .name = LIFT_FLOOR_VARIANT, .args = {
            required_arg("from", StoryArgType::WORD, "the floor the lift stands at, as the panel names it (B1F, B2F, B4F; 1F to 5F)"),
            required_arg("to", StoryArgType::WORD, "the floor it rides to"),
        }},
        {.kind = StoryStepKind::GAME, .name = USE_ITEM_VARIANT, .args = {
            required_arg("item", StoryArgType::WORD, "the item's slug, as read_item_list_name() names it"),
            optional_arg("safari", StoryArgType::BOOLEAN, false, "in the Safari Zone, whose START menu has RETIRE on top"),
        }},
        {.kind = StoryStepKind::GAME, .name = RIDE_BIKE_VARIANT, .args = {}},
        {.kind = StoryStepKind::GAME, .name = REGISTER_BIKE_VARIANT, .args = {}},
    };
    for (const std::string& center : {CENTER_ENTER_VARIANT, CENTER_EXIT_VARIANT}){
        specs.push_back({.kind = StoryStepKind::GAME, .name = center, .args = {}});
    }
    for (Pokemon::StoryActionSpec& move : field_move_specs()){
        specs.push_back(std::move(move));
    }
    return specs;
}


namespace{

//  How long a message and its menu take to show on fast text, before the
//  menu takes the D-pad. Not measured on the console.
constexpr Milliseconds LIFT_MENU_WAIT = 1500ms;
constexpr Milliseconds MENU_TAP = 80ms;
constexpr Milliseconds MENU_TAP_RELEASE = 240ms;
//  The lift's shake and the doors, then the overworld, A press to the end.
constexpr Milliseconds HIDEOUT_SCENE_TIMEOUT = 30000ms;

} // namespace

void pick_lift_floor(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride){
    const std::vector<std::string>& menu = lift_menu(ride.lift);
    const MenuPresses presses = lift_menu_presses(ride, menu.size());
    console.log(std::format(
        "Story: lift from {} to {}: {} press(es) {}.",
        menu[ride.from], menu[ride.to], presses.presses, Pokemon::direction_name(presses.direction)
    ));
    pbf_press_button(context, BUTTON_A, 160ms, LIFT_MENU_WAIT);
    const DpadPosition dpad = presses.direction == Pokemon::Direction::SOUTH ? DPAD_DOWN : DPAD_UP;
    for (size_t c = 0; c < presses.presses; c++){
        pbf_press_dpad(context, dpad, MENU_TAP, MENU_TAP_RELEASE);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    //  B during the ride does nothing; the menu took the A.
    mash_until_overworld(console, context, HIDEOUT_SCENE_TIMEOUT);
    //  The ride shakes the lift with the overworld showing, and the game
    //  drops taps until it stops (console B, 2026-11-11).
    context.wait_for_all_requests();
    ConsoleScreenLook eyes(console, context);
    const Pokemon::ScreenSettle settle = wait_for_quiet_overworld(
        [&eyes](bool fresh){ return eyes.look(fresh); },
        MovementMechanics::AFTER_BATTLE_QUIET, HIDEOUT_SCENE_TIMEOUT
    );
    console.log(std::format("Story: after the lift's ride, {}", Pokemon::describe(settle)));
}

void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride, PlayerGender player){
    walk_room_legs(console, context, lift_in_legs(), Pokemon::Direction::NORTH, player);
    pick_lift_floor(console, context, ride);
    walk_room_legs(console, context, lift_out_legs(), Pokemon::Direction::NORTH, player);
}

void push_poster_switch(ConsoleHandle& console, ProControllerContext& context){
    console.log("Story: the switch behind the poster.");
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    mash_until_overworld(console, context, HIDEOUT_SCENE_TIMEOUT);
}

std::optional<Pokemon::Outcome> run_field_game_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, PlayerGender player,
    const Pokemon::StoryStep& step
){
    const auto done = [&step]{
        return step.pose_after ? Pokemon::outcome_ok(Pokemon::pose_of(*step.pose_after)) : Pokemon::outcome_ok();
    };
    if (const std::optional<LiftRide> ride = parse_lift_floor_step(step)){
        ride_lift(console, context, *ride, player);
        return done();
    }
    if (std::optional<Pokemon::Outcome> moved = run_field_move_step(console, context, language, step)){
        return moved;
    }
    if (is_center_variant(step.action)){
        //  Both start facing north: at the door, or at the nurse.
        walk_room_legs(console, context, center_legs(step.action), Pokemon::Direction::NORTH, player);
        return done();
    }
    if (step.action == POSTER_SWITCH_VARIANT){
        push_poster_switch(console, context);
        return Pokemon::outcome_ok();
    }
    if (!is_item_use_step(step)){
        return std::nullopt;
    }
    if (step.action == RIDE_BIKE_VARIANT){
        OverworldWalkSteps::get_on_bike(console, context, player);
        return done();
    }
    ConsoleItemUseActions bag(console, context, language, step_start_menu(step.args));
    const ItemUseEnd end = step.action == REGISTER_BIKE_VARIANT
        ? register_item_in_bag(console.logger(), bag, "bicycle")
        : use_item_from_bag(console.logger(), bag, *parse_use_item_step(step));
    if (end != ItemUseEnd::USED){
        return Pokemon::outcome_failed(std::string("use_item_") + item_use_end_name(end), step.action);
    }
    return done();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
