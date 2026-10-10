//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/Ids.hpp"
#include "domain/Message.hpp"

namespace picasso::domain {
    class ChatRepository {
    public:
        virtual ~ChatRepository() = default;

        struct AppendResult {
            Message message;
            bool duplicate{};               // true: this (conversation, sender, client_message_id) was already stored
        };
        
        virtual AppendResult append(
            const ConversationId& conversation_id,
            SteamId sender,
            const std::string& client_message_id,
            const std::string& body,
            std::int64_t now_epoch_ms) = 0;

        /*
         * The newest `count` non-deleted messages with id > `after` (MessageId(0) = no lower
         * bound), returned ASCENDING. Asking for limit + 1 is how a caller learns whether
         * there were more than `limit`.
         */
        virtual std::vector<Message> newest(
            const ConversationId& conversation_id,
            MessageId after,
            int count) = 0;

        /* The newest `count` non-deleted messages with id < `before`, returned ASCENDING: one scroll-up page. */
        virtual std::vector<Message> olderThan(
            const ConversationId& conversation_id,
            MessageId before,
            int count) = 0;

        enum class DeleteOutcome {
            Deleted,                        // removed now
            AlreadyDeleted,                 // removed earlier - deleting is idempotent
            NotFound,                       // no such message, or it is not the sender's
        };

        /* Soft delete: the row stays, with its text emptied. Only the original sender may delete. */
        virtual DeleteOutcome softDelete(
            const ConversationId& conversation_id,
            MessageId message_id,
            SteamId sender,
            std::int64_t now_epoch_ms) = 0;

        struct DeletedMessage {
            ConversationId conversation_id;
            MessageId message_id;
        };

        /* Messages deleted after `since_epoch_ms` in any conversation `member` belongs to. */
        virtual std::vector<DeletedMessage> deletedSince(SteamId member, std::int64_t since_epoch_ms) = 0;
    };
} // namespace picasso::domain
