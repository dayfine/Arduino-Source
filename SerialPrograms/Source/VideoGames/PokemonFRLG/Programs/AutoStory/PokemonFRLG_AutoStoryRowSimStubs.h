/*  AutoStory Row Sim Stubs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRowSimStubs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRowSimStubs_H

#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


struct RowStubs{
    std::string row = {};
    std::vector<std::string> steps = {};
};

class SimStubs{
public:
    void begin_row(std::string row);
    FrlgStepAction stub(FrlgStepAction then = {});
    const std::vector<RowStubs>& rows() const{
        return m_rows;
    }

private:
    void record(const StoryStep& step);

    std::vector<RowStubs> m_rows;
};

std::string stub_report(const std::vector<RowStubs>& rows);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
