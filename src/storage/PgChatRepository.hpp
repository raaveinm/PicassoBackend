//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "domain/ports/ChatRepository.hpp"

namespace picasso::storage {
    class PicassoDatabaseClient;

    class PgChatRepository final : public domain::ChatRepository {
        const std::string TAG{"CHAT_REPOSITORY"};
    public:
        explicit PgChatRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        domain::MessageId append(
            const domain::ConversationId& conversationId,
            domain::SteamId sender,
            const std::string& text_message
        ) override;

        std::vector<domain::Message> historyAfter(
            const domain::ConversationId& conversation_id,
            domain::MessageId after,
            int limit
        ) override;

    private:
        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
