//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>

#include "domain/ports/SessionRepository.hpp"

namespace picasso::storage {
    class PgSessionRepository final : public domain::SessionRepository {
    public:
        explicit PgSessionRepository(std::string dsn);

        void store(const domain::Session& session) override;

        std::optional<domain::Session> findByTokenHash(const std::string& tokenHash) override;

        void revoke(const std::string& tokenHash) override;

    private:
        std::string dsn_;
    };
} // namespace picasso::storage
