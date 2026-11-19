/*  Panel Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/StaticGlobals.h"
#include "ML/ML_Panels.h"
#include "NintendoSwitch/NintendoSwitch_Panels.h"
#include "PokemonBDSP/PokemonBDSP_Panels.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Panels.h"
#include "VideoGames/PokemonHome/PokemonHome_Panels.h"
#include "VideoGames/PokemonLA/PokemonLA_Panels.h"
#include "VideoGames/PokemonLGPE/PokemonLGPE_Panels.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Panels.h"
#include "VideoGames/PokemonPokopia/PokemonPokopia_Panels.h"
#include "VideoGames/PokemonRSE/PokemonRSE_Panels.h"
#include "VideoGames/PokemonSV/PokemonSV_Panels.h"
#include "VideoGames/PokemonSwSh/PokemonSwSh_Panels.h"
#include "VideoGames/ZeldaTotK/ZeldaTotK_Panels.h"
#include "PanelCatalog.h"

namespace PokemonAutomation{


PanelCatalog::PanelCatalog(){
    add(std::make_unique<NintendoSwitch::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonHome::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLGPE::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonSwSh::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonBDSP::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLA::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonSV::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLZA::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonFRLG::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonPokopia::PanelListFactory>());
    if (STATIC_GLOBALS.DEVELOPER_MODE){
        add(std::make_unique<NintendoSwitch::PokemonRSE::PanelListFactory>());
    }

    add(std::make_unique<NintendoSwitch::ZeldaTotK::PanelListFactory>());

    if (STATIC_GLOBALS.DEVELOPER_MODE){
        add(std::make_unique<ML::PanelListFactory>());
    }
}

PanelCatalog::PanelCatalog(std::vector<std::unique_ptr<PanelListDescriptor>> lists){
    for (std::unique_ptr<PanelListDescriptor>& list : lists){
        add(std::move(list));
    }
}

void PanelCatalog::add(std::unique_ptr<PanelListDescriptor> list){
    const size_t index = m_lists.size();
    m_lists.emplace_back(std::move(list));
    const PanelListDescriptor& back = *m_lists.back();
    if (!m_tab_map.emplace(back.name(), index).second){
        const std::string name = back.name();
        m_lists.pop_back();
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Duplicate Category Name: " + name);
    }
}

std::optional<size_t> PanelCatalog::find_category(std::string_view name) const{
    auto iter = m_tab_map.find(name);
    if (iter == m_tab_map.end()){
        return std::nullopt;
    }
    return iter->second;
}

std::optional<PanelCatalogMatch> PanelCatalog::find_panel(std::string_view identifier) const{
    for (size_t c = 0; c < m_lists.size(); c++){
        for (PanelEntry& entry : m_lists[c]->panels()){
            if (entry.descriptor == nullptr || entry.descriptor->identifier() != identifier){
                continue;
            }
            return PanelCatalogMatch{
                .category_index = c,
                .display_name = std::move(entry.display_name),
                .descriptor = std::shared_ptr<const PanelDescriptor>(std::move(entry.descriptor)),
            };
        }
    }
    return std::nullopt;
}


} // namespace PokemonAutomation
