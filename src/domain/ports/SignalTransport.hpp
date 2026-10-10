//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>
#include <vector>

#include "domain/Ids.hpp"

namespace picasso::domain {
    /*
     * Frames are opaque text here on purpose: domain/ must not know about oat++
     * DTOs or JSON. Encoding happens in transport/ws/EnvelopeCodec, and the
     * services below only decide who receives what.
     */
    class SignalTransport {
    public:
        virtual ~SignalTransport() = default;

        virtual void sendTo(SteamId steamId, const std::string& frame) = 0;

        virtual void sendToAll(const std::vector<SteamId>& steamIds, const std::string& frame) = 0;

        /**
         * Like sendToAll, but skips one connection. Used when a user sends from one device:
         * their OTHER devices (and everyone else) get the message, the sending one gets an
         * ack instead - delivering it back would show it twice.
         */
        virtual void sendToAllExcept(
            const std::vector<SteamId>& steamIds,
            const std::string& frame,
            ConnectionId except) = 0;
    };
} // namespace picasso::domain
