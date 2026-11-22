/*  Nintendo Switch Controller Descriptor (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/VideoPipeline/Sources/VideoSource_MgbaLink.h"
#include "NintendoSwitch_MgbaLink_Descriptor.h"
#include "NintendoSwitch_MgbaLink_ProController.h"

namespace PokemonAutomation{

template class InterfaceType_t<NintendoSwitch::ControllerDescriptor_MgbaLink>;

namespace NintendoSwitch{


ControllerDescriptor_MgbaLink::ControllerDescriptor_MgbaLink()
    : ControllerDescriptor_MgbaLink(std::string(VideoSourceDescriptor_MgbaLink::DEFAULT_ADDRESS))
{}
ControllerDescriptor_MgbaLink::ControllerDescriptor_MgbaLink(std::string address)
    : UiState<const ControllerDescriptor_MgbaLink, ControllerDescriptor>(INTERFACE_NAME)
    , m_address(std::move(address))
{}

bool ControllerDescriptor_MgbaLink::operator==(const ControllerDescriptor& x) const{
    if (typeid(*this) != typeid(x)){
        return false;
    }
    return m_address == static_cast<const ControllerDescriptor_MgbaLink&>(x).m_address;
}
std::string ControllerDescriptor_MgbaLink::display_name() const{
    return "mGBA (" + m_address + ")";
}

JsonValue ControllerDescriptor_MgbaLink::to_json() const{
    return m_address;
}
void ControllerDescriptor_MgbaLink::load_json(const JsonValue& json){
    const std::string* address = json.to_string();
    if (address != nullptr && parse_mgba_address(*address)){
        m_address = *address;
    }
}

std::unique_ptr<ControllerConnection> ControllerDescriptor_MgbaLink::open_connection(Logger&) const{
    return std::make_unique<ControllerConnection_MgbaLink>(m_address);
}
std::unique_ptr<AbstractController> ControllerDescriptor_MgbaLink::make_controller(
    Logger& logger,
    ControllerConnection& connection,
    ControllerType
) const{
    const std::string& address = static_cast<ControllerConnection_MgbaLink&>(connection).address();
    return std::make_unique<ProController_MgbaLink>(logger, address);
}


ControllerConnection_MgbaLink::ControllerConnection_MgbaLink(std::string address)
    : m_address(std::move(address))
{
    m_controller_list.emplace_back(ControllerType::NintendoSwitch_WiredController);
    m_current_controller = ControllerType::NintendoSwitch_WiredController;
    set_status_line0("mGBA Link: " + m_address, COLOR_BLUE);
    set_status_line1("Sends keys on the mGBA video source's connection to " + m_address + ".");
    declare_ready();
}


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
