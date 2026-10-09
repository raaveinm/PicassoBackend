//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include "storage/PgContactRepository.hpp"

#include "domain/Clock.hpp"
#include "storage/QueryCheck.hpp"
#include "storage/Rows.hpp"

namespace picasso::storage {
    namespace {
        oatpp::Int64 sqlId(const domain::SteamId steam_id) {
            return oatpp::Int64(static_cast<v_int64>(steam_id.value())); // NOLINT(*-return-braced-init-list)
        }

        oatpp::Int64 sqlInt(const std::int64_t value) {
            return oatpp::Int64(value); // NOLINT(*-return-braced-init-list)
        }

        domain::SteamId steamIdOf(const v_int64 raw) {
            return domain::SteamId(static_cast<std::uint64_t>(raw));
        }

        /* Requests older than this are treated as gone everywhere, without a cleanup job. */
        std::int64_t oldestLiveRequest(const std::int64_t now_epoch_ms) {
            return now_epoch_ms - domain::CONTACT_REQUEST_TTL_MS;
        }
    } // namespace

    ///////////////////////////////////////////////
    /// Users
    ///////////////////////////////////////////////

    bool PgContactRepository::userExists(const domain::SteamId steam_id) {
        const auto rows = require(
            db_->userExists(sqlId(steam_id)),
            TAG,
            "userExists"
            ) -> fetch<oatpp::Vector<oatpp::Object<ScalarBoolRow>>>();
        return !rows->empty() && *rows->at(0)->value;
    }

    void PgContactRepository::ensureUser(const domain::SteamId steam_id) {
        require(db_->createUser(sqlId(steam_id)), TAG, "ensureUser");
    }

    ///////////////////////////////////////////////
    /// Queries
    ///////////////////////////////////////////////

    std::optional<domain::ContactLevel> PgContactRepository::levelOf(
        const domain::SteamId owner,
        const domain::SteamId other
    ) {
        const auto rows = require(db_->selectContactLevel(sqlId(owner), sqlId(other)), TAG, "levelOf")
            ->fetch<oatpp::Vector<oatpp::Object<ScalarStringRow>>>();
        if (rows->empty()) return std::nullopt;
        return domain::parseContactLevel(*rows->at(0)->value);
    }

    bool PgContactRepository::canCommunicate(const domain::SteamId first, const domain::SteamId second) {
        const auto rows = require(db_->selectCanCommunicate(sqlId(first), sqlId(second)), TAG, "canCommunicate")
            ->fetch<oatpp::Vector<oatpp::Object<ScalarBoolRow>>>();
        return !rows->empty() && *rows->at(0)->value;
    }

    domain::ContactsSnapshot PgContactRepository::snapshot(const domain::SteamId me) {
        domain::ContactsSnapshot snapshot;
        const auto min_created_at = sqlInt(oldestLiveRequest(domain::nowEpochMs()));

        const auto contact_rows = require(db_->selectContacts(sqlId(me)), TAG, "snapshot.contacts")
            ->fetch<oatpp::Vector<oatpp::Object<ContactRow>>>();
        for (const auto& row : *contact_rows) {
            const auto level = domain::parseContactLevel(*row->level);
            if (!level) continue;               // a level this build doesn't know: skip rather than guess
            snapshot.contacts.push_back(domain::Contact{
                .other = steamIdOf(*row->other_id),
                .level = *level,
                .since_epoch_ms = *row->since,
            });
        }

        const auto incoming_rows = require(
            db_->selectIncomingRequests(sqlId(me), min_created_at), TAG, "snapshot.incoming"
        )->fetch<oatpp::Vector<oatpp::Object<PendingRow>>>();
        for (const auto& row : *incoming_rows) {
            snapshot.incoming.push_back(domain::ContactRequest{
                .other = steamIdOf(*row->other_id),
                .created_at_epoch_ms = *row->created_at,
            });
        }

        const auto outgoing_rows = require(
            db_->selectOutgoingRequests(sqlId(me), min_created_at), TAG, "snapshot.outgoing"
        )->fetch<oatpp::Vector<oatpp::Object<PendingRow>>>();
        for (const auto& row : *outgoing_rows) {
            snapshot.outgoing.push_back(domain::ContactRequest{
                .other = steamIdOf(*row->other_id),
                .created_at_epoch_ms = *row->created_at,
            });
        }

        return snapshot;
    }

    std::optional<domain::ContactRepository::RequestState> PgContactRepository::requestState(
        const domain::SteamId from,
        const domain::SteamId to
    ) {
        const auto rows = require(db_->selectRequestState(sqlId(from), sqlId(to)), TAG, "requestState")
            ->fetch<oatpp::Vector<oatpp::Object<PendingRow>>>();
        if (rows->empty()) return std::nullopt;

        const auto& row = rows->at(0);
        RequestState state;
        state.created_at_epoch_ms = *row->created_at;
        if (row->declined_at) { state.declined_at_epoch_ms = *row->declined_at; }
        return state;
    }

