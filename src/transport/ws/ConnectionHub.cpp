//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/ConnectionHub.hpp"

#include <algorithm>
#include <mutex>

namespace picasso::transport::ws {
    void ConnectionHub::add(const domain::SteamId steamId, const std::shared_ptr<Outbound>& connection) {
        const std::unique_lock lock(mutex_);
        connections_[steamId].push_back(connection);
    }

    void ConnectionHub::remove(const domain::SteamId steamId, const Outbound* connection) {
        const std::unique_lock lock(mutex_);

        const auto entry = connections_.find(steamId);
        if (entry == connections_.end()) {
            return;
        }

        auto& sockets = entry->second;
        /* Drops the departing connection and any that expired without a clean close. */
        sockets.erase(std::remove_if(sockets.begin(), sockets.end(),
                                     [connection](const std::weak_ptr<Outbound>& candidate) {
                                         const auto locked = candidate.lock();
                                         return locked == nullptr || locked.get() == connection;
                                     }),
                      sockets.end());

        if (sockets.empty()) {
            connections_.erase(entry);
        }
    }

    bool ConnectionHub::isOnline(const domain::SteamId steamId) const {
        return !resolve(steamId).empty();
    }

    std::vector<domain::SteamId> ConnectionHub::onlineAmong(const std::vector<domain::SteamId>& candidates) const {
        std::vector<domain::SteamId> online;
        online.reserve(candidates.size());

        for (const auto& candidate : candidates) {
            if (isOnline(candidate)) {
                online.push_back(candidate);
            }
        }

        return online;
    }

    void ConnectionHub::sendTo(const domain::SteamId steamId, const std::string& frame) {
        for (const auto& connection : resolve(steamId)) {
            connection->send(frame);
        }
    }

    void ConnectionHub::sendToAll(const std::vector<domain::SteamId>& steamIds, const std::string& frame) {
        for (const auto& steamId : steamIds) {
            sendTo(steamId, frame);
        }
    }

    std::vector<std::shared_ptr<Outbound>> ConnectionHub::resolve(const domain::SteamId steamId) const {
        std::vector<std::shared_ptr<Outbound>> live;

        {
            const std::shared_lock lock(mutex_);

            const auto entry = connections_.find(steamId);
            if (entry == connections_.end()) {
                return live;
            }

            live.reserve(entry->second.size());
            for (const auto& candidate : entry->second) {
                if (auto locked = candidate.lock()) {
                    live.push_back(std::move(locked));
                }
            }
        }

        /*
         * The lock is released before the caller sends. Writing to a socket under the
         * hub lock would serialise every fan-out in the process behind the slowest
         * consumer, and a send that blocks would hold the registry hostage.
         */
        return live;
    }
} // namespace picasso::transport::ws
