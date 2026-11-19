/*  Panel List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "PanelList.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



PanelListDescriptor::PanelListDescriptor(
    std::string name,
    const std::string& icon_path,
    bool enabled
)
    : m_name(std::move(name))
    , m_enabled(enabled)
{
    if (!icon_path.empty()){
        try{
            m_icon = ImageRGB32(icon_path);
        }catch (FileException& e){
            e.log(global_logger_tagged());
        }
    }
}

bool load_panel_session(
    PanelHolder& holder,
    const std::shared_ptr<const PanelDescriptor>& descriptor,
    const JsonValue& panel_json
){
    if (descriptor == nullptr){
        return false;
    }
    if (!holder.report_new_panel_intent(*descriptor)){
        return false;
    }
    std::unique_ptr<PanelSession> panel = descriptor->make_panel();
    panel->load_json(panel_json);
    holder.load_panel(descriptor, std::move(panel));
    return true;
}


} // namespace PokemonAutomation


