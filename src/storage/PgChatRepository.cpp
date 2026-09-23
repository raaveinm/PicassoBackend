//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgChatRepository.hpp"

#include "PicassoDatabaseClient.hpp"
#include "Rows.hpp"
#include "domain/Errors.hpp"
#include "domain/Clock.hpp"

namespace picasso::storage {
    domain::MessageId PgChatRepository::append(
        const domain::ConversationId& conversationId,
        const domain::SteamId sender,
        const std::string& text_message
    ) {
        const auto result = db_->insertMessage(
            oatpp::Int64(conversationId.value()),
            oatpp::Int64(static_cast<int64_t>(sender.value())),
            oatpp::String(text_message),
            oatpp::Int64(domain::nowEpochMs())
        );

        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, "PgChatRepository::append() failed");
            throw std::runtime_error("PgChatRepository::append() failed");
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        return domain::MessageId(*rows->at(0)->value);
    }

    std::vector<domain::Message> PgChatRepository::historyAfter(
        const domain::ConversationId& conversation_id,
        const domain::MessageId after,
        const int limit
    ) {
        const auto result = db_->selectMessagesAfter(
            oatpp::Int64(conversation_id.value()),
            oatpp::Int64(after.value()),
            oatpp::Int32(limit)
        );

        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgChatRepository::historyAfter() failed" + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgChatRepository::historyAfter() failed");
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<MessageRow>>>();

        std::vector<domain::Message> messages;
        messages.reserve(rows->size());

        for (const auto& row : *rows) {
            messages.push_back(domain::Message{
                .id = domain::MessageId(*row->id),
                .conversation_id = domain::ConversationId(*row->conversation_id),
                .sender_steam_id = domain::SteamId(*row->sender_steam_id),
                .text_message = *row->text_message,
                .created_at_epoch_ms = *row->sent_at
            });
        }

        return messages;
    }
} // namespace picasso::storage
