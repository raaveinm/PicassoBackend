//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgSessionRepository.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::storage {
    PgSessionRepository::PgSessionRepository(std::string dsn) : dsn_(std::move(dsn)) {}

    void PgSessionRepository::store(const domain::Session&) {
        notImplemented("SessionRepository::store", "4: storage");
    }

    std::optional<domain::Session> PgSessionRepository::findByTokenHash(const std::string&) {
        notImplemented("SessionRepository::findByTokenHash", "4: storage");
    }

    void PgSessionRepository::revoke(const std::string&) {
        notImplemented("SessionRepository::revoke", "4: storage");
    }
} // namespace picasso::storage
