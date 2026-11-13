/*  BDSP AutoStory - Talk-over tally
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a talk-over ended and what it saw: the shared TalkOver judgement with
 *  a count of the looks that showed a box. Pure, with no console: tests
 *  replay looks through it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TalkOverTally_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TalkOverTally_H

#include <cstddef>
#include <string>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_SceneJudges.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How talk_over() ended.
enum class TalkOverEnd{
    QUIET,
    BATTLE,     //  Only with TalkOverWatch::TRAINER.
};

//  TalkOver's judgement, with how many looks showed a box: what was said,
//  in any language, without reading it. Pure: tests replay looks through it.
class TalkOverTally{
public:
    explicit TalkOverTally(Pokemon::TalkOverRule rule = {})
        : m_judge(rule)
    {}
    Pokemon::SceneMove look(const Pokemon::ScreenEvents& events, WallClock now){
        const Pokemon::SceneMove move = m_judge.look(events, now);
        if (move == Pokemon::SceneMove::NEXT){
            m_box_looks++;
        }
        return move;
    }
    size_t box_looks() const{
        return m_box_looks;
    }
    bool warped() const{
        return m_judge.warped();
    }
    std::string describe() const{
        return m_judge.describe();
    }

private:
    Pokemon::TalkOver m_judge;
    size_t m_box_looks = 0;
};

//  How talk_over_seen() ended, and how many of its looks showed a box.
struct TalkOverSeen{
    TalkOverEnd end = TalkOverEnd::QUIET;
    size_t box_looks = 0;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
