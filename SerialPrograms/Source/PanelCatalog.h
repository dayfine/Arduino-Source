/*  Panel Catalog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PanelCatalog_H
#define PokemonAutomation_PanelCatalog_H

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Panels/PanelList.h"

namespace PokemonAutomation{


//  Match returned by PanelCatalog::find_panel().
struct PanelCatalogMatch{
    size_t category_index;
    std::string display_name;
    std::shared_ptr<const PanelDescriptor> descriptor;
};


//  Widget-free catalog of all registered program categories and panels.
class PanelCatalog{
public:
    PanelCatalog();
    explicit PanelCatalog(std::vector<std::unique_ptr<PanelListDescriptor>> lists);

    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists() const{
        return m_lists;
    }

    //  Find category index by category display name.
    std::optional<size_t> find_category(std::string_view name) const;

    //  Scan each category's panels() for `identifier` and convert the matching
    //  entry's unique_ptr into an owning shared_ptr<const PanelDescriptor>.
    std::optional<PanelCatalogMatch> find_panel(std::string_view identifier) const;

private:
    void add(std::unique_ptr<PanelListDescriptor> list);

private:
    std::vector<std::unique_ptr<PanelListDescriptor>> m_lists;
    std::map<std::string, size_t, std::less<>> m_tab_map;
};


} // namespace PokemonAutomation
#endif
