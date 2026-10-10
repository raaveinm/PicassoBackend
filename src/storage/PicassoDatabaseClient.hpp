//
// Created by raaveinm on 9/19/26.
//

// otherwise throws an compilation err
// ReSharper disable CppVariableCanBeMadeConstexpr
#ifndef PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP
#define PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP

#include "oatpp/orm/SchemaMigration.hpp"
#include "oatpp/orm/DbClient.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DbClient)

namespace picasso::storage {
    const std::string DATABASE_NAME = "picasso_database";
    const std::string DATABASE_TAG = "DATABASE_CLIENT";

    ///////////////////////////////////////////////
    /// Migration Client
    ///////////////////////////////////////////////

    class PicassoDatabaseClient : public oatpp::orm::DbClient {
    public:
        explicit PicassoDatabaseClient(const std::shared_ptr<oatpp::orm::Executor>& executor)
            : DbClient(executor)
        {
            oatpp::orm::SchemaMigration migration(executor, DATABASE_NAME);
            migration.addFile(/*ver*/ 1, /*filename*/DATABASE_MIGRATIONS "/0001_init.sql" );
            migration.migrate();

            OATPP_LOGD(DATABASE_TAG, "Database client initialized, schema version: %ld", executor->getSchemaVersion(DATABASE_NAME));
        }

        ///////////////////////////////////////////////
        /// Queries
        ///////////////////////////////////////////////

        QUERY(createUser,
            "INSERT INTO users VALUES (:steam_id) ON CONFLICT (steam_id) DO NOTHING;",
            PARAM(oatpp::Int64, steam_id))

        // ON CONFLICT DO NOTHING + RETURNING: a row comes back only if this call inserted it, which is
        // how the caller tells a first send from a retry of one that is already stored.
        QUERY(insertMessage,
            "INSERT INTO message_data (conversation_id, sender_steam_id, client_message_id, text_message, sent_at) "
            "VALUES (:conversation_id, :sender_steam_id, :client_message_id, :text_message, :sent_at) "
            "ON CONFLICT (conversation_id, sender_steam_id, client_message_id) DO NOTHING "
            "RETURNING id, conversation_id, sender_steam_id, client_message_id, text_message, sent_at;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, sender_steam_id),
            PARAM(oatpp::String, client_message_id),
            PARAM(oatpp::String, text_message),
            PARAM(oatpp::Int64, sent_at))

        // Includes a deleted row: a retry of a message that was deleted since must find it, not insert it again.
        QUERY(selectMessageByClientId,
            "SELECT id, conversation_id, sender_steam_id, client_message_id, text_message, sent_at "
            "FROM message_data "
            "WHERE conversation_id = :conversation_id AND sender_steam_id = :sender_steam_id "
            "  AND client_message_id = :client_message_id;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, sender_steam_id),
            PARAM(oatpp::String, client_message_id))

        // DESC + LIMIT picks the NEWEST rows; the repository reverses them into ascending order.
        QUERY(selectNewestMessages,
            "SELECT id, conversation_id, sender_steam_id, client_message_id, text_message, sent_at "
            "FROM message_data "
            "WHERE conversation_id = :conversation_id AND id > :after_id AND deleted_at IS NULL "
            "ORDER BY id DESC "
            "LIMIT :limit_count;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, after_id),
            PARAM(oatpp::Int32, limit_count))

        QUERY(selectOlderMessages,
            "SELECT id, conversation_id, sender_steam_id, client_message_id, text_message, sent_at "
            "FROM message_data "
            "WHERE conversation_id = :conversation_id AND id < :before_id AND deleted_at IS NULL "
            "ORDER BY id DESC "
            "LIMIT :limit_count;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, before_id),
            PARAM(oatpp::Int32, limit_count))

        // The text is blanked as well as the row marked: a tombstone has no reason to keep what was said.
        QUERY(markMessageDeleted,
            "UPDATE message_data SET deleted_at = :deleted_at, text_message = '' "
            "WHERE id = :message_id AND conversation_id = :conversation_id "
            "  AND sender_steam_id = :sender_steam_id AND deleted_at IS NULL "
            "RETURNING id AS value;",
            PARAM(oatpp::Int64, message_id),
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, sender_steam_id),
            PARAM(oatpp::Int64, deleted_at))

