/*  AutoStory Teach
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step that teaches a TM or HM from the bag to a party member,
 *  forgetting a move the table names, as the game runs it (console B,
 *  2026-11-03; the screens are in PokemonFRLG_AutoStoryTeachScreens.h):
 *
 *  - BAG, KEY ITEMS, TM CASE, OPEN, the TM or HM, USE: the party menu,
 *    "Teach which POKEMON?", each member ABLE or NOT ABLE. A on a member
 *    that knows the move says so; one NOT ABLE: "<MON> and <MOVE> are not
 *    compatible.".
 *  - With fewer than four moves it learns, in the TM's scene ("Machine
 *    set!", "<MON> learned <MOVE>!").
 *  - With four: "<MON> wants to learn the move <MOVE>.", "However, <MON>
 *    already knows four moves.", "Should a move be deleted and replaced
 *    with <MOVE>?" YES/NO. YES opens the moves page; A on a move forgets
 *    it and it learns, in the same scene. An HM move can't be
 *    forgotten (pokefirered: "HM moves can't be forgotten now.") and the
 *    moves page shows again. B there, then "Stop trying to teach <MOVE>?"
 *    YES, leaves it unlearned.
 *  - A TM is used up; an HM is not.
 *
 *  A TM or HM the TM CASE hasn't got skips the step, with a log line: the
 *  table can teach a TM a run may not have won yet.
 *
 *  A move tutor teaches the same way from the overworld, the player facing
 *  the tutor (pokefirered data/scripts/move_tutors.inc, e.g.
 *  Route4_EventScript_MegaKickTutor): its offer, YES/NO; "This move can be
 *  learned only once. Is that okay?", YES/NO; "Which POKEMON wants to
 *  learn it?", then the party menu (ChooseMonForMoveTutor) with the TM's
 *  prompts. After it, the party menu closes onto the tutor's last line,
 *  not the TM CASE.
 *
 *  The step reads the member's moves first (MemberReader) and decides
 *  everything before it opens the bag: the move to forget, by name or
 *  slot; a refusal if the member already knows the move, has four moves
 *  and no move to forget is named, or the move named is an HM move. After
 *  it, the moves are read again: the step succeeds only if the new move
 *  shows in the forgotten move's place.
 *
 *  The decisions take their game steps from a TeachActions, so they run in
 *  tests against a fake party and TM Case; on the console they are
 *  ScreenTeachActions (PokemonFRLG_AutoStoryTeachScreens.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTeach_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTeach_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryTeachActions.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How a move teaching sequence concluded.
enum class TeachEnd{
    LEARNED,
    ALREADY_KNOWN,
    //  decide_teach() or the game refused it.
    REFUSED,
    //  The member's moves couldn't be read: nothing was pressed.
    UNREAD,
    //  The TM or HM isn't in the TM CASE: skipped, nothing taught.
    NO_ITEM,
    FAILED,
};
//  The human-readable name of a teach outcome.
std::string_view teach_end_name(TeachEnd end);

//  The summary result of a move teaching operation.
struct TeachResult{
    TeachEnd end = TeachEnd::FAILED;
    //  The move forgotten, if any.
    std::string forgot = {};
    std::string detail = {};
};

//  The first party slot whose member `reader` reads as `species`; nothing
//  if none does. Stops at the first slot not read (an empty slot ends the
//  party).
std::optional<size_t> find_party_species(const MemberReader& reader, std::string_view species);

//  From the overworld: reads the member, teaches as `plan` says, back to
//  the overworld, and reads the member again to confirm.
TeachResult run_teach(Logger& logger, TeachActions& actions, const MemberReader& reader, const TeachPlan& plan);

//  Formats a log message summarizing the move teaching result.
std::string teach_message(const TeachPlan& plan, const TeachResult& result);


//  Register auto-story move teaching unit tests.
void add_tests_AutoStoryTeach(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
