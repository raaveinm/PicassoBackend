//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/Repositories.hpp"

#include "domain/Errors.hpp"
#include "storage/PgChatRepository.hpp"
#include "storage/PgConversationRepository.hpp"
#include "storage/PgSessionRepository.hpp"

namespace picasso::storage {
    Repositories makeRepositories(const std::string& dsn) {
        return Repositories{
            .chat = std::make_shared<PgChatRepository>(dsn),
            .conversations = std::make_shared<PgConversationRepository>(dsn),
            .sessions = std::make_shared<PgSessionRepository>(dsn),
        };
    }

    void migrate(const std::string&) {
        notImplemented("storage::migrate", "4: storage");
    }
} // namespace picasso::storage
