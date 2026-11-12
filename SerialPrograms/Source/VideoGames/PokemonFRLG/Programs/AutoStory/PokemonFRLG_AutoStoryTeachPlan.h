/*  AutoStory Teach Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The TEACH step's plan, its args in the story table, and the decision of
 *  which move to forget.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTeachPlan_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTeachPlan_H

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
    //  The member by its species' slug (e.g. "pidgey"), as its summary
    //  reads: the first slot that holds one is taught, and `member` is
    //  not used. Empty: `member`.
    std::string species = {};
    //  The move to forget when it knows four: by name, or else by its
    //  slot (0-3). Neither: it must have a free slot.
    std::string forget_move = {};
    std::optional<size_t> forget_slot = std::nullopt;
};

//  Whether `move` is an HM's, which the game won't let a Pokemon forget.
bool is_hm_move(std::string_view move);

//  The GAME action of a TEACH step: item WORD (required), the TM or HM,
//  e.g. "hm03"; move WORD (required), e.g. "surf"; slot INTEGER (from 1)
//  or species WORD (one of them required); forget WORD or forget_slot
//  INTEGER (from 1) (optional): the move to forget when it knows four.
inline const std::string TEACH_ACTION = "teach";
//  `plan` as its action's args.
Pokemon::StoryActionArgs teach_args(const TeachPlan& plan);
//  The plan a TEACH_ACTION step's args give; nothing if the item isn't a
//  TM or HM or a slot is out of range.
std::optional<TeachPlan> teach_plan_of(const Pokemon::StoryActionArgs& args);

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

//  The first party slot whose member `reader` reads as `species`; nothing
//  if none does. Stops at the first slot not read (an empty slot ends the
//  party).
std::optional<size_t> find_party_species(const MemberReader& reader, std::string_view species);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
