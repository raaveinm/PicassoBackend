//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "PicassoDatabaseClient.hpp"
#include "domain/ports/ConversationRepository.hpp"

namespace picasso::storage {
    class PgConversationRepository final : public domain::ConversationRepository {
        const std::string TAG{"CONVERSATION_REPOSITORY"};
    public:
        explicit PgConversationRepository(std::shared_ptr<PicassoDatabaseClient> db)
            : db_(std::move(db)) {
        }

        bool isMember(const domain::ConversationId& conversation_id, domain::SteamId steam_id) override;

        std::vector<domain::SteamId> members(const domain::ConversationId& conversation_id) override;

        std::vector<domain::ConversationId> conversationsOf(domain::SteamId steamId) override;

        DmResult getOrCreateDm(domain::SteamId first, domain::SteamId second, std::int64_t now_epoch_ms) override;

        domain::ConversationId createPalette(
            const std::string& name,
            domain::SteamId creator,
            std::int64_t now_epoch_ms
        ) override;

        std::optional<domain::Conversation> get(const domain::ConversationId& conversation_id) override;

        InviteOutcome invite(
            const domain::ConversationId& palette,
            domain::SteamId inviter,
            domain::SteamId invitee,
            std::int64_t now_epoch_ms
        ) override;

        bool acceptInvite(
            const domain::ConversationId& palette,
            domain::SteamId invitee,
            std::int64_t now_epoch_ms
        ) override;

        bool declineInvite(
            const domain::ConversationId& palette,
            domain::SteamId invitee,
            std::int64_t now_epoch_ms
        ) override;

        std::vector<domain::PaletteInvite> pendingInvitesOf(domain::SteamId invitee) override;

    private:
        /* The unique key on chat is what makes a concurrent get-or-create safe; this is the read half of it. */
        std::optional<domain::ConversationId> findDm(std::uint64_t member_a, std::uint64_t member_b) const;

        std::shared_ptr<PicassoDatabaseClient> db_;
    };
} // namespace picasso::storage
