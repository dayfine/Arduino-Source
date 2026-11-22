/*  Nintendo Switch Controller Selector (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The address box of the "TCP: mGBA Link" controller entry.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MgbaLink_SelectorWidget_H
#define PokemonAutomation_NintendoSwitch_MgbaLink_SelectorWidget_H

#include <QLineEdit>
#include "Common/Qt/UiStateQtWidget.h"
#include "Controllers/ControllerDescriptor.h"
#include "Controllers/ControllerSelectorWidget.h"
#include "NintendoSwitch_MgbaLink_Descriptor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  Edits the address; ignores text that is not 127.0.0.1:<port> or
//  localhost:<port>.
class ControllerSelectorWidget_MgbaLink
    : public QLineEdit
    , public UiComponentQtWidget
{
public:
    using ParentState = const ControllerDescriptor_MgbaLink;

    ControllerSelectorWidget_MgbaLink(
        QWidget& parent,
        const ParentState& current
    )
        : ControllerSelectorWidget_MgbaLink(static_cast<ControllerSelectorWidget&>(parent), &current)
    {}
    ControllerSelectorWidget_MgbaLink(
        ControllerSelectorWidget& parent,
        const ControllerDescriptor* current
    );

    virtual QWidget& widget() override{
        return *this;
    }
};


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
