//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <vector>

#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"

namespace picasso::domain {
    class ConversationRepository {
    public:
        virtual ~ConversationRepository() = default;

        /* Gate for every chat write and every signaling forward. */
        virtual bool isMember(const ConversationId& conversationId, SteamId steamId) = 0;

        /* Fan-out target list. */
        virtual std::vector<SteamId> members(const ConversationId& conversationId) = 0;

        /*
         * Read once when a WS session opens and cached on it. ICE candidates arrive
         * in bursts and must not cost a query each.
         */
        virtual std::vector<ConversationId> conversationsOf(SteamId steamId) = 0;
    };
} // namespace picasso::domain
