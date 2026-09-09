//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgConversationRepository.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::storage {
    PgConversationRepository::PgConversationRepository(std::string dsn) : dsn_(std::move(dsn)) {}

    bool PgConversationRepository::isMember(const domain::ConversationId&, const domain::SteamId) {
        notImplemented("ConversationRepository::isMember", "4: storage");
    }

    std::vector<domain::SteamId> PgConversationRepository::members(const domain::ConversationId&) {
        notImplemented("ConversationRepository::members", "4: storage");
    }

    std::vector<domain::ConversationId> PgConversationRepository::conversationsOf(const domain::SteamId) {
        notImplemented("ConversationRepository::conversationsOf", "4: storage");
    }
} // namespace picasso::storage
