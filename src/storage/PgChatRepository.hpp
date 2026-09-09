//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>

#include "domain/ports/ChatRepository.hpp"

namespace picasso::storage {
    class PgChatRepository final : public domain::ChatRepository {
    public:
        explicit PgChatRepository(std::string dsn);

        domain::MessageId append(const domain::ConversationId& conversationId,
                                 domain::SteamId sender,
                                 const std::string& body) override;

        std::vector<domain::Message> historyAfter(const domain::ConversationId& conversationId,
                                                  domain::MessageId after,
                                                  int limit) override;

    private:
        std::string dsn_;
    };
} // namespace picasso::storage
