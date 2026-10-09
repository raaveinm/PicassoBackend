//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "PicassoDatabaseClient.hpp"
#include "domain/ports/ContactRepository.hpp"

namespace picasso::storage {
    class PgContactRepository final : public domain::ContactRepository {
        const std::string TAG{"CONTACT_REPOSITORY"};
    public:
        explicit PgContactRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        bool userExists(domain::SteamId steam_id) override;

        void ensureUser(domain::SteamId steam_id) override;

        std::optional<domain::ContactLevel> levelOf(domain::SteamId owner, domain::SteamId other) override;

        bool canCommunicate(domain::SteamId first, domain::SteamId second) override;

        domain::ContactsSnapshot snapshot(domain::SteamId me) override;

        std::optional<RequestState> requestState(domain::SteamId from, domain::SteamId to) override;

        int countPendingFrom(domain::SteamId from) override;

        int countCreatedSince(domain::SteamId from, std::int64_t since_epoch_ms) override;

        void putRequest(domain::SteamId from, domain::SteamId to, std::int64_t now_epoch_ms) override;

        bool accept(domain::SteamId from, domain::SteamId to, std::int64_t now_epoch_ms) override;

        bool decline(domain::SteamId from, domain::SteamId to, std::int64_t now_epoch_ms) override;

        void withdraw(domain::SteamId from, domain::SteamId to) override;

        bool remove(domain::SteamId first, domain::SteamId second) override;

        void block(domain::SteamId owner, domain::SteamId other, std::int64_t now_epoch_ms) override;

        bool unblock(domain::SteamId owner, domain::SteamId other) override;

        bool setTier(domain::SteamId owner, domain::SteamId other, domain::ContactLevel level) override;

    private:
        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
