//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>

#include "domain/ports/ConversationRepository.hpp"

namespace picasso::storage {
    class PgConversationRepository final : public domain::ConversationRepository {
    public:
        explicit PgConversationRepository(std::string dsn);

        bool isMember(const domain::ConversationId& conversationId, domain::SteamId steamId) override;

        std::vector<domain::SteamId> members(const domain::ConversationId& conversationId) override;

        std::vector<domain::ConversationId> conversationsOf(domain::SteamId steamId) override;

    private:
        std::string dsn_;
    };
} // namespace picasso::storage
