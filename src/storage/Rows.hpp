//
// Created by Kirill "Raaveinm" on 9/22/26.
//

#ifndef PICKUSALLBACKEND_ROWS_HPP
#define PICKUSALLBACKEND_ROWS_HPP

/**
 *
 * DTO Private fields
 *
 * @ScalarInt64Row used as unified pass for id's like
 * steam_id / chat_id (every object with @Int64 type)
 *
 */

#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace picasso::storage {

    class ScalarInt64Row : public oatpp::DTO {
        DTO_INIT(ScalarInt64Row, DTO)
        DTO_FIELD(Int64, value);
    };

    class MessageRow : public oatpp::DTO {
        DTO_INIT(MessageRow, DTO)
        DTO_FIELD(Int64, id);
        DTO_FIELD(Int64, conversation_id);
        DTO_FIELD(Int64, sender_steam_id);
        DTO_FIELD(String, client_message_id);
        DTO_FIELD(String, text_message);
        DTO_FIELD(Int64, sent_at);
    };

    class DeletedMessageRow : public oatpp::DTO {
        DTO_INIT(DeletedMessageRow, DTO)
        DTO_FIELD(Int64, conversation_id);
        DTO_FIELD(Int64, message_id);
    };

    class ExistsRow : public oatpp::DTO {
        DTO_INIT(ExistsRow, DTO)
        DTO_FIELD(Boolean, is_member);
    };

    class ScalarStringRow : public oatpp::DTO {
        DTO_INIT(ScalarStringRow, DTO)
        DTO_FIELD(String, value);
    };

    class ScalarBoolRow : public oatpp::DTO {
        DTO_INIT(ScalarBoolRow, DTO)
        DTO_FIELD(Boolean, value);
    };

    /* One contacts row, seen from its owner. */
    class ContactRow : public oatpp::DTO {
        DTO_INIT(ContactRow, DTO)
        DTO_FIELD(Int64, other_id);
        DTO_FIELD(String, level);
        DTO_FIELD(Int64, since);
    };

    /*
     * A contact request or a palette invite, reduced to the counterpart and the two
     * timestamps. declined_at is null while pending.
     */
    class PendingRow : public oatpp::DTO {
        DTO_INIT(PendingRow, DTO)
        DTO_FIELD(Int64, other_id);
        DTO_FIELD(Int64, created_at);
        DTO_FIELD(Int64, declined_at);
    };

    class InviteRow : public oatpp::DTO {
        DTO_INIT(InviteRow, DTO)
        DTO_FIELD(Int64, palette_id);
        DTO_FIELD(String, name);
        DTO_FIELD(Int64, inviter_id);
        DTO_FIELD(Int64, created_at);
    };

    /* name is null for a dm (the LEFT JOIN to palette finds nothing). */
    class ConversationRow : public oatpp::DTO {
        DTO_INIT(ConversationRow, DTO)
        DTO_FIELD(Int64, id);
        DTO_FIELD(String, kind);
        DTO_FIELD(String, name);
        DTO_FIELD(Int64, created_at);
    };

    class SessionRow : public oatpp::DTO {
        DTO_INIT(SessionRow, DTO)
        DTO_FIELD(String, token_hash);
        DTO_FIELD(Int64, steam_id);
        DTO_FIELD(Int64, created_at);
        DTO_FIELD(Int64, expires_at);
        DTO_FIELD(Int64, revoked_at);
    };
}



#endif //PICKUSALLBACKEND_ROWS_HPP
