//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/ChatService.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::service {
    ChatService::ChatService(std::shared_ptr<domain::ChatRepository> chat,
                             std::shared_ptr<domain::ConversationRepository> conversations,
                             std::shared_ptr<domain::PresenceRegistry> presence,
                             std::shared_ptr<domain::SignalTransport> transport)
        : chat_(std::move(chat)),
          conversations_(std::move(conversations)),
          presence_(std::move(presence)),
          transport_(std::move(transport)) {}

    domain::MessageId ChatService::submit(const domain::SteamId,
                                          const domain::ConversationId&,
                                          const std::string&,
                                          const std::string&) {
        /*
         * Shape once implemented (roadmap step 4):
         *   1. conversations_->isMember(conversationId, sender) or reject
         *   2. chat_->append(conversationId, sender, body) -> messageId
         *   3. ack the sender with localMessageId + messageId
         *   4. fan out to presence_->onlineAmong(conversations_->members(...))
         */
        notImplemented("service: ChatService::submit", "4: storage");
    }
} // namespace picasso::service
