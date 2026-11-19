/*  Panel Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/StaticGlobals.h"
#include "ML/ML_Panels.h"
#include "NintendoSwitch/NintendoSwitch_Panels.h"
#include "VideoGames/PokemonSwSh/PokemonSwSh_Panels.h"
#include "VideoGames/PokemonHome/PokemonHome_Panels.h"
#include "PokemonBDSP/PokemonBDSP_Panels.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Panels.h"
#include "VideoGames/PokemonLA/PokemonLA_Panels.h"
#include "VideoGames/PokemonLGPE/PokemonLGPE_Panels.h"
#include "VideoGames/PokemonRSE/PokemonRSE_Panels.h"
#include "VideoGames/PokemonSV/PokemonSV_Panels.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Panels.h"
#include "VideoGames/PokemonPokopia/PokemonPokopia_Panels.h"
#include "VideoGames/ZeldaTotK/ZeldaTotK_Panels.h"
#include "PanelCatalog.h"

namespace PokemonAutomation{


std::vector<std::unique_ptr<PanelListDescriptor>> make_all_panel_lists(){
    std::vector<std::unique_ptr<PanelListDescriptor>> ret;
    ret.emplace_back(std::make_unique<NintendoSwitch::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonHome::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonLGPE::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonSwSh::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonBDSP::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonLA::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonSV::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonLZA::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonFRLG::PanelListFactory>());
    ret.emplace_back(std::make_unique<NintendoSwitch::PokemonPokopia::PanelListFactory>());
    if (STATIC_GLOBALS.DEVELOPER_MODE){
        ret.emplace_back(std::make_unique<NintendoSwitch::PokemonRSE::PanelListFactory>());
    }

    ret.emplace_back(std::make_unique<NintendoSwitch::ZeldaTotK::PanelListFactory>());

    if (STATIC_GLOBALS.DEVELOPER_MODE){
        ret.emplace_back(std::make_unique<ML::PanelListFactory>());
    }
    return ret;
}

std::optional<PanelMatch> find_panel(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    std::string_view identifier
){
    for (size_t c = 0; c < lists.size(); c++){
        for (PanelEntry& entry : lists[c]->panels()){
            if (entry.descriptor == nullptr || entry.descriptor->identifier() != identifier){
                continue;
            }
            return PanelMatch{.category_index = c, .entry = std::move(entry)};
        }
    }
    return std::nullopt;
}


} // namespace PokemonAutomation
