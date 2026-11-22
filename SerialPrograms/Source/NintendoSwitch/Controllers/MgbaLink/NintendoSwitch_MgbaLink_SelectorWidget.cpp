/*  Nintendo Switch Controller Selector (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/Sources/VideoSource_MgbaLink.h"
#include "NintendoSwitch_MgbaLink_SelectorWidget.h"

namespace PokemonAutomation{

template class RegisterUiStateQtWidget<NintendoSwitch::ControllerSelectorWidget_MgbaLink>;

namespace NintendoSwitch{


ControllerSelectorWidget_MgbaLink::ControllerSelectorWidget_MgbaLink(
    ControllerSelectorWidget& parent,
    const ControllerDescriptor* current
)
    : QLineEdit(&parent)
{
    QSizePolicy policy;
    policy.setHorizontalStretch(3);
    this->setSizePolicy(policy);
    this->setPlaceholderText(QString::fromUtf8(
        VideoSourceDescriptor_MgbaLink::DEFAULT_ADDRESS.data(),
        (qsizetype)VideoSourceDescriptor_MgbaLink::DEFAULT_ADDRESS.size()
    ));

    if (current == nullptr || current->interface_type != ControllerInterface::MgbaLink){
        std::shared_ptr<ControllerDescriptor> descriptor =
            parent.session().option().get_descriptor_from_cache(ControllerInterface::MgbaLink);
        if (!descriptor){
            descriptor = std::make_shared<ControllerDescriptor_MgbaLink>();
        }
        parent.session().set_device(descriptor);
    }

    std::shared_ptr<ControllerDescriptor> active = parent.session().descriptor();
    this->setText(QString::fromStdString(static_cast<const ControllerDescriptor_MgbaLink&>(*active).address()));

    connect(
        this, &QLineEdit::editingFinished,
        &parent, [this, &parent](){
            std::string address = this->text().toStdString();
            if (!parse_mgba_address(address)){
                //  Only 127.0.0.1:<port> or localhost:<port>, as the video source.
                return;
            }
            std::shared_ptr<ControllerDescriptor> selected =
                std::make_shared<ControllerDescriptor_MgbaLink>(std::move(address));
            std::shared_ptr<ControllerDescriptor> current = parent.session().descriptor();
            if (*current == *selected){
                return;
            }
            parent.session().set_device(std::move(selected));
        }
    );
}


}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
