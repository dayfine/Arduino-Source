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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonFRLG_AutoStoryMember.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a TEACH step teaches.
struct TeachPlan{
    //  The TM or HM's slug, e.g. "hm03", "tm28"; or TEACH_TUTOR, the move
    //  tutor the player faces.
    std::string item = {};
    //  The move it holds, e.g. "surf".
    std::string move = {};
    //  The party slot (0-5).
    size_t member = 0;
    //  The move to forget when it knows four: by name, or else by its
    //  slot (0-3). Neither: it must have a free slot.
    std::string forget_move = {};
    std::optional<size_t> forget_slot = std::nullopt;
};

//  TeachPlan::item for a move tutor.
inline constexpr std::string_view TEACH_TUTOR = "tutor";

//  Whether `move` is an HM's, which the game won't let a Pokemon forget.
bool is_hm_move(std::string_view move);

//  The variant of a GAME step that is a TEACH step.
inline constexpr std::string_view TEACH_VARIANT = "teach";
//  Its args: the TM, HM or tutor, the move, the party slot, and the move
//  to forget by name or by slot.
inline constexpr Pokemon::StoryArgSpec TEACH_ARGS[] = {
    {"item", Pokemon::StoryArgType::TEXT, true},
    {"move", Pokemon::StoryArgType::TEXT, true},
    {"slot", Pokemon::StoryArgType::NUMBER, true},
    {"forget", Pokemon::StoryArgType::TEXT, false},
    {"forget_slot", Pokemon::StoryArgType::NUMBER, false},
};

//  `plan` as a TEACH step's args: {"item": "hm03", "move": "surf", "slot":
//  1, "forget": "tail-whip"} or "forget_slot": 2, slots counted from 1, no
//  forget when it has room.
Pokemon::StoryArgs teach_args(const TeachPlan& plan);
//  The plan a TEACH step (teach_step()) runs; nothing if it isn't one.
std::optional<TeachPlan> parse_teach_step(const Pokemon::StoryStep& step);

//  The table's TEACH step, once per save (StoryStep::once_per_save): a
//  retry from the same save skips it when the lead knew the move or there
//  was no TM.
Pokemon::StoryStep teach_step(std::string name, std::string why, const TeachPlan& plan);


//  The move slot `plan` forgets for a member that knows `moves`: nothing
//  when it has room. Empty `error` if it can be taught, else why not.
struct TeachDecision{
    std::optional<size_t> forget = std::nullopt;
    std::string error = {};
    //  It knows the move already: nothing to do.
    bool already_known = false;
};
//  Determines which move slot to overwrite given current moves.
TeachDecision decide_teach(const TeachPlan& plan, const std::vector<std::string>& moves);


//  What A on the member showed.
enum class TeachPrompt{
    LEARNED,
    //  "Delete an older move to make room?"
    DELETE_PROMPT,
    NOT_COMPATIBLE,
    ALREADY_KNOWS,
    FAILED,
};

//  The outcome of attempting to forget a move.
enum class ForgetEnd{
    LEARNED,
    //  "HM moves can't be forgotten now.": back on the moves page.
    HM_REFUSED,
    FAILED,
};

//  What opening the TM or HM from the bag came to.
enum class ItemOpen{
    //  On "Teach which POKEMON?".
    OPENED,
    //  The TM CASE's list was read and hasn't got it.
    NOT_IN_CASE,
    FAILED,
};

//  The game steps a TEACH step takes, injected so its decisions run in
//  tests.
class TeachActions{
public:
    virtual ~TeachActions() = default;

    //  From the overworld: the bag, the TM Case, `item` (which holds
    //  `move`), USE, to the party menu. For TEACH_TUTOR: talk to the
    //  tutor the player faces, YES to its questions, to the party menu.
    virtual ItemOpen open_item(std::string_view item, std::string_view move) = 0;
    //  On "Teach which POKEMON?": A on `slot`.
    virtual TeachPrompt choose_member(size_t slot) = 0;
    //  On "Delete an older move?": YES, then A on move `slot` (0-3).
    virtual ForgetEnd forget(size_t slot) = 0;
    //  From any of its prompts: give up teaching (NO, "Stop trying?" YES).
    virtual void stop_teaching() = 0;
    //  Back to the overworld. False if it didn't come back.
    virtual bool leave() = 0;
    //  Saves the screen as a debug image, named `label`: the step failed.
    virtual void dump_screen(std::string_view label) = 0;
};

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
const char* teach_end_name(TeachEnd end);

//  The summary result of a move teaching operation.
struct TeachResult{
    TeachEnd end = TeachEnd::FAILED;
    //  The move forgotten, if any.
    std::string forgot = {};
    std::string detail = {};
};

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
