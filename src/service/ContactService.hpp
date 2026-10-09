//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <cstdint>
#include <memory>

#include "domain/Contact.hpp"
#include "domain/Ids.hpp"
#include "domain/ports/ContactRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/EventSink.hpp"

namespace picasso::service {
    /*
     * The contact state machine (chat-sync-contract.md, section 3A). The repository
     * offers atomic primitives; this class decides which one a request maps to and
     * which answers must stay indistinguishable so a block or an unknown steamId is
     * never revealed.
     */
    class ContactService {
    public:
        ContactService(std::shared_ptr<domain::ContactRepository> contacts,
                       std::shared_ptr<domain::ConversationRepository> conversations,
                       std::shared_ptr<domain::EventSink> events = std::make_shared<domain::NullEventSink>());

        ///////////////////////////////////////////////
        /// Queries
        ///////////////////////////////////////////////

        /* The caller's own view: contacts (incl. blocklist), requests both ways, pending palette invites. */
        [[nodiscard]] domain::ContactsSnapshot snapshot(domain::SteamId me) const;

        ///////////////////////////////////////////////
        /// Requests
        ///////////////////////////////////////////////

        enum class RequestOutcome {
            Pending,                            // 202 - also what a silently dropped request answers
            Accepted,                           // 200 - the target already had a request to us, so it resolved into a contact
            AlreadyContacts,                    // 200 - nothing to do
            Self,                               // 422
            UnblockFirst,                       // 409 - the CALLER has blocked the target
            RateLimited,                        // 429
        };

        /*
         * Every case in which the target has blocked the caller, declined recently, or
         * has never logged in answers Pending: nothing observable differs from a request
         * that really is waiting, so the endpoint is not a probe.
         */
        RequestOutcome request(domain::SteamId me, domain::SteamId target) const;

        /* Recipient says yes. False when nothing was pending (expired, withdrawn, never sent). */
        bool accept(domain::SteamId me, domain::SteamId from) const;

        /*
         * One DELETE, two meanings: as the recipient this declines (silently), as the
         * sender it withdraws. Idempotent either way.
         */
        void declineOrWithdraw(domain::SteamId me, domain::SteamId other) const;

        ///////////////////////////////////////////////
        /// Contact rows
        ///////////////////////////////////////////////

        enum class LevelOutcome {
            Ok,
            Self,                               // 422
            NotAContact,                        // 404 - a tier change needs an existing ally/friend row
        };

        /* Ally<->Friend on the caller's own row, or a block (Imposter). */
        LevelOutcome setLevel(domain::SteamId me, domain::SteamId other, domain::ContactLevel level) const;

        /* DELETE /contacts/{id}: a removal if ally/friend, an unblock if imposter, otherwise a no-op. */
        void removeOrUnblock(domain::SteamId me, domain::SteamId other) const;

    private:
        /* Tells both parties what their own row about the other now is (nullopt = stranger). */
        void publishPair(domain::SteamId first, domain::SteamId second) const;

        std::shared_ptr<domain::ContactRepository> contacts_;
        std::shared_ptr<domain::ConversationRepository> conversations_;
        std::shared_ptr<domain::EventSink> events_;
    };
} // namespace picasso::service
