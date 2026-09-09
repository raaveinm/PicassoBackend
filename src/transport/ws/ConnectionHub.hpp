//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "domain/Ids.hpp"
#include "domain/ports/PresenceRegistry.hpp"
#include "domain/ports/SignalTransport.hpp"
#include "transport/ws/Outbound.hpp"

namespace picasso::transport::ws {
    /*
     * The instance-local answer to "who is online" and "deliver this to them".
     * Implements both domain ports, so the services never see a socket.
     *
     * One steamId maps to several connections on purpose - a user with the client
     * open on a phone and a desktop is two connections and both should receive.
     */
    class ConnectionHub final : public domain::PresenceRegistry, public domain::SignalTransport {
    public:
        void add(domain::SteamId steamId, const std::shared_ptr<Outbound>& connection);

        void remove(domain::SteamId steamId, const Outbound* connection);

        bool isOnline(domain::SteamId steamId) const override;

        std::vector<domain::SteamId> onlineAmong(const std::vector<domain::SteamId>& candidates) const override;

        void sendTo(domain::SteamId steamId, const std::string& frame) override;

        void sendToAll(const std::vector<domain::SteamId>& steamIds, const std::string& frame) override;

    private:
        /* Collects live connections under a shared lock; sending happens after it is released. */
        std::vector<std::shared_ptr<Outbound>> resolve(domain::SteamId steamId) const;

        mutable std::shared_mutex mutex_;
        /*
         * weak_ptr, not shared_ptr: a session that died without a clean onClose must
         * not be kept alive by the registry that is supposed to forget it.
         */
        std::unordered_map<domain::SteamId, std::vector<std::weak_ptr<Outbound>>> connections_;
    };
} // namespace picasso::transport::ws
