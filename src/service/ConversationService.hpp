//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"
#include "domain/ports/ContactRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/EventSink.hpp"

namespace picasso::service {
    /*
     * Creating conversations and growing palettes. The contact graph is consulted in
     * exactly two places - a dm needs canCommunicate, and so does every palette
     * invitation - and nowhere inside a palette.
     *
     * An unknown steamId is deliberately answered as NotAllowed, same as a stranger or
     * a blocker: a distinct "no such user" would turn this into a who-is-on-this-server probe.
     */
    class ConversationService {
    public:
        ConversationService(
            std::shared_ptr<domain::ConversationRepository> conversations,
            std::shared_ptr<domain::ContactRepository> contacts,
            std::shared_ptr<domain::EventSink> events
            = std::make_shared<domain::NullEventSink>());

        ///////////////////////////////////////////////
        /// Creation
        ///////////////////////////////////////////////

        enum class CreateOutcome {
            Created,                            // 201
            Existing,                           // 200 - a dm that was already there
            NotAllowed,                         // 403 - stranger, removed, blocked or unknown: one answer
            Self,                               // 422
            InvalidName,                        // 422 - empty or too long
            TooManyInvitees,                    // 422
        };

        struct CreateResult {
            CreateOutcome outcome{CreateOutcome::NotAllowed};
            std::optional<domain::Conversation> conversation;
        };

        /* Get-or-create; needs canCommunicate(me, peer). */
        [[nodiscard]] CreateResult createDm(domain::SteamId me, domain::SteamId peer) const;

        /*
         * Atomic from the caller's view: if ANY invitee fails canCommunicate, nothing is
         * created. The creator is the only member at first; everyone invited is pending.
         */
        [[nodiscard]] CreateResult createPalette(domain::SteamId me,
                                                 const std::string& name,
                                                 const std::vector<domain::SteamId>& invitees) const;

        ///////////////////////////////////////////////
        /// Invitations
        ///////////////////////////////////////////////

        enum class InviteOutcome {
            Invited,                            // 201
            Unchanged,                          // 200 - already a member / already invited / declined recently
            NotAMember,                         // 404 - no such palette, or the caller is not in it
            NotAllowed,                         // 403
            Self,                               // 422
        };

        struct InviteResult {
            InviteOutcome outcome{InviteOutcome::NotAMember};
            std::optional<domain::Conversation> conversation;
        };

        /* Any member may invite. Contact levels gate the invitation, never what happens inside the palette. */
        [[nodiscard]] InviteResult invite(domain::SteamId me,
                                          const domain::ConversationId& palette,
                                          domain::SteamId invitee) const;

        /* Invitee joins. nullopt when nothing was pending. */
        [[nodiscard]] std::optional<domain::Conversation> acceptInvite(
            domain::SteamId me,
            const domain::ConversationId& palette) const;

        /* Silent. Idempotent. */
        void declineInvite(domain::SteamId me, const domain::ConversationId& palette) const;

    private:
        std::shared_ptr<domain::ConversationRepository> conversations_;
        std::shared_ptr<domain::ContactRepository> contacts_;
        std::shared_ptr<domain::EventSink> events_;
    };
} // namespace picasso::service
