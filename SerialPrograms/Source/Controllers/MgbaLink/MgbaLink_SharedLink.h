/*  MgbaLink Shared Link
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The bridge in mGBA holds one client at a time, so the video source and
 *  the controller of one console slot share one Connection. The video
 *  source opens it (and reopens it after a loss) and publishes it here under
 *  its "host:port"; the controller looks it up under the same text.
 *
 *  Qt-free.
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_SharedLink_H
#define PokemonAutomation_Controllers_MgbaLink_SharedLink_H

#include <stdint.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "MgbaLink_Connection.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  Open connections by address. Thread-safe.
class LinkRegistry{
public:
    //  An open connection and which one it is: every publish gets a new
    //  serial, so a user can tell a reconnect from the link it already uses.
    struct Link{
        std::shared_ptr<Connection> connection;
        uint64_t serial = 0;
    };

    //  Keeps a connection listed until it is destroyed.
    class Publication{
    public:
        Publication() = default;
        Publication(Publication&& x) noexcept;
        Publication& operator=(Publication&& x) noexcept;
        Publication(const Publication&) = delete;
        Publication& operator=(const Publication&) = delete;
        ~Publication();

    private:
        friend class LinkRegistry;
        Publication(LinkRegistry& registry, std::string address, uint64_t serial)
            : m_registry(&registry)
            , m_address(std::move(address))
            , m_serial(serial)
        {}
        void reset() noexcept;

        LinkRegistry* m_registry = nullptr;
        std::string m_address;
        uint64_t m_serial = 0;
    };

    //  Lists "connection" under "address", replacing what was listed there.
    [[nodiscard]] Publication publish(std::string address, std::shared_ptr<Connection> connection);

    //  The connection listed under "address", if any.
    std::optional<Link> find(std::string_view address) const;

private:
    void unpublish(const std::string& address, uint64_t serial) noexcept;

    mutable Mutex m_lock;
    uint64_t m_last_serial = 0;
    std::map<std::string, Link, std::less<>> m_links;
};

//  The registry the app's video sources and controllers share.
LinkRegistry& link_registry();


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif
