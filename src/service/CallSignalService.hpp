//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "domain/Ids.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/PresenceRegistry.hpp"
#include "domain/ports/SignalTransport.hpp"

namespace picasso::service {
    /*
     * Signaling only - no media ever passes through here. SDP and ICE payloads stay
     * opaque strings the server never parses.
     *
     * "Forward by toSteamId" is not enough on its own: without the membership check
     * below, any authenticated user could push arbitrary payloads at any steamId and
     * the server would be an open relay. Both ends must be in the conversation.
     */
    class CallSignalService {
    public:
        CallSignalService(std::shared_ptr<domain::ConversationRepository> conversations,
                          std::shared_ptr<domain::PresenceRegistry> presence,
                          std::shared_ptr<domain::SignalTransport> transport);

        /* Notifies the online members of the conversation that a call is starting. */
        void invite(domain::SteamId from, const domain::ConversationId& conversationId);

        /*
         * Relays an already-encoded frame to a single peer. `from` is server-known;
         * the frame must already carry it, because the recipient has no other way to
         * tell who sent it and a client-supplied value would be unverified.
         */
        void relayToPeer(domain::SteamId from,
                         const domain::ConversationId& conversationId,
                         domain::SteamId to,
                         const std::string& frame);

    private:
        std::shared_ptr<domain::ConversationRepository> conversations_;
        std::shared_ptr<domain::PresenceRegistry> presence_;
        std::shared_ptr<domain::SignalTransport> transport_;
    };
} // namespace picasso::service
