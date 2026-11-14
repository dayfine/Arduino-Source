/*  PLZA AutoStory - Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's detectors as the shared screen events (Pokemon_ScreenEvents.h),
 *  so the shared judgements (Pokemon_SceneJudges.h) read PLZA frames too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H

#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The shared events from PLZA's detectors:
//  - DIALOG_BOX: a white dialogue box's bottom edge (FlatWhiteDialogDetector's
//    strip, without its advance arrow: the box shows while the text still
//    types), or a blue or teal box;
//  - CHOICE_BOX: the selection arrow among a dialogue's choices;
//  - INTERACT_PROMPT: an A prompt over the scene ("A Talk", "A Take Elevator");
//  - BATTLE_STARTED: the battle HUD's hints at the bottom centre ("ZL: Focus
//    on a Pokemon", "X A: Select Move"), read by their A icon. Story battles
//    show no Run prompt, so RunFromBattleDetector misses them;
//  - PLAYER_CAN_MOVE: the party bar at the bottom left, with no box;
//  - FADE_BLACK / FADE_WHITE: fade_of_screen().
//  Follow-up: the party bar is missed in some rooms (Hotel Z's hall,
//  za2_m3x). The tutorial's "Move / Control Camera" hint is the story
//  backend's CONTROLS_HINT_READING (ControlsHintDetector), not an event here.
class PlzaScreenObserver : public ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;
    //  observe() for a frame that is not a snapshot (a ScreenState's read).
    ScreenEvents read(const ImageViewRGB32& screen);
    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    BlueDialogDetector m_blue_dialog;
    TealDialogDetector m_teal_dialog;
    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
    ButtonDetector m_battle_hint;
    OverworldPartySelectionDetector m_party;
};

//  What a scene mash is after: the overworld back, a choice on screen
//  (pressing nothing that would pick one), or a story battle's HUD.
enum class SceneMashGoal{
    OVERWORLD,
    CHOICE,
    BATTLE,
};

//  What a scene mash does on one frame.
enum class SceneMashAction{
    WAIT,           //  nothing: a fade, or a quiet screen the goal waits out
    PRESS_TEXT,     //  the mash's text button (A or B) at a dialogue box
    PRESS_A,        //  A: the top choice, or a tutorial card / splash (B does not close them)
    PRESS_B,        //  B on a screen with no box: closes the map and Pokedex screens
    DONE,           //  the goal shows
};

//  How a scene mash reads a frame's events (the console loop is
//  mash_scene() in PokemonLZA_AutoStoryActions.h):
//  - the goal's event ends it (OVERWORLD: PLAYER_CAN_MOVE; the caller holds
//    it a second, since the party bar also shows between lines);
//  - a fade waits;
//  - a choice takes its top row with A, except when the goal is the choice;
//  - a box gets the text button;
//  - with no box: BATTLE presses A (cards, the VS splash), a B mash presses
//    B (it closes the screens a talk opens), an A mash waits (A would talk
//    to whoever stands in front).
SceneMashAction scene_mash_action(const ScreenEvents& events, SceneMashGoal goal, bool text_button_is_b);

//  How long a scene step's mash waits for its scene to show. 02.02's street
//  scene shows its first box ~10 s in from the save (console B, 2026-11-10);
//  02.07's first evening holds a ~25 s black screen first.
constexpr Milliseconds SCENE_START_WINDOW(45000);

//  Whether a scene mash's scene has started, from its frames: a box, a
//  choice or a battle. A fade alone does not count: a walk's way into a new
//  area fades too. A run that walked past a scene's trigger (02.02 after
//  the Brigitte battle, console B, 2026-11-13) shows none of them, and its
//  A mash pressed for 2 minutes before it ended somewhere else.
class SceneStart{
public:
    SceneStart(WallClock start, Milliseconds window)
        : m_deadline(start + window)
    {}
    //  Takes the scene as started (a tutorial card).
    void see(){
        m_seen = true;
    }
    //  Takes a frame's `events` at `now`; true once the window has passed
    //  without the scene.
    bool never_started(const ScreenEvents& events, WallClock now);

private:
    WallClock m_deadline;
    bool m_seen = false;
};

//  Registers the adapter's image tests (lab-B frames, 2026-11-08; they live
//  in CommandLineTests, PokemonLZA/AutoStoryScreenEvents/).
void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
