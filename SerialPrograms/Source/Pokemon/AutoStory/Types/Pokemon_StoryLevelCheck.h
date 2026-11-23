/*  Story Level Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one rule for a check that a party member has reached a level, which
 *  every game's level check applies.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryLevelCheck_H
#define PokemonAutomation_Pokemon_StoryLevelCheck_H

namespace PokemonAutomation{
namespace Pokemon{


//  How far below a level check's minimum a member may be and still pass.
//  A minimum is a plan's estimate of what a fight needs, with margin: a
//  member a level or three short wins it too, and stopping the run there
//  costs more than the fight's risk (FRLG 17.04, console B, 2026-11-15:
//  Giovanni's check stopped a Lv49 lead 3 times at "min 50"; the user:
//  49 is fine, stop at 46 or lower).
inline constexpr unsigned LEVEL_CHECK_TOLERANCE = 3;

//  What a level check makes of a member's level.
enum class LevelVerdict{
    //  At least the minimum.
    MET,
    //  Below the minimum by LEVEL_CHECK_TOLERANCE or less: passes, with a
    //  warning.
    WITHIN_TOLERANCE,
    //  Further below: fails.
    BELOW,
};

//  The verdict on a member of level `level` against `minimum`.
inline LevelVerdict level_verdict(unsigned level, unsigned minimum){
    if (level >= minimum){
        return LevelVerdict::MET;
    }
    return level + LEVEL_CHECK_TOLERANCE >= minimum ? LevelVerdict::WITHIN_TOLERANCE : LevelVerdict::BELOW;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
