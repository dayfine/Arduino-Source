/*  MgbaLink Shared Link
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "MgbaLink_SharedLink.h"

namespace PokemonAutomation{
namespace MgbaLink{


LinkRegistry::Publication::Publication(Publication&& x) noexcept
    : m_registry(x.m_registry)
    , m_address(std::move(x.m_address))
    , m_serial(x.m_serial)
{
    x.m_registry = nullptr;
}
LinkRegistry::Publication& LinkRegistry::Publication::operator=(Publication&& x) noexcept{
    if (this != &x){
        reset();
        m_registry = x.m_registry;
        m_address = std::move(x.m_address);
        m_serial = x.m_serial;
        x.m_registry = nullptr;
    }
    return *this;
}
LinkRegistry::Publication::~Publication(){
    reset();
}
void LinkRegistry::Publication::reset() noexcept{
    if (m_registry != nullptr){
        m_registry->unpublish(m_address, m_serial);
        m_registry = nullptr;
    }
}


LinkRegistry::Publication LinkRegistry::publish(std::string address, std::shared_ptr<Connection> connection){
    std::lock_guard<Mutex> lg(m_lock);
    uint64_t serial = ++m_last_serial;
    m_links[address] = Link{std::move(connection), serial};
    return Publication(*this, std::move(address), serial);
}
std::optional<LinkRegistry::Link> LinkRegistry::find(std::string_view address) const{
    std::lock_guard<Mutex> lg(m_lock);
    auto iter = m_links.find(address);
    if (iter == m_links.end()){
        return std::nullopt;
    }
    return iter->second;
}
void LinkRegistry::unpublish(const std::string& address, uint64_t serial) noexcept{
    std::lock_guard<Mutex> lg(m_lock);
    auto iter = m_links.find(address);
    //  A later publish under the same address replaced this one: keep it.
    if (iter != m_links.end() && iter->second.serial == serial){
        m_links.erase(iter);
    }
}


LinkRegistry& link_registry(){
    static LinkRegistry registry;
    return registry;
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation
