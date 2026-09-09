//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgChatRepository.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::storage {
    PgChatRepository::PgChatRepository(std::string dsn) : dsn_(std::move(dsn)) {}

    domain::MessageId PgChatRepository::append(const domain::ConversationId&,
                                               const domain::SteamId,
                                               const std::string&) {
        notImplemented("ChatRepository::append", "4: storage");
    }

    std::vector<domain::Message> PgChatRepository::historyAfter(const domain::ConversationId&,
                                                                const domain::MessageId,
                                                                int) {
        notImplemented("ChatRepository::historyAfter", "4: storage");
    }
} // namespace picasso::storage
