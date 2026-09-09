//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "domain/Ids.hpp"
#include "domain/ports/ChatRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/PresenceRegistry.hpp"
#include "domain/ports/SignalTransport.hpp"

namespace picasso::service {
    class ChatService {
    public:
        ChatService(std::shared_ptr<domain::ChatRepository> chat,
                    std::shared_ptr<domain::ConversationRepository> conversations,
                    std::shared_ptr<domain::PresenceRegistry> presence,
                    std::shared_ptr<domain::SignalTransport> transport);

        /*
         * `sender` is the whole point of this signature. It is not read from the
         * frame - it comes from the WS session, which had it stamped in after the
         * token check. A caller with no authenticated identity cannot call this.
         *
         * Persists first, then acks, then fans out: the ack must not promise
         * durability the database has not confirmed.
         */
        domain::MessageId submit(domain::SteamId sender,
                                 const domain::ConversationId& conversationId,
                                 const std::string& localMessageId,
                                 const std::string& body);

    private:
        std::shared_ptr<domain::ChatRepository> chat_;
        std::shared_ptr<domain::ConversationRepository> conversations_;
        std::shared_ptr<domain::PresenceRegistry> presence_;
        std::shared_ptr<domain::SignalTransport> transport_;
    };
} // namespace picasso::service
