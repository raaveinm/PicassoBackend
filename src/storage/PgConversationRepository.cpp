//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "storage/PgConversationRepository.hpp"

#include <algorithm>
#include <stdexcept>

#include "domain/Clock.hpp"
#include "storage/QueryCheck.hpp"
#include "storage/Rows.hpp"

namespace picasso::storage {

    bool PgConversationRepository::isMember(
        const domain::ConversationId& conversation_id,
        const domain::SteamId steam_id
    ) {
        const auto result = db_->isMember(
            oatpp::Int64(conversation_id.value()),
            oatpp::Int64(static_cast<v_int64>(steam_id.value())));

        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::isMember: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::isMember: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ExistsRow>>>();
        return !rows->empty() && *rows->at(0)->is_member;
    }

    std::vector<domain::SteamId> PgConversationRepository::members(const domain::ConversationId& conversation_id) {
        const auto result = db_->selectMembers(oatpp::Int64(conversation_id.value()));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::members: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::members: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        std::vector<domain::SteamId> steamIds;
        steamIds.reserve(rows->size());
        for (const auto& row : *rows) {
            steamIds.emplace_back(static_cast<std::uint64_t>(*row->value));
        }
        return steamIds;
    }

    std::vector<domain::ConversationId> PgConversationRepository::conversationsOf(const domain::SteamId steamId) {
        const auto result = db_->selectConversationsOf(oatpp::Int64(static_cast<v_int64>(steamId.value())));
        if (!result->isSuccess()) {
            OATPP_LOGE(TAG, ("PgConversationRepository::conversationsOf: " + *result->getErrorMessage()).c_str());
            throw std::runtime_error("PgConversationRepository::conversationsOf: " + *result->getErrorMessage());
        }

        const auto rows = result->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        std::vector<domain::ConversationId> ids;
        ids.reserve(rows->size());
        for (const auto& row : *rows) {
            ids.emplace_back(*row->value);
        }
        return ids;
    }

    ///////////////////////////////////////////////
    /// Creation
    ///////////////////////////////////////////////

    std::optional<domain::ConversationId> PgConversationRepository::findDm(
        const std::uint64_t member_a,
        const std::uint64_t member_b
    ) const
    {
        const auto rows = require(
            db_->selectDmBetween(
                oatpp::Int64(static_cast<v_int64>(member_a)),
                oatpp::Int64(static_cast<v_int64>(member_b))
            ),
            TAG, "findDm"
        )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        if (rows->empty()) return std::nullopt;
        return domain::ConversationId(*rows->at(0)->value);
    }

    domain::ConversationRepository::DmResult PgConversationRepository::getOrCreateDm(
        const domain::SteamId first,
        const domain::SteamId second,
        const std::int64_t now_epoch_ms
    ) {
        const std::uint64_t low = std::min(first.value(), second.value());
        const std::uint64_t high = std::max(first.value(), second.value());

        if (const auto existing = findDm(low, high)) {
            return DmResult{.id = *existing, .created = false};
        }

        try {
            auto transaction = db_->beginTransaction();
            const auto connection = transaction.getConnection();

            const auto created = require(
                db_->insertConversationRow(oatpp::String("dm"), oatpp::Int64(static_cast<v_int64>(now_epoch_ms)), connection),
                TAG, "getOrCreateDm.conversation"
            )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
            const auto conversation_id = domain::ConversationId(*created->at(0)->value);

            require(
                db_->insertChatRow(
                    oatpp::Int64(conversation_id.value()),
                    oatpp::Int64(static_cast<v_int64>(low)),
                    oatpp::Int64(static_cast<v_int64>(high)),
                    connection
                ),
                TAG, "getOrCreateDm.chat"
            );

            transaction.commit();
            return DmResult{.id = conversation_id, .created = true};
        } catch (const std::runtime_error&) {
            if (const auto winner = findDm(low, high)) {
                return DmResult{.id = *winner, .created = false};
            }
            throw;
        }
    }

    domain::ConversationId PgConversationRepository::createPalette(
        const std::string& name,
        const domain::SteamId creator,
        const std::int64_t now_epoch_ms
    ) {
        auto transaction = db_->beginTransaction();
        const auto connection = transaction.getConnection();

        const auto created = require(
            db_->insertConversationRow(oatpp::String("palette"), oatpp::Int64(static_cast<v_int64>(now_epoch_ms)), connection),
            TAG, "createPalette.conversation"
        )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
        const auto conversation_id = domain::ConversationId(*created->at(0)->value);

        require(
            db_->insertPaletteRow(oatpp::Int64(conversation_id.value()), oatpp::String(name), connection),
            TAG, "createPalette.palette"
        );
        require(
            db_->insertMemberRow(
                oatpp::Int64(conversation_id.value()),
                oatpp::Int64(static_cast<v_int64>(creator.value())),
                oatpp::Int64(static_cast<v_int64>(now_epoch_ms)),
                connection
            ),
            TAG, "createPalette.creator"
        );

        transaction.commit();
        return conversation_id;
    }

