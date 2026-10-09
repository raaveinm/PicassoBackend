//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/Contact.hpp"
#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"

namespace picasso::domain {
    class ConversationRepository {
    public:
        virtual ~ConversationRepository() = default;

        ///////////////////////////////////////////////
        /// Membership
        ///////////////////////////////////////////////

        /* Gate for every chat write and every signaling forward. Pending invitees are NOT members. */
        virtual bool isMember(const ConversationId& conversationId, SteamId steamId) = 0;

        /* Fan-out target list. */
        virtual std::vector<SteamId> members(const ConversationId& conversationId) = 0;

        /*
         * Read once when a WS session opens and cached on it. ICE candidates arrive
         * in bursts and must not cost a query each.
         */
        virtual std::vector<ConversationId> conversationsOf(SteamId steamId) = 0;

        ///////////////////////////////////////////////
        /// Creation
        ///////////////////////////////////////////////

        struct DmResult {
            ConversationId id;
            bool created{};
        };

         // Get-or-create. The pair is canonicalised here (member_a < member_b)
        virtual DmResult getOrCreateDm(SteamId first, SteamId second, std::int64_t now_epoch_ms) = 0;

        virtual ConversationId createPalette(
            const std::string& name,
            SteamId creator,
            std::int64_t now_epoch_ms) = 0;

        virtual std::optional<Conversation> get(const ConversationId& conversationId) = 0;

        ///////////////////////////////////////////////
        /// Palette invitations
        ///////////////////////////////////////////////

        enum class InviteOutcome {
            Created,
            AlreadyMember,
            AlreadyInvited,
            CooldownActive
        };

        virtual InviteOutcome invite(
            const ConversationId& palette,
            SteamId inviter,
            SteamId invitee,
            std::int64_t now_epoch_ms) = 0;

        /* Deletes the invite and inserts the member, atomically. False if there was no pending invite. */
        virtual bool acceptInvite(const ConversationId& palette, SteamId invitee, std::int64_t now_epoch_ms) = 0;

        /* Marks it declined (cooldown marker); the inviter is not told. False if none was pending. */
        virtual bool declineInvite(const ConversationId& palette, SteamId invitee, std::int64_t now_epoch_ms) = 0;

        /* The invitee's own pending invitations, newest first. */
        virtual std::vector<PaletteInvite> pendingInvitesOf(SteamId invitee) = 0;
    };
} // namespace picasso::domain
