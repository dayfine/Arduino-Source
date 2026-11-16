/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's steps before "Confirm Choices": launch the game from HOME, pick
 *  the language, the look and type the name (PokemonLZA_AutoStoryNewGameChoices.h),
 *  and a developer mode that saves each language's intro screens.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The screens a new game (no save) may start on.
enum class NewGameStart{
    HOME,               //  The Switch's HOME, PLZA's tile selected, the game closed.
    LANGUAGE_LIST,
    CONFIRM_CHOICES,    //  The language, look and name chosen already.
};

//  The screen now, read over up to 2 s with no press. Throws UserSetupError,
//  having pressed nothing, if it is none of NewGameStart's.
NewGameStart require_new_game_start(ConsoleHandle& console, ProControllerContext& context);

//  01.00's first step: from HOME (the selected tile launched, never moving
//  HOME's cursor) or the language list, plays the language, the look and
//  the name of `choices` up to "Confirm Choices"; on "Confirm Choices" it
//  presses nothing. Throws UserSetupError before any press if `choices`
//  cannot be played (new_game_problem()) or the keyboard's counter takes
//  fewer characters than the name, OperationFailedException (with a
//  screenshot) if a screen does not show or its highlight does not reach
//  the choice.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);

//  Capture mode's recording of the screens between the launch and the
//  language list: a polled frame is saved when it differs from the last
//  saved one by more than `change` (pixel RMSD), at most one per `every`.
class IntroFrameRecorder{
public:
    IntroFrameRecorder(double change, Milliseconds every)
        : m_change(change)
        , m_every(every)
    {}

    //  Whether to save the frame polled at `at` (since the recording
    //  began), `from_saved` from the last saved one (none: nothing saved
    //  yet). Counts it as saved when so.
    bool see(Milliseconds at, std::optional<double> from_saved);

    //  How many frames were saved.
    size_t saved() const{
        return m_saved;
    }

private:
    double m_change;
    Milliseconds m_every;
    std::optional<Milliseconds> m_last_saved;
    size_t m_saved = 0;
};

//  The file capture mode saves `language`'s screen `screen` to, in the
//  Screenshots folder: "<tag>_<screen>.png".
std::string intro_capture_file(NewGameLanguage language, std::string_view screen);

//  Developer: for each of the 10 languages (Korean too: nothing is typed),
//  closes the game, launches it from HOME and saves each distinct screen
//  up to the language list as "<tag>_NN.png" (NN from 00, in order), then
//  "<tag>_language-list.png"; picks the language and look `look`, saves
//  "<tag>_look-picker.png" and "<tag>_keyboard.png", and goes back to HOME.
//  Nothing is typed or confirmed, so no save is made.
void capture_intro_screens(ConsoleHandle& console, ProControllerContext& context, size_t look);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
