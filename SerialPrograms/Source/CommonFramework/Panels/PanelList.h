/*  Panel List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PanelList_H
#define PokemonAutomation_PanelList_H

#include <memory>
#include <vector>
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Panels/PanelTools.h"

class QWidget;

namespace PokemonAutomation{

class PanelListWidget;


class PanelListDescriptor{
    using MakePanelEntries = std::vector<PanelEntry>(*)();

public:
    virtual ~PanelListDescriptor() = default;
    PanelListDescriptor(
        std::string name,
        const std::string& icon_path = "",
        bool enabled = true
    );

    const std::string& name() const{ return m_name; }
    const ImageRGB32& icon() const{ return m_icon; }
    bool enabled() const{ return m_enabled; }

    PanelListWidget* make_QWidget(QWidget& parent, PanelHolder& holder) const;

    //  The entries of the list, as the widget shows them.
    std::vector<PanelEntry> panels() const{ return make_panels(); }

protected:
    virtual std::vector<PanelEntry> make_panels() const = 0;

protected:
    std::string m_name;
    ImageRGB32 m_icon;
    bool m_enabled;
};


//  Instantiate `descriptor` (non-null), load `panel_json` into it, and install
//  it in `holder`. Returns false, loading nothing, if
//  `holder.report_new_panel_intent()` declines.
bool load_panel_session(
    const std::shared_ptr<const PanelDescriptor>& descriptor,
    const JsonValue& panel_json,
    PanelHolder& holder
);



}
#endif
