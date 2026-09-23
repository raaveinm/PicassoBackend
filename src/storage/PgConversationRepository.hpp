//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "PicassoDatabaseClient.hpp"
#include "domain/ports/ConversationRepository.hpp"

namespace picasso::storage {
    class PgConversationRepository final : public domain::ConversationRepository {
        const std::string TAG{"CONVERSATION_REPOSITORY"};
    public:
        explicit PgConversationRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        bool isMember(const domain::ConversationId& conversation_id, domain::SteamId steam_id) override;

        std::vector<domain::SteamId> members(const domain::ConversationId& conversation_id) override;

        std::vector<domain::ConversationId> conversationsOf(domain::SteamId steamId) override;

    private:
        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
