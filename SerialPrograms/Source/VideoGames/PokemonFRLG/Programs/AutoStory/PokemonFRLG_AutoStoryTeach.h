/*  AutoStory Teach
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step that teaches a TM or HM from the bag to a party member,
 *  forgetting a move the table names, as pokefirered runs it (tm_case.c,
 *  party_menu.c):
 *
 *  - BAG, KEY ITEMS, TM CASE, the TM or HM, USE: "Booted up a TM/HM. It
 *    contained <MOVE>. Teach <MOVE> to a POKEMON?" YES/NO.
 *  - YES: the party menu, "Teach which POKEMON?", each member ABLE or NOT
 *    ABLE. A on a member that knows the move: "<MON> knows <MOVE>
 *    already."; one NOT ABLE: "<MON> and <MOVE> are not compatible.".
 *  - With fewer than four moves it learns: "<MON> learned <MOVE>!".
 *  - With four: "<MON> is trying to learn <MOVE>. But <MON> can't learn
 *    more than four moves. Delete an older move to make room for <MOVE>?"
 *    YES opens the moves page; A on a move forgets it ("1, 2, and...
 *    Poof!") and it learns. An HM move can't be forgotten: "HM moves
 *    can't be forgotten now." and the moves page again. NO, then "Stop
 *    trying to teach <MOVE>?" YES, leaves it unlearned.
 *  - A TM is used up; an HM is not.
 *
 *  The step reads the member's moves first (MemberReader) and decides
 *  everything before it opens the bag: the move to forget, by name or
 *  slot; a refusal if the member already knows the move, has four moves
 *  and no move to forget is named, or the move named is an HM move. After
 *  it, the moves are read again: the step succeeds only if the new move
 *  shows in the forgotten move's place.
 *
 *  The decisions take their game steps from a TeachActions, so they run in
 *  tests against a fake party and TM Case. On the console the TM Case,
 *  the party menu's prompts and the moves page have no readers yet, and
 *  the summary reader is pending (MemberReader): the console step stops at
 *  its first read.
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
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonFRLG_AutoStoryMember.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a TEACH step teaches.
struct TeachPlan{
    //  The TM or HM's slug, e.g. "hm03", "tm28".
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

//  Whether `move` is an HM's, which the game won't let a Pokemon forget.
bool is_hm_move(std::string_view move);

//  The prefix of a GAME step's variant that names a TEACH step.
inline constexpr std::string_view TEACH_VARIANT_PREFIX = "teach ";

//  `plan` as a variant: "teach hm03 surf slot=1 forget=tail-whip" or
//  "... forget_slot=2", slots counted from 1, no forget when it has room.
std::string teach_variant(const TeachPlan& plan);
std::optional<TeachPlan> parse_teach_variant(std::string_view variant);

Pokemon::StoryStep teach_step(std::string name, std::string why, const TeachPlan& plan);


//  The move slot `plan` forgets for a member that knows `moves`: nothing
//  when it has room. Empty `error` if it can be taught, else why not.
struct TeachDecision{
    std::optional<size_t> forget = std::nullopt;
    std::string error = {};
    //  It knows the move already: nothing to do.
    bool already_known = false;
};
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

enum class ForgetEnd{
    LEARNED,
    //  "HM moves can't be forgotten now.": back on the moves page.
    HM_REFUSED,
    FAILED,
};

//  The game steps a TEACH step takes, injected so its decisions run in
//  tests.
class TeachActions{
public:
    virtual ~TeachActions() = default;

    //  From the overworld: the bag, the TM Case, `item`, USE, YES, to the
    //  party menu. False if the item isn't there or the party didn't show.
    virtual bool open_item(std::string_view item) = 0;
    //  On "Teach which POKEMON?": A on `slot`.
    virtual TeachPrompt choose_member(size_t slot) = 0;
    //  On "Delete an older move?": YES, then A on move `slot` (0-3).
    virtual ForgetEnd forget(size_t slot) = 0;
    //  From any of its prompts: give up teaching (NO, "Stop trying?" YES).
    virtual void stop_teaching() = 0;
    //  Back to the overworld. False if it didn't come back.
    virtual bool leave() = 0;
};

enum class TeachEnd{
    LEARNED,
    ALREADY_KNOWN,
    //  decide_teach() or the game refused it.
    REFUSED,
    //  The member's moves couldn't be read: nothing was pressed.
    UNREAD,
    FAILED,
};
const char* teach_end_name(TeachEnd end);

struct TeachResult{
    TeachEnd end = TeachEnd::FAILED;
    //  The move forgotten, if any.
    std::string forgot = {};
    std::string detail = {};
};

//  From the overworld: reads the member, teaches as `plan` says, back to
//  the overworld, and reads the member again to confirm.
TeachResult run_teach(Logger& logger, TeachActions& actions, const MemberReader& reader, const TeachPlan& plan);

std::string teach_message(const TeachPlan& plan, const TeachResult& result);


//  The console's TEACH steps: the TM Case and the prompts have no readers
//  yet, so each step fails instead of pressing blind. The step reaches them
//  only once a MemberReader reads moves.
class ConsoleTeachActions : public TeachActions{
public:
    explicit ConsoleTeachActions(ConsoleHandle& console);

    virtual bool open_item(std::string_view item) override;
    virtual TeachPrompt choose_member(size_t slot) override;
    virtual ForgetEnd forget(size_t slot) override;
    virtual void stop_teaching() override;
    virtual bool leave() override;

private:
    ConsoleHandle& m_console;
};


void add_tests_AutoStoryTeach(UnitTestDatabase& database);



}
}
}
#endif
