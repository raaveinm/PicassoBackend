//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "domain/ports/ChatRepository.hpp"

namespace picasso::storage {
    class PicassoDatabaseClient;

    class PgChatRepository final : public domain::ChatRepository {
        const std::string TAG{"CHAT_REPOSITORY"};
    public:
        explicit PgChatRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        AppendResult append(
            const domain::ConversationId& conversation_id,
            domain::SteamId sender,
            const std::string& client_message_id,
            const std::string& body,
            std::int64_t now_epoch_ms
        ) override;

        std::vector<domain::Message> newest(
            const domain::ConversationId& conversation_id,
            domain::MessageId after,
            int count
        ) override;

        std::vector<domain::Message> olderThan(
            const domain::ConversationId& conversation_id,
            domain::MessageId before,
            int count
        ) override;

        DeleteOutcome softDelete(
            const domain::ConversationId& conversation_id,
            domain::MessageId message_id,
            domain::SteamId sender,
            std::int64_t now_epoch_ms
        ) override;

        std::vector<DeletedMessage> deletedSince(domain::SteamId member, std::int64_t since_epoch_ms) override;

    private:
        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
