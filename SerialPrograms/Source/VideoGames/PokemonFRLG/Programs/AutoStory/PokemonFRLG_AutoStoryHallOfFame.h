/*  AutoStory Hall of Fame
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The step after the Champion's win: through the talk to the Hall of
 *  Fame, and screenshots of the team there. pokefirered's hall_of_fame.c:
 *
 *  - Task_Hof_InitTeamSaveData prints "Saving... Don't turn off the
 *    power." and saves (HallOfFameSavingDetector, as EliteFourFarmer
 *    waits for it).
 *  - Task_Hof_DisplayMon slides each party member in to its place
 *    (SpriteCB_GetOnScreen, 15 px a frame) and holds it 120 frames (2 s)
 *    with its number, name and level; the ones before it stay, faded.
 *  - Task_Hof_PaletteFadeAndPrintWelcomeText: the whole team at full
 *    color and "Welcome to the HALL OF FAME!"; confetti falls for 400
 *    frames (6.7 s).
 *  - The player's picture holds 120 frames, slides right, and the
 *    LEAGUE CHAMP card waits for A (Task_Hof_ExitOnKeyPressed); A rolls
 *    the credits.
 *
 *  So a still screen for a second is a member's hold, the first run of
 *  motion that lasts seconds is the confetti, whose first frame is the
 *  whole team, and the long still after it is the card.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryHallOfFame_H
#define PokemonAutomation_PokemonFRLG_AutoStoryHallOfFame_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step after the Champion's win: B through the talk until the
//  Hall of Fame saves, then its screenshots (HallOfFameShots) in
//  Screenshots/AutoStory/hall_of_fame/, each path logged. It ends on the
//  LEAGUE CHAMP card, before the credits. No args.
inline const std::string HALL_OF_FAME_ACTION = "hall_of_fame";

//  A screenshot of the Hall of Fame.
enum class HallOfFameShot{
    //  Not one to keep.
    NONE,
    //  A party member on its own, held with its name.
    MON,
    //  The whole team, as the confetti starts.
    TEAM,
    //  The LEAGUE CHAMP card, which waits for A.
    CHAMPION,
};
//  "mon", "team", "champion": the shot's part of its file name.
std::string_view hall_of_fame_shot_name(HallOfFameShot shot);

//  Which looks at the Hall of Fame to keep, from whether each look, one
//  every HALL_OF_FAME_LOOK (in the .cpp), differs from the one before.
class HallOfFameShots{
public:
    //  One look: the shot it is. TEAM is the frame that started this run of
    //  changed looks (run_started()), which the caller kept.
    HallOfFameShot see(bool changed);
    //  The last look started a run of changed looks: the caller keeps its
    //  frame, the TEAM shot if the run turns out to be the confetti.
    bool run_started() const{
        return m_run_started;
    }
    //  The CHAMPION card was seen: the scene is over.
    bool done() const{
        return m_done;
    }

private:
    size_t m_still = 0;
    size_t m_moving = 0;
    //  The still run going on was shot.
    bool m_hold_shot = false;
    bool m_confetti = false;
    bool m_run_started = false;
    bool m_done = false;
};

//  HALL_OF_FAME_ACTION's spec.
Pokemon::StoryActionSpec hall_of_fame_action_spec();

//  On the console, a HALL_OF_FAME_ACTION step; nothing for another. Fails
//  if the save message doesn't show; the screenshots are logged, and one
//  that can't be taken doesn't fail the step.
std::optional<Pokemon::Outcome> run_hall_of_fame_step(
    ConsoleHandle& console, ProControllerContext& context, const Pokemon::StoryStep& step
);


//  Register the Hall of Fame shot tests.
void add_tests_AutoStoryHallOfFame(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
