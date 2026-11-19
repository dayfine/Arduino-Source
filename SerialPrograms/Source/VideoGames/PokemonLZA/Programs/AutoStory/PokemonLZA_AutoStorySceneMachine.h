/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Presses through a run of scenes to free control, as ScreenMachine
 *  states that match on backend observations (console or scripted).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  When a scene machine gives up on a quiet screen, and when it presses A blind.
struct SceneMachineConfig{
    //  Fails ("scene_quiet") once nothing busy (text, a fade) and no sign of
    //  control has shown for this long...
    std::optional<Milliseconds> quiet = std::nullopt;
    //  ...and this long has passed since the start: a cutscene can hold a
    //  still frame longer than `quiet`.
    Milliseconds min_length = Milliseconds(0);
    //  Presses `blind_button` every `blind_press_every` once nothing busy and
    //  no control has shown for this long, for screens no detector reads
    //  (03.04's Pokedex). Only then: a blind A at the end of a talk restarts it.
    std::optional<Milliseconds> blind_press_after = std::nullopt;
    //  Zero: on every look, which closes the menus a talk opens (03.11).
    Milliseconds blind_press_every = Milliseconds(2000);
    //  Once a scene is under way, one blind press after this long quiet, then
    //  again only after the next cue: an unread box inside a scene (03.07).
    std::optional<Milliseconds> gap_press = std::nullopt;
    //  B on a screen A does not close (03.04's Pokedex entry).
    Button blind_button = BUTTON_A;
    bool stop_at_choice = false;
    //  Ends at the battle HUD or a lock-on. The HUD shows only while locked
    //  on, so after a black fade a still screen (3 s) or the party bar (1 s)
    //  gets ZL held while `lock_on` watches for the move symbols, then up to
    //  4 quarter turns + L. A cutscene (moving, no party bar) gets no ZL.
    bool stop_at_battle = false;
    //  None: no lock-on (a backend that models none).
    std::optional<LockOnWatch> lock_on = std::nullopt;
    //  The button for text boxes and a lone advance chevron. None: B in a
    //  machine that ends in control (A on a closing box can restart the talk),
    //  else A. Set A for a scene whose choice a B would cancel (03.01's lobby).
    std::optional<Button> text_button = std::nullopt;
    //  The quiet end is control: Hotel Z shows no party bar or prompt.
    bool quiet_is_control = false;
    //  The step plays a scene: the HUD ends it only once a scene is under way.
    //  Fails ("scene_never_started") when the HUD shows this long with no
    //  scene: the player is not at the trigger.
    std::optional<Milliseconds> start_window = std::nullopt;
    //  Ends once this many black fades have passed and then 5 s with no text,
    //  fade or banner (or on the controls hint). The mission banner is part of
    //  the scene: up 2 s it gets A, up to 3 times; one that stays fails.
    //  (01.01: the banner detector misses it on the Italian game.)
    std::optional<size_t> end_after_black_fades = std::nullopt;
};

//  Text, a choice, a battle, a tutorial card, or with no HUD up a fade or a
//  moving screen (a cutscene can play a minute with no text).
bool scene_under_way(const ScreenObservation& look);

//  A text box, or the advance chevron without one.
bool text_shows(const ScreenObservation& look);

//  A black or a white fade.
bool fade_shows(const ScreenObservation& look);

//  The party bar with no text box over it (it shows between a talk's lines too).
bool party_bar_free(const ScreenObservation& look);

//  "text box, advance arrow": `look`'s events and readings; "nothing" for none.
std::string describe_scene_look(const ScreenObservation& look);

//  Presses through scenes until control: the party bar or controls hint held
//  1 s, an A prompt held 1 s past `min_length`, or the mission banner on two
//  looks. A choice takes its top row. Fails as "scene_quiet" (unless
//  `quiet_is_control`), "scene_never_started", "scene_timeout" (by the looks'
//  time) or "scene_stuck" (a state that recurs too often, or a frozen screen).
//  For which config fields a run still needs, logs "[SceneQuirk] <step>
//  <field> fired" the first time each changes what the machine does, and
//  "[PlusSkip] <step> pressed -> prompt|skipped|map|nothing" per Plus held;
//  `step` is "<checkpoint>/<step>".
std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout, std::string_view step
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