    int PgContactRepository::countPendingFrom(const domain::SteamId from) {
        const auto rows = require(
            db_->countPendingRequestsFrom(sqlId(from), sqlInt(oldestLiveRequest(domain::nowEpochMs()))),
            TAG, "countPendingFrom"
        )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        return rows->empty() ? 0 : static_cast<int>(*rows->at(0)->value);
    }

    int PgContactRepository::countCreatedSince(const domain::SteamId from, const std::int64_t since_epoch_ms) {
        const auto rows = require(
            db_->countRequestsCreatedSince(sqlId(from), sqlInt(since_epoch_ms)), TAG, "countCreatedSince"
        )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        return rows->empty() ? 0 : static_cast<int>(*rows->at(0)->value);
    }

    ///////////////////////////////////////////////
    /// Transitions
    ///////////////////////////////////////////////

    void PgContactRepository::putRequest(
        const domain::SteamId from,
        const domain::SteamId to,
        const std::int64_t now_epoch_ms
    ) {
        require(db_->upsertRequest(sqlId(from), sqlId(to), sqlInt(now_epoch_ms)), TAG, "putRequest");
    }

    bool PgContactRepository::accept(
        const domain::SteamId from,
        const domain::SteamId to,
        const std::int64_t now_epoch_ms
    ) {
        auto transaction = db_->beginTransaction();
        const auto connection = transaction.getConnection();

        const auto deleted = require(
            db_->deletePendingRequest(sqlId(from), sqlId(to), sqlInt(oldestLiveRequest(now_epoch_ms)), connection),
            TAG, "accept.deleteRequest"
        );
        if (!returnedRow(deleted)) { return false; }    // nothing pending: the transaction rolls back untouched

        require(db_->insertAllyIfAbsent(sqlId(from), sqlId(to), sqlInt(now_epoch_ms), connection), TAG, "accept.rowFrom");
        require(db_->insertAllyIfAbsent(sqlId(to), sqlId(from), sqlInt(now_epoch_ms), connection), TAG, "accept.rowTo");

        transaction.commit();
        return true;
    }

    bool PgContactRepository::decline(
        const domain::SteamId from,
        const domain::SteamId to,
        const std::int64_t now_epoch_ms
    ) {
        return returnedRow(require(
            db_->markRequestDeclined(
                sqlId(from), sqlId(to), sqlInt(now_epoch_ms), sqlInt(oldestLiveRequest(now_epoch_ms))
            ),
            TAG, "decline"
        ));
    }

    void PgContactRepository::withdraw(const domain::SteamId from, const domain::SteamId to) {
        require(db_->deleteRequest(sqlId(from), sqlId(to)), TAG, "withdraw");
    }

    bool PgContactRepository::remove(const domain::SteamId first, const domain::SteamId second) {
        auto transaction = db_->beginTransaction();
        const auto connection = transaction.getConnection();

        const auto connected = returnedRow(require(
            db_->deleteConnectedRows(sqlId(first), sqlId(second), connection), TAG, "remove.rows"
        ));
        if (!connected) { return false; }

        // The pair no longer satisfies canCommunicate, so anything that depended on it goes too.
        require(db_->deleteRequestsBetween(sqlId(first), sqlId(second), connection), TAG, "remove.requests");
        require(db_->deleteInvitesBetween(sqlId(first), sqlId(second), connection), TAG, "remove.invites");

        transaction.commit();
        return true;
    }

    void PgContactRepository::block(
        const domain::SteamId owner,
        const domain::SteamId other,
        const std::int64_t now_epoch_ms
    ) {
        auto transaction = db_->beginTransaction();
        const auto connection = transaction.getConnection();

        // The FK needs a users row; blocking someone who never logged in here is allowed.
        require(db_->createUser(sqlId(other), connection), TAG, "block.user");
        require(db_->upsertImposter(sqlId(owner), sqlId(other), sqlInt(now_epoch_ms), connection), TAG, "block.row");
        // The other side's row goes too, so from their side a block is indistinguishable from a removal.
        require(db_->deleteContactRow(sqlId(other), sqlId(owner), connection), TAG, "block.otherRow");
        require(db_->deleteRequestsBetween(sqlId(owner), sqlId(other), connection), TAG, "block.requests");
        require(db_->deleteInvitesBetween(sqlId(owner), sqlId(other), connection), TAG, "block.invites");

        transaction.commit();
    }

    bool PgContactRepository::unblock(const domain::SteamId owner, const domain::SteamId other) {
        return returnedRow(require(db_->deleteImposterRow(sqlId(owner), sqlId(other)), TAG, "unblock"));
    }

    bool PgContactRepository::setTier(
        const domain::SteamId owner,
        const domain::SteamId other,
        const domain::ContactLevel level
    ) {
        if (!domain::allowsCommunication(level)) { return false; }
        return returnedRow(require(
            db_->updateTier(sqlId(owner), sqlId(other), oatpp::String(std::string(domain::toWireString(level)))),
            TAG, "setTier"
        ));
    }
} // namespace picasso::storage
