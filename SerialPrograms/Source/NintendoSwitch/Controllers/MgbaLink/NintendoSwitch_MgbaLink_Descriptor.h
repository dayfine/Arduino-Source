/*  Nintendo Switch Controller Descriptor (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The "TCP: mGBA Link" controller entry: an mGBA instance by "host:port".
 *  It opens no socket of its own; the controller sends on the connection
 *  the mGBA video source with the same address opened. Developer mode only
 *  (fork lab tooling).
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MgbaLink_Descriptor_H
#define PokemonAutomation_NintendoSwitch_MgbaLink_Descriptor_H

#include <string>
#include "Controllers/ControllerConnection.h"
#include "Controllers/ControllerDescriptor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  Selects an mGBA instance by "host:port" (saved in the profile).
class ControllerDescriptor_MgbaLink : public UiState<const ControllerDescriptor_MgbaLink, ControllerDescriptor>{
public:
    static constexpr ControllerInterface INTERFACE_NAME = ControllerInterface::MgbaLink;

public:
    //  The mGBA video source's default address.
    ControllerDescriptor_MgbaLink();
    explicit ControllerDescriptor_MgbaLink(std::string address);

    const std::string& address() const{
        return m_address;
    }

    virtual bool operator==(const ControllerDescriptor& x) const override;
    virtual std::string display_name() const override;

    virtual JsonValue to_json() const override;
    virtual void load_json(const JsonValue& json) override;

    virtual std::unique_ptr<ControllerConnection> open_connection(Logger& logger) const override;
    virtual std::unique_ptr<AbstractController> make_controller(
        Logger& logger,
        ControllerConnection& connection,
        ControllerType controller_type
    ) const override;

private:
    std::string m_address;
};


//  Ready at once: whether mGBA is reachable shows in the controller's
//  is_ready(), which follows the video source's connection.
class ControllerConnection_MgbaLink : public ControllerConnection{
public:
    explicit ControllerConnection_MgbaLink(std::string address);

    const std::string& address() const{
        return m_address;
    }

private:
    const std::string m_address;
};


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
