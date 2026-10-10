//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgChatRepository.hpp"

#include <algorithm>
#include <stdexcept>

#include "PicassoDatabaseClient.hpp"
#include "Rows.hpp"
#include "storage/QueryCheck.hpp"

namespace picasso::storage {
    namespace {
        oatpp::Int64 sqlId(const domain::SteamId steam_id) {
            return oatpp::Int64(static_cast<v_int64>(steam_id.value()));
        }

        domain::Message toMessage(const oatpp::Object<MessageRow>& row) {
            return domain::Message{
                .id = domain::MessageId(*row->id),
                .conversation_id = domain::ConversationId(*row->conversation_id),
                .sender_steam_id = domain::SteamId(static_cast<std::uint64_t>(*row->sender_steam_id)),
                .client_message_id = *row->client_message_id,
                .text_message = *row->text_message,
                .created_at_epoch_ms = *row->sent_at,
            };
        }

        /* The queries return newest-first (that is what LIMIT needs); callers want oldest-first. */
        std::vector<domain::Message> ascending(const oatpp::Vector<oatpp::Object<MessageRow>>& rows) {
            std::vector<domain::Message> messages;
            messages.reserve(rows->size());
            for (const auto& row : *rows) {
                messages.push_back(toMessage(row));
            }
            std::ranges::reverse(messages);
            return messages;
        }
    } // namespace

    domain::ChatRepository::AppendResult PgChatRepository::append(
        const domain::ConversationId& conversation_id,
        const domain::SteamId sender,
        const std::string& client_message_id,
        const std::string& body,
        const std::int64_t now_epoch_ms
    ) {
        const auto inserted = require(
            db_->insertMessage(
                oatpp::Int64(conversation_id.value()),
                sqlId(sender),
                oatpp::String(client_message_id),
                oatpp::String(body),
                oatpp::Int64(now_epoch_ms)
            ),
            TAG,
            "append"
        )->fetch<oatpp::Vector<oatpp::Object<MessageRow>>>();

        if (!inserted->empty()) {
            return AppendResult{.message = toMessage(inserted->at(0)), .duplicate = false};
        }

        // Nothing inserted: hand back what the first attempt stored.
        const auto existing = require(
            db_->selectMessageByClientId(
                oatpp::Int64(conversation_id.value()),
                sqlId(sender),
                oatpp::String(client_message_id)
            ),
            TAG,
            "append.existing"
        )->fetch<oatpp::Vector<oatpp::Object<MessageRow>>>();

        if (existing->empty()) {
            throw std::runtime_error("PgChatRepository::append: insert skipped but no existing row found");
        }
        return AppendResult{.message = toMessage(existing->at(0)), .duplicate = true};
    }

    std::vector<domain::Message> PgChatRepository::newest(
        const domain::ConversationId& conversation_id,
        const domain::MessageId after,
        const int count
    ) {
        return ascending(
            require(
                db_->selectNewestMessages(
                    oatpp::Int64(conversation_id.value()),
                    oatpp::Int64(after.value()),
                    oatpp::Int32(count)
                ),
                TAG,
                "newest"
            )->fetch<oatpp::Vector<oatpp::Object<MessageRow>>>()
        );
    }

    std::vector<domain::Message> PgChatRepository::olderThan(
        const domain::ConversationId& conversation_id,
        const domain::MessageId before,
        const int count
    ) {
        return ascending(
            require(
                db_->selectOlderMessages(
                    oatpp::Int64(conversation_id.value()),
                    oatpp::Int64(before.value()),
                    oatpp::Int32(count)
                ),
                TAG,
                "olderThan"
            )->fetch<oatpp::Vector<oatpp::Object<MessageRow>>>()
        );
    }

    domain::ChatRepository::DeleteOutcome PgChatRepository::softDelete(
        const domain::ConversationId& conversation_id,
        const domain::MessageId message_id,
        const domain::SteamId sender,
        const std::int64_t now_epoch_ms
    ) {
        const bool deleted_now = returnedRow(
            require(
                db_->markMessageDeleted(
                    oatpp::Int64(message_id.value()),
                    oatpp::Int64(conversation_id.value()),
                    sqlId(sender),
                    oatpp::Int64(now_epoch_ms)
                ),
                TAG,
                "softDelete"
            )
        );
        if (deleted_now) {
            return DeleteOutcome::Deleted;
        }

        // Matched nothing: either it was already deleted (success, idempotent) or it is not this sender's.
        const bool deleted_before = returnedRow(
            require(
                db_->selectDeletedMessageOfSender(
                    oatpp::Int64(message_id.value()),
                    oatpp::Int64(conversation_id.value()),
                    sqlId(sender)
                ),
                TAG,
                "softDelete.check"
            )
        );
        return deleted_before ? DeleteOutcome::AlreadyDeleted : DeleteOutcome::NotFound;
    }

    std::vector<domain::ChatRepository::DeletedMessage> PgChatRepository::deletedSince(
        const domain::SteamId member,
        const std::int64_t since_epoch_ms
    ) {
        const auto rows = require(
            db_->selectDeletedSince(oatpp::Int64(since_epoch_ms), sqlId(member)),
            TAG,
            "deletedSince"
        )->fetch<oatpp::Vector<oatpp::Object<DeletedMessageRow>>>();

        std::vector<DeletedMessage> deleted;
        deleted.reserve(rows->size());
        for (const auto& row : *rows) {
            deleted.push_back(DeletedMessage{
                .conversation_id = domain::ConversationId(*row->conversation_id),
                .message_id = domain::MessageId(*row->message_id),
            });
        }
        return deleted;
    }
} // namespace picasso::storage
