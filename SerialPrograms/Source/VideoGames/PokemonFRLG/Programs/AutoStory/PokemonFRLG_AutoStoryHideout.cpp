/*  AutoStory Rocket Hideout Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


const std::vector<std::string>& rocket_hideout_lift_menu(){
    static const std::vector<std::string> MENU{"B1F", "B2F", "B4F", "EXIT"};
    return MENU;
}

namespace{

//  The row of `floor` on the panel's menu, if it is a floor (not EXIT).
std::optional<size_t> floor_row(std::string_view floor){
    const std::vector<std::string>& menu = rocket_hideout_lift_menu();
    for (size_t row = 0; row + 1 < menu.size(); row++){
        if (menu[row] == floor){
            return row;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<LiftRide> parse_lift_floor_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, LIFT_FLOOR_VARIANT, LIFT_FLOOR_ARGS);
    if (args == nullptr){
        return std::nullopt;
    }
    const std::optional<size_t> from = floor_row(*Pokemon::story_arg_text(*args, "from"));
    const std::optional<size_t> to = floor_row(*Pokemon::story_arg_text(*args, "to"));
    if (!from || !to){
        return std::nullopt;
    }
    return LiftRide{*from, *to};
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
    return (step.variant == POSTER_SWITCH_VARIANT && step.args.empty()) || parse_lift_floor_step(step).has_value();
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
    const MenuPresses presses = lift_menu_presses(ride, rocket_hideout_lift_menu().size());
    console.log(
        "Story: lift from " + rocket_hideout_lift_menu()[ride.from] + " to " + rocket_hideout_lift_menu()[ride.to]
        + ": " + std::to_string(presses.presses) + " press(es) " + Pokemon::direction_name(presses.direction) + "."
    );
    pbf_press_button(context, BUTTON_A, 160ms, LIFT_MENU_WAIT);
    const DpadPosition dpad = presses.direction == Pokemon::Direction::SOUTH ? DPAD_DOWN : DPAD_UP;
    for (size_t c = 0; c < presses.presses; c++){
        pbf_press_dpad(context, dpad, MENU_TAP, MENU_TAP_RELEASE);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    //  B during the ride does nothing; the menu took the A.
    mash_until_overworld(console, context, HIDEOUT_SCENE_TIMEOUT);
}

void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride){
    walk_room_legs(console, context, lift_in_legs(), Pokemon::Direction::NORTH);
    pick_lift_floor(console, context, ride);
    walk_room_legs(console, context, lift_out_legs(), Pokemon::Direction::NORTH);
}

void push_poster_switch(ConsoleHandle& console, ProControllerContext& context){
    console.log("Story: the switch behind the poster.");
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    mash_until_overworld(console, context, HIDEOUT_SCENE_TIMEOUT);
}

std::optional<Pokemon::Outcome> run_field_game_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, const Pokemon::StoryStep& step
){
    const auto done = [&step]{
        return step.pose_after ? Pokemon::outcome_ok(*step.pose_after) : Pokemon::outcome_ok();
    };
    if (const std::optional<LiftRide> ride = parse_lift_floor_step(step)){
        ride_lift(console, context, *ride);
        return done();
    }
    if (is_center_variant(step.variant)){
        //  Both start facing north: at the door, or at the nurse.
        walk_room_legs(console, context, center_legs(step.variant), Pokemon::Direction::NORTH);
        return done();
    }
    if (step.variant == POSTER_SWITCH_VARIANT){
        push_poster_switch(console, context);
        return Pokemon::outcome_ok();
    }
    if (!is_item_use_step(step)){
        return std::nullopt;
    }
    const std::string item = parse_use_item_step(step).value_or("bicycle");
    ConsoleItemUseActions bag(console, context, language);
    const ItemUseEnd end = use_item_from_bag(console.logger(), bag, item);
    if (end != ItemUseEnd::USED){
        return Pokemon::outcome_failed(std::string("use_item_") + item_use_end_name(end), step.variant);
    }
    return done();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