    std::optional<domain::Conversation> PgConversationRepository::get(const domain::ConversationId& conversation_id) {
        const auto rows = require(
            db_->selectConversation(oatpp::Int64(conversation_id.value())), TAG, "get"
        )->fetch<oatpp::Vector<oatpp::Object<ConversationRow>>>();
        if (rows->empty()) return std::nullopt;

        const auto& row = rows->at(0);
        const auto kind = domain::parseConversationKind(*row->kind);
        if (!kind) {
            throw std::runtime_error("PgConversationRepository::get: unknown kind '" + *row->kind + "'");
        }

        domain::Conversation conversation;
        conversation.id = conversation_id;
        conversation.kind = *kind;
        conversation.created_at_epoch_ms = *row->created_at;
        if (row->name) { conversation.name = *row->name; }

        conversation.members = members(conversation_id);
        std::ranges::sort(conversation.members);

        if (*kind == domain::ConversationKind::Palette) {
            const auto min_created_at = domain::nowEpochMs() - domain::PALETTE_INVITE_TTL_MS;
            const auto invited = require(
                db_->selectPendingInviteesOf(
                    oatpp::Int64(conversation_id.value()),
                    oatpp::Int64(static_cast<v_int64>(min_created_at))
                ),
                TAG, "get.invited"
            )->fetch<oatpp::Vector<oatpp::Object<ScalarInt64Row>>>();
            for (const auto& invitee : *invited) {
                conversation.invited.emplace_back(static_cast<std::uint64_t>(*invitee->value));
            }
            std::ranges::sort(conversation.invited);
        }
        return conversation;
    }

    ///////////////////////////////////////////////
    /// Palette invitations
    ///////////////////////////////////////////////

    domain::ConversationRepository::InviteOutcome PgConversationRepository::invite(
        const domain::ConversationId& palette,
        const domain::SteamId inviter,
        const domain::SteamId invitee,
        const std::int64_t now_epoch_ms
    ) {
        if (isMember(palette, invitee)) { return InviteOutcome::AlreadyMember; }

        const auto rows = require(
            db_->selectInviteState(
                oatpp::Int64(palette.value()),
                oatpp::Int64(static_cast<v_int64>(invitee.value()))
            ),
            TAG, "invite.state"
        )->fetch<oatpp::Vector<oatpp::Object<PendingRow>>>();

        if (!rows->empty()) {
            const auto& state = rows->at(0);
            if (state->declined_at) {
                // Declined recently: drop silently, the inviter must not learn the invitee said no.
                if (now_epoch_ms - *state->declined_at < domain::DECLINE_COOLDOWN_MS) {
                    return InviteOutcome::CooldownActive;
                }
            } else if (now_epoch_ms - *state->created_at < domain::PALETTE_INVITE_TTL_MS) {
                return InviteOutcome::AlreadyInvited;
            }
            // else: a decline past its cooldown, or an expired invite - re-arm below.
        }

        require(
            db_->upsertInvite(
                oatpp::Int64(palette.value()),
                oatpp::Int64(static_cast<v_int64>(invitee.value())),
                oatpp::Int64(static_cast<v_int64>(inviter.value())),
                oatpp::Int64(static_cast<v_int64>(now_epoch_ms))
            ),
            TAG, "invite.upsert"
        );
        return InviteOutcome::Created;
    }

    bool PgConversationRepository::acceptInvite(
        const domain::ConversationId& palette,
        const domain::SteamId invitee,
        const std::int64_t now_epoch_ms
    ) {
        auto transaction = db_->beginTransaction();
        const auto connection = transaction.getConnection();

        const auto deleted = require(
            db_->deletePendingInvite(
                oatpp::Int64(palette.value()),
                oatpp::Int64(static_cast<v_int64>(invitee.value())),
                oatpp::Int64(static_cast<v_int64>(now_epoch_ms - domain::PALETTE_INVITE_TTL_MS)),
                connection
            ),
            TAG, "acceptInvite.delete"
        );
        if (!returnedRow(deleted)) { return false; }    // nothing pending: roll back untouched

        require(
            db_->insertMemberRow(
                oatpp::Int64(palette.value()),
                oatpp::Int64(static_cast<v_int64>(invitee.value())),
                oatpp::Int64(now_epoch_ms),
                connection
            ),
            TAG, "acceptInvite.member"
        );

        transaction.commit();
        return true;
    }

    bool PgConversationRepository::declineInvite(
        const domain::ConversationId& palette,
        const domain::SteamId invitee,
        const std::int64_t now_epoch_ms
    ) {
        return returnedRow(require(
            db_->markInviteDeclined(
                oatpp::Int64(palette.value()),
                oatpp::Int64(static_cast<v_int64>(invitee.value())),
                oatpp::Int64(now_epoch_ms),
                oatpp::Int64(now_epoch_ms - domain::PALETTE_INVITE_TTL_MS)
            ),
            TAG, "declineInvite"
        ));
    }

    std::vector<domain::PaletteInvite> PgConversationRepository::pendingInvitesOf(const domain::SteamId invitee) {
        const auto rows = require(
            db_->selectPendingInvitesFor(
                oatpp::Int64(static_cast<v_int64>(invitee.value())),
                oatpp::Int64(static_cast<v_int64>(domain::nowEpochMs() - domain::PALETTE_INVITE_TTL_MS))
            ),
            TAG, "pendingInvitesOf"
        )->fetch<oatpp::Vector<oatpp::Object<InviteRow>>>();

        std::vector<domain::PaletteInvite> invites;
        invites.reserve(rows->size());
        for (const auto& row : *rows) {
            invites.push_back(domain::PaletteInvite{
                .conversation_id = domain::ConversationId(*row->palette_id),
                .name = *row->name,
                .inviter = domain::SteamId(static_cast<std::uint64_t>(*row->inviter_id)),
                .created_at_epoch_ms = *row->created_at,
            });
        }
        return invites;
    }
} // namespace picasso::storage
