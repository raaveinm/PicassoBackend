//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "PicassoDatabaseClient.hpp"
#include "domain/ports/SessionRepository.hpp"

namespace picasso::storage {
    class PgSessionRepository final : public domain::SessionRepository {
        const std::string TAG{"SESSION_REPOSITORY"};
    public:
        explicit PgSessionRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        void store(const domain::Session& session) override;

        std::optional<domain::Session> findByTokenHash(const std::string& tokenHash) override;

        void revoke(const std::string& tokenHash) override;

    private:
        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
