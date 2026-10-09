//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <cstdint>
#include <optional>

#include "domain/Contact.hpp"
#include "domain/Ids.hpp"

namespace picasso::domain {

    class ContactRepository {
    public:
        virtual ~ContactRepository() = default;

        ///////////////////////////////////////////////
        /// Users
        ///////////////////////////////////////////////

        virtual bool userExists(SteamId steam_id) = 0;

        /* Inserts a stub row if missing. Only a contact request may create one - see the contract. */
        virtual void ensureUser(SteamId steam_id) = 0;

        ///////////////////////////////////////////////
        /// Queries
        ///////////////////////////////////////////////

        /* The owner's own row about `other`, or nullopt for "stranger". */
        virtual std::optional<ContactLevel> levelOf(SteamId owner, SteamId other) = 0;

        /*
         * THE rule: both rows exist and both are ally/friend. An imposter row or a
         * missing row on either side fails it. DM access and palette invitations both
         * go through this and nothing else.
         */
        virtual bool canCommunicate(SteamId first, SteamId second) = 0;

        /* Contacts and requests only; ConversationRepository::pendingInvitesOf supplies palette_invites. */
        virtual ContactsSnapshot snapshot(SteamId me) = 0;

        /* A request from -> to, with its state. Declined ones included so the cooldown can be read. */
        struct RequestState {
            std::int64_t created_at_epoch_ms{};
            std::optional<std::int64_t> declined_at_epoch_ms;
        };
        virtual std::optional<RequestState> requestState(SteamId from, SteamId to) = 0;

        /* Pending (not declined) requests `from` has sent, for the outstanding cap. */
        virtual int countPendingFrom(SteamId from) = 0;

        /* Requests `from` has created since `since_epoch_ms`, for the daily cap. */
        virtual int countCreatedSince(SteamId from, std::int64_t since_epoch_ms) = 0;

        ///////////////////////////////////////////////
        /// Transitions
        ///////////////////////////////////////////////

        /* Creates the request, or re-arms an expired/declined one. */
        virtual void putRequest(SteamId from, SteamId to, std::int64_t now_epoch_ms) = 0;

        /*
         * from -> to is pending: deletes it and writes both rows as ally (an existing
         * ally/friend row is left alone, so accepting twice never downgrades a tier).
         * Returns false if there was no pending request.
         */
        virtual bool accept(SteamId from, SteamId to, std::int64_t now_epoch_ms) = 0;

        /* Marks the request declined; the sender is not told. Returns false if none was pending. */
        virtual bool decline(SteamId from, SteamId to, std::int64_t now_epoch_ms) = 0;

        /* Sender takes a request back. Idempotent. */
        virtual void withdraw(SteamId from, SteamId to) = 0;

        /*
         * Deletes both rows (ally/friend only - an imposter row is a block, not a
         * connection), plus pending requests and palette invites between the two.
         * Returns false if there was nothing to remove.
         */
        virtual bool remove(SteamId first, SteamId second) = 0;

        /*
         * Owner's row -> imposter; the OTHER side's row about the owner is deleted, so a
         * block looks exactly like a removal from their side; pending requests and
         * palette invites between the two are deleted.
         */
        virtual void block(SteamId owner, SteamId other, std::int64_t now_epoch_ms) = 0;

        /* Deletes the owner's imposter row. Does not restore ally. Returns false if there was none. */
        virtual bool unblock(SteamId owner, SteamId other) = 0;

        /* Sets the owner's own row between ally and friend; false if there is no ally/friend row to change. */
        virtual bool setTier(SteamId owner, SteamId other, ContactLevel level) = 0;
    };
} // namespace picasso::domain
