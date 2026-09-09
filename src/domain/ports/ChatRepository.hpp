//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>
#include <vector>

#include "domain/Ids.hpp"
#include "domain/Message.hpp"

namespace picasso::domain {
    class ChatRepository {
    public:
        virtual ~ChatRepository() = default;


        virtual MessageId append(const ConversationId& conversationId,
                                 SteamId sender,
                                 const std::string& body) = 0;

        /* Backs GET /conversations/{id}/messages?after={id} - the client's cache sync. */
        virtual std::vector<Message> historyAfter(const ConversationId& conversationId,
                                                  MessageId after,
                                                  int limit) = 0;
    };
} // namespace picasso::domain
