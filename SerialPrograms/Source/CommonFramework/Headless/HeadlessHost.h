/*  Headless Host
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_HeadlessHost_H
#define PokemonAutomation_HeadlessHost_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "CommonFramework/Panels/PanelTools.h"

namespace PokemonAutomation{


//  Process exit codes returned by --headless --run.
enum class HeadlessExit : int{
    OK = 0,
    PROGRAM_ERROR = 1,
    STARTUP = 3,
    BAD_REQUEST = 4,
    DEVICE = 5,
    SIGNAL_INT = 130,
    SIGNAL_TERM = 143,
};


//  Widgetless PanelHolder for --headless runs. Never calls make_ui_component()
//  or writes persistent settings.
class HeadlessHost final : public PanelHolder{
public:
    PanelSession* current_panel() const{ return m_current_panel.get(); }

    void clear_panel();

    virtual bool report_new_panel_intent(const PanelDescriptor&) override{ return true; }
    virtual void load_panel(
        std::shared_ptr<const PanelDescriptor> descriptor,
        std::unique_ptr<PanelSession> panel
    ) override;
    virtual void on_busy() override{}
    virtual void on_idle() override{}

private:
    std::shared_ptr<const PanelDescriptor> m_current_panel_descriptor;
    std::unique_ptr<PanelSession> m_current_panel;
};


struct HeadlessRunOptions{
    std::string program_id;
    std::optional<JsonValue> options_override = std::nullopt;
    std::optional<JsonValue> base_panel_json = std::nullopt;
    bool handle_signals = true;
};


//  Run one program from `lists` to completion under QCoreApplication::exec().
HeadlessExit run_headless_program(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options
);


} // namespace PokemonAutomation
#endif
