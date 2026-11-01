/*  Switch Lab Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lab channel's console commands for a Switch program: the Command File
 *  Runner's commands (NintendoSwitch_CommandFileRunner.h), run on the
 *  selected program's console while that program is stopped.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_LabCommands_H
#define PokemonAutomation_NintendoSwitch_LabCommands_H

#include "CommonFramework/Windows/LabChannel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


class SwitchLabCommands : public LabConsoleCommands{
public:
    virtual Kind kind(const std::string& command) const override;
    virtual void run(
        PanelSession& panel, CancellableScope& scope,
        const std::vector<std::string>& words, LabCommandOutput& output
    ) override;
    virtual void keep_awake(PanelSession& panel, CancellableScope& scope) override;
    virtual void help(std::ostream& out) override;
};



}
}
#endif
