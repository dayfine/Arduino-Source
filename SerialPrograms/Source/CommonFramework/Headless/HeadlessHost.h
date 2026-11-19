/*  Headless Host
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_HeadlessHost_H
#define PokemonAutomation_HeadlessHost_H

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Panels/PanelList.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "CommonFramework/Panels/PanelTools.h"

namespace PokemonAutomation{


//  Widgetless PanelHolder for --headless runs. Never calls make_ui_component()
//  or writes persistent settings.
class HeadlessHost final : public PanelHolder{
public:
    PanelSession* current_panel() const{ return m_current_panel.get(); }

    void clear_panel();

    virtual bool report_new_panel_intent(const PanelDescriptor& descriptor) override;
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


//  Parameters for a single --headless --run execution.
struct HeadlessRunOptions{
    std::string program_id;
    std::optional<std::string> options_path = std::nullopt;
    std::chrono::milliseconds readiness_timeout = std::chrono::seconds(10);
    std::chrono::milliseconds stop_timeout = std::chrono::seconds(5);
};


//  Merge `--options` JSON over `base_panel_json` without mutating it.
//  Rejects non-object JSON or any `ConsoleSetup` key.
std::optional<JsonValue> merge_headless_options(
    const JsonValue& base_panel_json,
    const JsonValue& override_json,
    std::string& error
);


//  Run one program from `lists` to completion under QCoreApplication::exec().
//  Returns the process exit code (0, 1, 4, 5, 130, 143).
int run_headless_program(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options
);


} // namespace PokemonAutomation
#endif
