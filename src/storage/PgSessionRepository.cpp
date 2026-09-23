//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgSessionRepository.hpp"

#include <stdexcept>

#include "domain/Clock.hpp"
#include "storage/Rows.hpp"


namespace picasso::storage {
    void PgSessionRepository::store(const domain::Session& session) {
        const oatpp::Int64 revokedAt = session.revoke_at_epoch_ms
            ? oatpp::Int64(*session.revoke_at_epoch_ms)
            : oatpp::Int64();

        const auto result = db_->insertSession(
            oatpp::String(session.token_hash),
            oatpp::Int64(static_cast<v_int64>(session.steam_id.value())),
            oatpp::Int64(session.created_at_epoch_ms),
            oatpp::Int64(session.expires_at_epoch_ms),
            revokedAt);

        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgSessionRepository::store: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgSessionRepository::store: " + *result->getErrorMessage());
        }
    }

    std::optional<domain::Session> PgSessionRepository::findByTokenHash(const std::string& tokenHash) {
        const auto result = db_->selectSessionByHash(oatpp::String(tokenHash));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgSessionRepository::findByTokenHash: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgSessionRepository::findByTokenHash: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<SessionRow>>>();

        if (rows->empty()) { return std::nullopt; }

        const auto& row = rows->at(0);
        domain::Session session;
        session.token_hash = *row->token_hash;
        session.steam_id = domain::SteamId(static_cast<std::uint64_t>(*row->steam_id));
        session.created_at_epoch_ms = *row->created_at;
        session.expires_at_epoch_ms = *row->expires_at;

        if (row->revoked_at) { session.revoke_at_epoch_ms = *row->revoked_at; }
        return session;
    }

    void PgSessionRepository::revoke(const std::string& tokenHash) {
        const auto result = db_->revokeSession(oatpp::String(tokenHash), oatpp::Int64(domain::nowEpochMs()));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("pgSessionRepository::revoke: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgSessionRepository::revoke: " + *result->getErrorMessage());
        }
    }
} // namespace picasso::storage
