/*  Panel Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PanelCatalog_H
#define PokemonAutomation_PanelCatalog_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include "CommonFramework/Panels/PanelList.h"

namespace PokemonAutomation{


//  Every program category, in dropdown order.
std::vector<std::unique_ptr<PanelListDescriptor>> make_all_panel_lists();


//  A panel found by find_panel(), with the index of its list in `lists`.
struct PanelMatch{
    size_t category_index;
    PanelEntry entry;
};

//  Scans each list's panels() for the panel with `identifier`.
std::optional<PanelMatch> find_panel(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    std::string_view identifier
);


} // namespace PokemonAutomation
#endif
