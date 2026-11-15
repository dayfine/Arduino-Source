/*  PokemonLZA AutoStory - Dialogs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's dialogue actions (PokemonLZA_AutoStoryActions.h has the
 *  rest): through a scene to the overworld, a talk, a choice.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogs_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogs_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a dialogue or a scene, runs `presses` (repeating it) until the
//  overworld is back: OverworldPartySelectionWatcher held OVERWORLD_HOLD.
//  Throws OperationFailedException with `failure` if it is not back within
//  `timeout`.
void press_until_overworld(
    ConsoleHandle& console, ProControllerContext& context,
    const std::function<void(ProControllerContext&)>& presses,
    Milliseconds timeout, std::string_view failure
);

//  press_until_overworld() with `button` mashed.
void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout);

//  What press_a_until_quiet() pressed A through.
struct ScreensPassed{
    bool card = false;
    bool text = false;
    bool timed_out = false;     //  still busy at the timeout
};

//  press_a_until_quiet()'s clock. Both times count from the first look,
//  not from the call: the presses sent before it still run then (03.10,
//  The menu's A and its 3 s ran out the quiet time,
//  and the claim ended on one look with no dialog seen).
class QuietPresses{
public:
    //  What press_a_until_quiet() does on a look.
    enum class Step{
        PRESS_A,    //  a dialog or card shows
        WAIT,       //  nothing shows, not for `quiet` yet
        QUIET,      //  nothing has shown for `quiet`: done
        TIMED_OUT,  //  `timeout` since the first look, still busy
    };

    QuietPresses(Milliseconds quiet, Milliseconds timeout)
        : m_quiet(quiet)
        , m_timeout(timeout)
    {}

    //  A look at `now`, `busy` when a dialog or card shows.
    Step look(bool busy, WallClock now);

private:
    Milliseconds m_quiet;
    Milliseconds m_timeout;
    std::optional<WallClock> m_first;
    std::optional<WallClock> m_last_busy;
};

//  A through tutorial cards and the dialogue boxes of `kinds`
//  (StoryDialogDetector) until none has shown for `quiet`, or `timeout`
//  (QuietPresses: both from the first look, once the presses sent before
//  the call have run).
ScreensPassed press_a_until_quiet(
    ConsoleHandle& console, ProControllerContext& context,
    StoryDialogKinds kinds, Milliseconds quiet, Milliseconds timeout
);

//  From the overworld, presses A at what the player faces. Returns once a
//  dialogue box shows; throws OperationFailedException if none does.
void talk_to_facing(ConsoleHandle& console, ProControllerContext& context);

//  The D-pad presses from choice row `current` to row `target`: down when
//  positive, up when negative.
int dialog_choice_moves(size_t current, size_t target);

//  With a dialogue's choices on screen, picks choice `index` (0 is the top):
//  waits for the selection arrow in dialog_choice_box(), reads the
//  highlighted row with read_dialog_choice_rows() (both in
//  Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h), presses the difference,
//  reads the row again, then A. Throws OperationFailedException if the arrow
//  never shows, no highlighted row reads, or the highlight is not on row
//  `index` after the presses.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
