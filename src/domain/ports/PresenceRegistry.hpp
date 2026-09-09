//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <vector>

#include "domain/Ids.hpp"

namespace picasso::domain {
    /*
     * Presence is instance-local by construction: the multi-server design has a
     * client holding one connection per server it knows about, so "online" only
     * ever means "online here". No external store is needed for this.
     */
    class PresenceRegistry {
    public:
        virtual ~PresenceRegistry() = default;

        virtual bool isOnline(SteamId steamId) const = 0;

        virtual std::vector<SteamId> onlineAmong(const std::vector<SteamId>& candidates) const = 0;
    };
} // namespace picasso::domain
