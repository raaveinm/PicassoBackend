//
// Created by raaveinm on 9/19/26.
//

#ifndef PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP
#define PICKUSALLBACKEND_PICASSODATABASECLIENT_HPP

#include "oatpp/orm/SchemaMigration.hpp"
#include "oatpp/orm/DbClient.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DbClient)

namespace picasso::storage {
    constexpr std::string DATABASE_NAME = "picasso_database";
    constexpr std::string TAG = "database_client";

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

            OATPP_LOGD(TAG, "Database client initialized, schema version: %ld", executor->getSchemaVersion(DATABASE_NAME));
        }

        ///////////////////////////////////////////////
        /// Queries
        ///////////////////////////////////////////////

        QUERY(createUser,
            "INSERT INTO users VALUES (:steam_id);",
            PARAM(oatpp::Int64, steam_id))

        QUERY(insertMessage,
            "INSERT INTO message_data (conversation_id, sender_steam_id, text_message, sent_at) "
            "VALUES (:conversation_id, :sender_steam_id, :text_message, :sent_at) "
            "RETURNING id;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, sender_steam_id),
            PARAM(oatpp::String, text_message),
            PARAM(oatpp::Int64, sent_at))

        QUERY(selectMessagesAfter,
            "SELECT id, conversation_id, sender_steam_id, text_message, sent_at "
            "FROM message_data "
            "WHERE conversation_id = :conversation_id AND id > :after_id "
            "ORDER BY id ASC "
            "LIMIT :limit_count;",
            PARAM(oatpp::Int64, conversation_id),
            PARAM(oatpp::Int64, after_id),
            PARAM(oatpp::Int32, limit_count))

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
            "SELECT member_a AS steam_id FROM chat WHERE conversation_id = :conversation_id "
            "UNION "
            "SELECT member_b AS steam_id FROM chat WHERE conversation_id = :conversation_id "
            "UNION "
            "SELECT user_id AS steam_id FROM members WHERE palette_id = :conversation_id;",
            PARAM(oatpp::Int64, conversation_id))

        QUERY(selectConversationsOf,
            "SELECT conversation_id FROM chat WHERE member_a = :steam_id OR member_b = :steam_id "
            "UNION "
            "SELECT palette_id AS conversation_id FROM members WHERE user_id = :steam_id;",
            PARAM(oatpp::Int64, steam_id))

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