        // Distinguishes "already deleted" (idempotent success) from "not yours / not there" after an UPDATE that matched nothing.
        QUERY(selectDeletedMessageOfSender,
            "SELECT id AS value FROM message_data "
            "WHERE id = :message_id AND conversation_id = :conversation_id "
            "  AND sender_steam_id = :sender_steam_id AND deleted_at IS NOT NULL;",
            PARAM(oatpp::Int64, message_id),
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, sender_steam_id))

        QUERY(selectDeletedSince,
            "SELECT conversation_id, id AS message_id FROM message_data "
            "WHERE deleted_at > :since "
            "  AND conversation_id IN ("
            "    SELECT conversation_id FROM chat WHERE member_a = :member_id OR member_b = :member_id "
            "    UNION "
            "    SELECT palette_id FROM members WHERE user_id = :member_id) "
            "ORDER BY id ASC;",
            PARAM(oatpp::Int64, since),
            PARAM(oatpp::Int64, member_id))

        QUERY(isMember,
            "SELECT EXISTS ("
            "  SELECT 1 FROM chat WHERE conversation_id = :conversation_id "
            "    AND (member_a = :steam_id OR member_b = :steam_id) "
            "  UNION "
            "  SELECT 1 FROM members WHERE palette_id = :conversation_id AND user_id = :steam_id"
            ") AS is_member;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, steam_id))

        QUERY(selectMembers,
            "SELECT member_a AS value FROM chat WHERE conversation_id = :conversation_id "
            "UNION "
            "SELECT member_b AS value FROM chat WHERE conversation_id = :conversation_id "
            "UNION "
            "SELECT user_id AS value FROM members WHERE palette_id = :conversation_id;",
            PARAM(oatpp::Int64, conversation_id))

        QUERY(selectConversationsOf,
            "SELECT conversation_id AS value FROM chat WHERE member_a = :steam_id OR member_b = :steam_id "
            "UNION "
            "SELECT palette_id AS value FROM members WHERE user_id = :steam_id;",
            PARAM(oatpp::Int64, steam_id))

        ///////////////////////////////////////////////
        /// Users
        ///////////////////////////////////////////////

        QUERY(userExists,
            "SELECT EXISTS (SELECT 1 FROM users WHERE steam_id = :steam_id) AS value;",
            PARAM(oatpp::Int64, steam_id))

        ///////////////////////////////////////////////
        /// Contacts
        ///////////////////////////////////////////////

        QUERY(selectContactLevel,
            "SELECT level AS value FROM contacts WHERE owner_id = :owner_id AND other_id = :other_id;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id))

        // Both directions must exist as ally/friend: exactly two matching rows.
        QUERY(selectCanCommunicate,
            "SELECT (SELECT count(*) FROM contacts "
            "  WHERE ((owner_id = :first_id AND other_id = :second_id) "
            "      OR (owner_id = :second_id AND other_id = :first_id)) "
            "    AND level IN ('ally', 'friend')) = 2 AS value;",
            PARAM(oatpp::Int64, first_id),
            PARAM(oatpp::Int64, second_id))

        QUERY(selectContacts,
            "SELECT other_id, level, since FROM contacts WHERE owner_id = :owner_id ORDER BY since DESC;",
            PARAM(oatpp::Int64, owner_id))

        QUERY(selectIncomingRequests,
            "SELECT from_id AS other_id, created_at, declined_at FROM contact_requests "
            "WHERE to_id = :steam_id AND declined_at IS NULL AND created_at > :min_created_at "
            "ORDER BY created_at DESC;",
            PARAM(oatpp::Int64, steam_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(selectOutgoingRequests,
            "SELECT to_id AS other_id, created_at, declined_at FROM contact_requests "
            "WHERE from_id = :steam_id AND declined_at IS NULL AND created_at > :min_created_at "
            "ORDER BY created_at DESC;",
            PARAM(oatpp::Int64, steam_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(selectRequestState,
            "SELECT to_id AS other_id, created_at, declined_at FROM contact_requests "
            "WHERE from_id = :from_id AND to_id = :to_id;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, to_id))

        QUERY(countPendingRequestsFrom,
            "SELECT count(*) AS value FROM contact_requests "
            "WHERE from_id = :from_id AND declined_at IS NULL AND created_at > :min_created_at;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(countRequestsCreatedSince,
            "SELECT count(*) AS value FROM contact_requests WHERE from_id = :from_id AND created_at > :since;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, since))

        // Creates the request, or re-arms a declined/expired one (clears the decline marker).
        QUERY(upsertRequest,
            "INSERT INTO contact_requests (from_id, to_id, created_at) VALUES (:from_id, :to_id, :created_at) "
            "ON CONFLICT (from_id, to_id) DO UPDATE SET created_at = EXCLUDED.created_at, declined_at = NULL;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, to_id),
            PARAM(oatpp::Int64, created_at))

        // RETURNING tells the caller whether a live request was actually there.
        QUERY(deletePendingRequest,
            "DELETE FROM contact_requests "
            "WHERE from_id = :from_id AND to_id = :to_id AND declined_at IS NULL AND created_at > :min_created_at "
            "RETURNING from_id AS value;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, to_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(markRequestDeclined,
            "UPDATE contact_requests SET declined_at = :declined_at "
            "WHERE from_id = :from_id AND to_id = :to_id AND declined_at IS NULL AND created_at > :min_created_at "
            "RETURNING from_id AS value;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, to_id),
            PARAM(oatpp::Int64, declined_at),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(deleteRequest,
            "DELETE FROM contact_requests WHERE from_id = :from_id AND to_id = :to_id;",
            PARAM(oatpp::Int64, from_id),
            PARAM(oatpp::Int64, to_id))

        QUERY(deleteRequestsBetween,
            "DELETE FROM contact_requests "
            "WHERE (from_id = :first_id AND to_id = :second_id) OR (from_id = :second_id AND to_id = :first_id);",
            PARAM(oatpp::Int64, first_id),
            PARAM(oatpp::Int64, second_id))

        // Accepting must never downgrade a tier the row already carries.
        QUERY(insertAllyIfAbsent,
            "INSERT INTO contacts (owner_id, other_id, level, since) VALUES (:owner_id, :other_id, 'ally', :since) "
            "ON CONFLICT (owner_id, other_id) DO NOTHING;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id),
            PARAM(oatpp::Int64, since))

        QUERY(upsertImposter,
            "INSERT INTO contacts (owner_id, other_id, level, since) VALUES (:owner_id, :other_id, 'imposter', :since) "
            "ON CONFLICT (owner_id, other_id) DO UPDATE SET level = 'imposter', since = EXCLUDED.since;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id),
            PARAM(oatpp::Int64, since))

        QUERY(deleteContactRow,
            "DELETE FROM contacts WHERE owner_id = :owner_id AND other_id = :other_id;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id))

        // An imposter row is a block, not a connection: remove() must not touch it.
        QUERY(deleteConnectedRows,
            "DELETE FROM contacts "
            "WHERE ((owner_id = :first_id AND other_id = :second_id) OR (owner_id = :second_id AND other_id = :first_id)) "
            "  AND level IN ('ally', 'friend') "
            "RETURNING owner_id AS value;",
            PARAM(oatpp::Int64, first_id),
            PARAM(oatpp::Int64, second_id))

        QUERY(deleteImposterRow,
            "DELETE FROM contacts WHERE owner_id = :owner_id AND other_id = :other_id AND level = 'imposter' "
            "RETURNING owner_id AS value;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id))

        QUERY(updateTier,
            "UPDATE contacts SET level = :level "
            "WHERE owner_id = :owner_id AND other_id = :other_id AND level IN ('ally', 'friend') "
            "RETURNING owner_id AS value;",
            PARAM(oatpp::Int64, owner_id),
            PARAM(oatpp::Int64, other_id),
            PARAM(oatpp::String, level))

        ///////////////////////////////////////////////
        /// Conversations
        ///////////////////////////////////////////////

        QUERY(selectDmBetween,
            "SELECT conversation_id AS value FROM chat WHERE member_a = :member_a AND member_b = :member_b;",
            PARAM(oatpp::Int64, member_a),
            PARAM(oatpp::Int64, member_b))

        QUERY(insertConversationRow,
            "INSERT INTO conversations (kind, created_at) VALUES (:kind, :created_at) RETURNING id AS value;",
            PARAM(oatpp::String, kind),
            PARAM(oatpp::Int64, created_at))

        QUERY(insertChatRow,
            "INSERT INTO chat (conversation_id, member_a, member_b) VALUES (:conversation_id, :member_a, :member_b);",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, member_a),
            PARAM(oatpp::Int64, member_b))

        QUERY(insertPaletteRow,
            "INSERT INTO palette (conversation_id, name) VALUES (:conversation_id, :name);",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::String, name))

        QUERY(insertMemberRow,
            "INSERT INTO members (palette_id, user_id, joined_at) VALUES (:palette_id, :user_id, :joined_at) "
            "ON CONFLICT (palette_id, user_id) DO NOTHING;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, user_id),
            PARAM(oatpp::Int64, joined_at))

        QUERY(selectConversation,
            "SELECT c.id AS id, c.kind AS kind, p.name AS name, c.created_at AS created_at "
            "FROM conversations c LEFT JOIN palette p ON p.conversation_id = c.id "
            "WHERE c.id = :conversation_id;",
            PARAM(oatpp::Int64, conversation_id))

        ///////////////////////////////////////////////
        /// Palette invitations
        ///////////////////////////////////////////////

        QUERY(selectInviteState,
            "SELECT inviter_id AS other_id, created_at, declined_at FROM palette_invites "
            "WHERE palette_id = :palette_id AND invitee_id = :invitee_id;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, invitee_id))

        QUERY(upsertInvite,
            "INSERT INTO palette_invites (palette_id, invitee_id, inviter_id, created_at) "
            "VALUES (:palette_id, :invitee_id, :inviter_id, :created_at) "
            "ON CONFLICT (palette_id, invitee_id) DO UPDATE "
            "SET inviter_id = EXCLUDED.inviter_id, created_at = EXCLUDED.created_at, declined_at = NULL;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, invitee_id),
            PARAM(oatpp::Int64, inviter_id),
            PARAM(oatpp::Int64, created_at))

        QUERY(deletePendingInvite,
            "DELETE FROM palette_invites "
            "WHERE palette_id = :palette_id AND invitee_id = :invitee_id AND declined_at IS NULL "
            "  AND created_at > :min_created_at "
            "RETURNING invitee_id AS value;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, invitee_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(markInviteDeclined,
            "UPDATE palette_invites SET declined_at = :declined_at "
            "WHERE palette_id = :palette_id AND invitee_id = :invitee_id AND declined_at IS NULL "
            "  AND created_at > :min_created_at "
            "RETURNING invitee_id AS value;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, invitee_id),
            PARAM(oatpp::Int64, declined_at),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(selectPendingInviteesOf,
            "SELECT invitee_id AS value FROM palette_invites "
            "WHERE palette_id = :palette_id AND declined_at IS NULL AND created_at > :min_created_at;",
            PARAM(oatpp::Int64, palette_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(selectPendingInvitesFor,
            "SELECT i.palette_id AS palette_id, p.name AS name, i.inviter_id AS inviter_id, i.created_at AS created_at "
            "FROM palette_invites i JOIN palette p ON p.conversation_id = i.palette_id "
            "WHERE i.invitee_id = :invitee_id AND i.declined_at IS NULL AND i.created_at > :min_created_at "
            "ORDER BY i.created_at DESC;",
            PARAM(oatpp::Int64, invitee_id),
            PARAM(oatpp::Int64, min_created_at))

        QUERY(deleteInvitesBetween,
            "DELETE FROM palette_invites "
            "WHERE (inviter_id = :first_id AND invitee_id = :second_id) "
            "   OR (inviter_id = :second_id AND invitee_id = :first_id);",
            PARAM(oatpp::Int64, first_id),
            PARAM(oatpp::Int64, second_id))

        ///////////////////////////////////////////////
        /// Sessions
        ///////////////////////////////////////////////

        QUERY(insertSession,
            "INSERT INTO sessions (token_hash, steam_id, created_at, expires_at, revoked_at) "
            "VALUES (:token_hash, :steam_id, :created_at, :expires_at, :revoked_at);",
            PARAM(oatpp::String, token_hash),
            PARAM(oatpp::Int64, steam_id),
            PARAM(oatpp::Int64, created_at),
            PARAM(oatpp::Int64, expires_at),
            PARAM(oatpp::Int64, revoked_at))

        QUERY(selectSessionByHash,
            "SELECT token_hash, steam_id, created_at, expires_at, revoked_at "
            "FROM sessions WHERE token_hash = :token_hash;",
            PARAM(oatpp::String, token_hash))

        QUERY(revokeSession,
            "UPDATE sessions SET revoked_at = :revoked_at WHERE token_hash = :token_hash;",
            PARAM(oatpp::String, token_hash),
            PARAM(oatpp::Int64, revoked_at))
    };

#include OATPP_CODEGEN_END(DbClient)
}

#endif //PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP
