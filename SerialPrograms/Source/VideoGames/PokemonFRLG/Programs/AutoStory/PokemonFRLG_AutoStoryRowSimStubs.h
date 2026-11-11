/*  AutoStory Row Sim Stubs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story simulations (PokemonFRLG_AutoStoryRowSim.h) bind to the
 *  actions they cannot play, and how a failed step ends their run.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRowSimStubs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRowSimStubs_H

#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The steps of one attempt at a row that a simulation did not play.
struct RowStubs{
    std::string row;
    std::vector<std::string> steps;
};

//  The steps a simulation stubs, by row: the stand-in it binds to each
//  action it cannot play.
class SimStubs{
public:
    //  An attempt at `row` begins; its stubs are listed apart from the last.
    void begin_row(std::string row);
    //  Needs the console; adds the step to the row's stubs, then does
    //  `then` (what the simulation keeps of the step: where pacing leaves
    //  the player), or passes with the pose as it was.
    Pokemon::StoryAction stub(Pokemon::StoryActionFunction then = {});
    const std::vector<RowStubs>& rows() const{
        return m_rows;
    }

private:
    void record(const Pokemon::StoryStep& step);

    std::vector<RowStubs> m_rows;
};

//  Each attempt's stubs, a line a row: "00.03: 2 stubbed: oak, rival".
std::string stub_report(const std::vector<RowStubs>& rows);

//  A failed step fails the run as it threw: a simulation has no save to
//  reload.
class SimStepGuard : public Pokemon::StepGuard{
public:
    virtual bool stops_on_failure(const Pokemon::StoryStep&) const override{
        return true;
    }
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
